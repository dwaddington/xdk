/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  
*/

/*
  Authors:
  Copyright (C) 2011, Daniel G. Waddington <d.waddington@samsung.com>
  Copyright (C) 2011, Chen Tian
*/

#ifndef __EXO_LOCKFREE_QUEUE__
#define __EXO_LOCKFREE_QUEUE__

#include <assert.h>
#include <new>
#include "atomic.h"

#define DBG_LOCKFREE_Q
//#define USE_GENODE_HEAP_FOR_OBJECTS /* use for testing only. */

#ifdef DBG_LOCKFREE_Q
#define DBG(...) PINF(__VA_ARGS__)
#define CHECK(X)  assert((((unsigned long) X ) & 7UL) == 0)
#else
#define DBG(...)
//#define CHECK(X)
#define CHECK(X)  assert((((unsigned long) X ) & 7UL) == 0)
#endif

/** 
 * Note: What's all this alignment about?  Well, after much sweat and
 * tears, on the Intel 64bit platform (and may be others), apparently 
 * it matters a great deal in this context.  The basic thing is, that on
 * the x86_64 platform the 'mov' instruction (e.g., used in assignments) is 
 * NOT guaranteed to be atomic when the addresses are not 8 byte aligned.
 * So if you are assigning to pointers concurrently, you have to make sure
 * they are aligned.  See Intel System Manual Vol.3 Chap.8
 */
namespace Exokernel {

  namespace Lockfree
  {
    /** 
     * Class for elements of the lock-free queue
     * 
     */
    template <class T> class Node {
    private:
      T data;

    public:
      Node<T> * _next __attribute__((aligned(__SIZEOF_POINTER__)));

    public:

      Node() : _next(NULL) {}
      Node(T val) : data(val), _next(NULL) {}

      virtual ~Node() {}

      T get_data() const {
        return data;
      }

      void set_data(T d) {
        data = d;
      }

      Node<T> * next() const { return _next; }

    } __attribute__((aligned(__SIZEOF_POINTER__)));


    /** 
     * Lock-free queue implementation using the algorithm by Michael & Scott @ Rochester
     * 
     * @param T Node type
     * @param _Allocator Allocator class
     * 
     */
    template <class T, 
              typename _Allocator = class Exokernel::Memory::Typed_stdc_allocator<T> >
    class Queue {
      
    private:
      
      Node<T> *_Q_Head __attribute__((aligned(__SIZEOF_POINTER__)));
      Node<T> *_Q_Tail __attribute__((aligned(__SIZEOF_POINTER__)));

      _Allocator _allocator;

    public:

      Queue() {

        _Q_Head = _Q_Tail = new (_allocator.alloc()) Node<T>;

        DBG("set _Q_Tail to 0x%p", _Q_Tail);

        assert(_Q_Head);
        _Q_Head->_next = NULL;
      }

      // dtor
      virtual ~Queue() {
        while(!empty())
          pop();
      }

      /** 
       * Return the head of the queue
       * 
       * 
       * @return Pointer to the head node
       */
      Node<T> * const head() {
        return _Q_Head;
      }
      
      /** 
       * Return the tail of the queue
       * 
       * 
       * @return Pointer to the tail node
       */
      Node<T> * const tail() {
        return _Q_Tail;
      }

      /** 
       * Push a value onto the queue
       * 
       * @param val value to push
       */
      void push(T val) {

        CHECK(_Q_Tail);
        CHECK(_Q_Head);

        Node<T> *node __attribute__((aligned(__SIZEOF_POINTER__)));

        node = (Node<T> *) new (_allocator.alloc()) Node<T>;

        CHECK(node);
        assert((umword_t) node != 0x7fff00000000);

        DBG("[%p]: push() qtail=%p qtail->_next=%p node=%p", 
            (void *) this, _Q_Tail, _Q_Tail->_next, node);

        if (node == NULL) {
          PERR("Lockfree queue failed to allocate memory");
          assert(0);
        }
        node->set_data(val);
        node->_next = NULL;

        Node<T> *tail __attribute__((aligned(__SIZEOF_POINTER__)));
        Node<T> *next __attribute__((aligned(__SIZEOF_POINTER__)));

        /* we are trying to push 'node' onto the queue */
        while (true) {

          assert(_Q_Tail);
          tail = _Q_Tail;

          assert((umword_t) tail->_next != 0x7fff00000000);
          next = tail->_next;
          assert((umword_t) next != 0x7fff00000000);

          if (tail == _Q_Tail) {
            if (next == NULL) {
              assert(node != next);

              if (Atomic::compare_and_swap((atomic_t*) &(tail->_next),
                                               (atomic_t) next,
                                               (atomic_t) node) == (atomic_t) next) {

                DBG("[%p]: Pushing LFQ item %p\n", (void *) this, (void*) node->get_data());
                break;
              }

            }
            else {

              if (Atomic::compare_and_swap((atomic_t *) & _Q_Tail,
                                               (atomic_t) tail,
                                               (atomic_t) next) != (atomic_t) tail) {
                DBG("[%p] Q_Tail after = %p next=%p\n", (void *) this, _Q_Tail, next);
              }
            }
          }
        }

        if (Atomic::compare_and_swap((atomic_t *) & _Q_Tail, (atomic_t) tail, (atomic_t) node) == (atomic_t) tail)
          DBG("++Q_Tail after = %p\n", _Q_Tail);

        DBG("[%p]: [afterwhile] push() qtail=%p qtail->_next=%p", (void *) this, _Q_Tail, _Q_Tail->_next);
      }

      /** 
       * Remove an element from the queue
       * 
       * 
       * @return removed element
       */
      T pop() {
        T res;
        Node<T> *head __attribute__((aligned(__SIZEOF_POINTER__)));
        Node<T> *tail __attribute__((aligned(__SIZEOF_POINTER__)));
        Node<T> *next __attribute__((aligned(__SIZEOF_POINTER__)));

        CHECK(_Q_Tail);
        CHECK(_Q_Head);
        DBG("[%p]: pop() qtail=%p qtail->_next=%p head=%p head->_next=%p", 
            (void *) this, _Q_Tail, _Q_Tail->_next, _Q_Head, _Q_Head->_next);

        while (true) {

          head = _Q_Head;
          tail = _Q_Tail;
          next = head->_next;
          CHECK(head);
          CHECK(tail);
          CHECK(next);
          if (head == _Q_Head) {
            if (head == tail) {
              if (next == NULL) {
                //                PERR("LFQ underflow");
                return T();
              }

              /* update the tail, but don't worry if its too late
                 and someone has already moved the tail from another
                 pop 
              */
              if (Atomic::compare_and_swap((atomic_t *) & _Q_Tail,
                                               (atomic_t) tail,
                                               (atomic_t) next) == (atomic_t) tail) {
                DBG("[%p]: Succeeded updating tail in empty list next=%p\n", (void *) 0, next);
              }
            }
            else {
              assert(next != NULL);
              res = next->get_data();
              assert(head != next);

              /* if we succeed in this CAS, then we managed to
                 successfully take the element, i.e. the next
                 element we peeked at is still valid.
              */
              if (Atomic::compare_and_swap((atomic_t *) & _Q_Head,
                                               (atomic_t) head,
                                               (atomic_t) next) == (atomic_t) head) {
                DBG("[%p]: Popping LFQ item %p\n", (void *) 0, (void*) &res);
                break;
              }
            }
          }
        }

        DBG("[%p]: [afterwhile] pop() qtail=%p qtail->_next=%p head=%p head->_next=%p", (void *) 0, _Q_Tail, _Q_Tail->_next, head, head->_next);

        /* free memory for element that has been popped */
        delete head;

        return res;
      }

      inline bool empty() {
        return (_Q_Head == _Q_Tail);
      }

      size_t const unsafe_length() {
        size_t s = 0;
        Node<T> * p = head();
        
        while(p != tail()) {
          s++;
          p = p->_next;
        }
        return s;
      }
      
    };
  }

}


#undef DBG
#undef DBG_LOCKFREE_Q
#endif
