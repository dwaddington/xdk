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
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __EXO_LIST_H__
#define __EXO_LIST_H__

namespace Exokernel
{
  /** An element of a doubly-linked list. */
  template <typename T>
  class List_element  {
  public:
    List_element<T>* _next; //<! The next element.
    List_element<T>* _prev; //<! The previous element.

    List_element() : _next(NULL), _prev(NULL) { }

    T* next() { return (T*) _next; } 
    T* prev() { return (T*) _prev; } 

    List_element<T>* next_e() { return _next; }
    List_element<T>* prev_e() { return _prev; }
    
    // void set_next(T* p) {
    //   assert(p != this); // disallow self-references
    //   _next = (List_element<T>*) p; 
    // }

    // void set_prev(T* p) {
    //   assert(p != this); // disallow self-references
    //   _prev = (List_element<T>*) p; 
    // }

    void set_next(List_element<T>* e) {
      assert(e != this); // disallow self-references
      _next = e; 
    }

    void set_prev(List_element<T>* e) {
      assert(e != this); // disallow self-references
      _prev = e; 
    }

  };


  /** A doubly-linked list. */
  template <typename T>
  class List {
  private:
    List_element<T>* _head;
    List_element<T>* _tail;

  public:
    /** Constructor. */
    List() : _head(NULL), _tail(NULL) { }

    /** Returns true if the list is empty and false otherwise.*/
    bool empty() { return _head == NULL; }

    /** Inserts an element at the end of list. */
    void insert(List_element<T>* e) {
      assert(e);
      if (this->empty()) {
        e->set_prev(NULL);
        e->set_next(NULL); 
        _head = _tail = e;
      }
      else {
        e->set_next(NULL); 
        e->set_prev(_tail); 
        _tail->set_next(e); 
        _tail = e;
      }
    }

    /** Inserts an element at the front of the list. */
    void insert_front(List_element<T>* e) {
      assert(e);
      if (this->empty()) {
        e->set_prev(NULL);
        e->set_next(NULL); 
        _head = _tail = e;
      }
      else {
        e->set_prev(NULL); 
        e->set_next(_head); 
        _head->set_prev(e); 
        _head = e;
      }
    }

    /** Remove an element from list (does not free memory). */
    void remove(List_element<T>* e) {
      assert(e);

      if (this->empty()) return; 

      if (e == _head) {
        _head = _head->next();     
        if (_head != NULL) {
          _head->set_prev(NULL); 
        }
        e->set_next(NULL);
      }
      else if (e == _tail) {
        _tail = _tail->prev();
        if (_tail != NULL) {
          _tail->set_next(NULL);
        }
        e->set_prev(NULL); 
      }
      else if (this->contains(e)) {
        // The element is in the middle of the list.
        e->prev_e()->set_next(e->next_e());  
        e->next_e()->set_prev(e->prev_e());  

        e->set_prev(NULL);
        e->set_next(NULL);
      }
      else {
        panic("Tried to remove an element that does not exist in list!\n");
      }

    }

    /** 
     * Checks if the list contains an element at specified address.
     * @param e a pointer to the element.  
     * @return true if the list contains the element; false otherwise.
     */
    bool contains(List_element<T>* e) {
      assert(e);
      List_element<T>* cur = _head;
      while (cur != NULL) {
        if (cur == e) {
          return true;
        }
        cur = cur->next_e();
      }
      return false;
    }


    /** 
     * Pops a member from the front of the list.
     * @return  popped off element.
     */
    T* pop_front() {
      if (this->empty()) return NULL; 

      List_element<T>* oh = _head; 

      _head = _head->next_e();     
      if (_head != NULL) {
        _head->set_prev(NULL); 
      }

      oh->set_next(NULL);

      return static_cast<T*>(oh);
    }


    /** 
     * Fetches an element from the list given an index (counting from 0) 
     * without removing the element from the list. 
     * @param index the index of element to fetch.
     * @return a pointer to element
     */
    T* get(unsigned index) {
      if (this->empty()) return NULL; 

      List_element<T>* p = _head; 
      while (index > 0) {
         p = p->_next;
         if (p == NULL) {
          return NULL;
         }
         index--;
       }
      return static_cast<T*>(p);
    }

    /** 
     * Returns the head of the list without removing it.
     * @return the head of the list.
     */
    T* head() {
      return static_cast<T*>(_head);
    }

    /** 
     * Returns the tail the list without removing it.
     * @return the head of the list.
     */
    T* tail() {
      return static_cast<T*>(_tail);
    }
  
    /** Returns the length of the list. */
    size_t length() {
      size_t s = 0;
      List_element<T>* p = _head;
      while (p != NULL) {
        p = p->_next;
        s++;
      }
      return s;
    }

    /** []operator returns NULL on OOB */
    T& operator[](int index) {
      return *(this->get(index));
    }

    /** Dumps the contest of the list. */
    void dump() {
      List_element<T>* p = _head; 
      if (!p) printf("NULL\n");
      while (p) {
        printf("[%p]->", p);
        p = p->_next;
      } 
      printf("\n");
    }

 
    /** Join list 'l' to the end of this list */
    // void append(List<T> * l) {
    //   assert(l);
    //   insert(l->head());
    // }
  };


  /** A stack. */
  template <typename T>
  class Stack : public List<T> {

  public:
    /** Pushes an element into the stack. */
    void push(List_element<T> *e) {
      insert(e);
    }

    /** Pops an element from the stack. */
    List_element<T> * pop() {    
      List_element<T> * r = List<T>::tail();
      remove(r);
      return r;
    }

  };

}
#endif // __EXO_LIST_H__
