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
  Copyright (C) 2013, Juan A. Colmenares <juan.col@samsung.com>
*/

/**
 * 
 * Implementation of the Non-Blocking Buffer (for single address space). 
 * It is based on the paper: "A Non-Blocking Buffer Mechanism for 
 * Real-Time Event %Message Communication" by K. H. Kim, published in Real-Time
 * Systems 32(3), March 2006.
 */

#ifndef _EXOKERNEL_SYMMETRIC_NBB_H_
#define _EXOKERNEL_SYMMETRIC_NBB_H_

#ifdef NUMA_ENABLE
        #include <numa.h>
#else
        #include <common/xdk_numa_wrapper.h>
#endif

namespace Exokernel
{

  /** The operation was performed successfully. */
  const int NBB_OK = 0;

  /** The insertion operation failed because the NBB was full. */
  const int NBB_FULL = -1; 

  /**
   * The insertion operation failed because the NBB was full, but a consumer was
   * in the middle of reading an item. 
   */
  const int NBB_FULL_BUT_CONSUMER_READING = -2; 

  /** The read operation failed because the NBB is empty. */
  const int NBB_EMPTY = -3;

  /**
   * The read operation failed because the NBB was empty, but a producer was
   * in the middle of inserting an item. 
   */
  const int NBB_EMPTY_BUT_PRODUCER_INSERTING = -4;



  template<class T> class Symmetric_nbb; // Forward declaration


  /** Access point to a symmetric NBB object for a producer thread. */
  template <class T>
  class Symmetric_producer_axpoint {

  public:

    /** 
     * Inserts an item into the NBB.
     *     
     * @param item the item to be inserted. 
     *        The type of the item (i.e., the template parameter T) can be a pointer type, 
     *        a primitive type, or a user-defined type that implements the default constructor 
     *        and the assignment operator. 
     * @retval NBB_OK if the item was inserted sucessfully.
     * @retval NBB_FULL if the NBB is full.
     * @retval NBB_FULL_BUT_CONSUMER_READING if the NBB was full but the
     *         consumer thread was  in the middle of reading an item.
     */
    int insert_item(T item) {
      unsigned temp_ac = _nbb->_ac; // read is atomic

      if (_nbb->_last_uc - temp_ac == 2 * _nbb->_buffer_size) {
        return NBB_FULL;
      }

      if (_nbb->_last_uc - temp_ac == 2 * _nbb->_buffer_size - 1) {
        return NBB_FULL_BUT_CONSUMER_READING;
      }

      _nbb->_uc = _nbb->_last_uc + 1; // write is atomic

      int index = (_nbb->_last_uc / 2) % _nbb->_buffer_size; 
      _nbb->_buffer[index] = item;

      unsigned tmp_last_uc = _nbb->_last_uc + 2; 
      if (tmp_last_uc == _nbb->_count_limit) {
        tmp_last_uc = 0; 
      }

      _nbb->_uc = tmp_last_uc; // write is atomic

      _nbb->_last_uc = _nbb->_uc;

      return NBB_OK;
    }


    /** Destructor */     
    virtual ~Symmetric_producer_axpoint(){}

  private:

    /**
     * Private default constructor.
     * It is private because we do not want the application to directly create
     * the access-point objects. Only the Symmetric NBB class can create them.
     * Each Symmetric NBB object has a producer access point and application can
     * access it.
     */ 
    Symmetric_producer_axpoint() : _nbb(NULL) {}

    Symmetric_nbb<T>* _nbb; //!< Pointer to the NBB.

    /** Private empty copy constructor. */
    Symmetric_producer_axpoint(const Symmetric_producer_axpoint<T>& axpoint) {};

    /** Private empty assignment operator. */
    Symmetric_producer_axpoint<T>& operator=(const Symmetric_producer_axpoint<T>& axpoint) {};

    friend class Symmetric_nbb<T>;
  };



  /** Access point to a Symmetric NBB object for the consumer thread. */
  template <class T>
  class Symmetric_consumer_axpoint {
    
  public:

    /**
     * Reads an item from the NBB and copies its content into an item passed as
     * a reference by the callee consumer thread.
     *     
     * @param rItem reference to the item that will receive the copy of the
     *        content of the item in the NBB that is being read. Note that this
     *        method modifies the content of the object referenced by this
     *        parameter.  The type of the item (i.e., the template parameter T)
     *        can be a pointer type, a primitive type, or a user-defined type
     *        that implements the default constructor and the assignment
     *        operator. 
     * @retval NBB_OK if the pointer was read sucessfully, 
     * @retval NBB_EMPTY if the NBB was empty,
     * @retval NBB_EMPTY_BUT_PRODUCER_INSERTING if the NBB was empty but the
     *         producer thread was in the middle of inserting an item.
     */    
    int read_item(T& rItem) {
      unsigned temp_uc = _nbb->_uc; // read is atomic

      if (temp_uc == _nbb->_last_ac) {
        return NBB_EMPTY;
      }

      if (temp_uc - _nbb->_last_ac == 1) {
        return NBB_EMPTY_BUT_PRODUCER_INSERTING;
      }

      _nbb->_ac = _nbb->_last_ac + 1; // write is atomic

      int index = (_nbb->_last_ac / 2) % _nbb->_buffer_size;
      rItem = _nbb->_buffer[index];

      unsigned tmp_last_ac = _nbb->_last_ac + 2; 
      if (tmp_last_ac == _nbb->_count_limit) {
        tmp_last_ac = 0; 
      }

      _nbb->_ac = tmp_last_ac; // write is atomic

      _nbb->_last_ac = _nbb->_ac;

      return NBB_OK;
    }

    /** Destructor. */     
    virtual ~Symmetric_consumer_axpoint(){}

  private:

    /**
     * Private default constructor.
     * It is private because we do not want the application to directly create
     * the access-point objects. Only the Symmetric NBB class can create them.
     * Each Symmetric NBB object has a consumer access point and application can
     * access it.
     */ 
    Symmetric_consumer_axpoint() : _nbb(NULL) {};

    Symmetric_nbb<T>* _nbb; //!< Pointer to the NBB.

    /** Private empty copy constructor. */
    Symmetric_consumer_axpoint(const Symmetric_consumer_axpoint<T>& axpoint) {};

    /** Private empty assignment operator. */
    Symmetric_consumer_axpoint<T> operator=(const Symmetric_consumer_axpoint<T>& axpoint) {};

    friend class Symmetric_nbb<T>;
  };



  /** 
   *  Symmetric implementation of the Non-Blocking Buffer (for a single address space). 
   *   
   *  This class (along with the access point classes for producer and consumer) implements a
   *  Symmetric version of the Non-Blocking Buffer that allows a single producer thread to insert
   *  items in the NBB and allows a single consumer thread to read items from the NBB.  The term
   *  symmetric refers to that that each item is copied twice, when it is inserted and when it is
   *  read.
   *  
   *  The type of the items is defined by the template parameter \c T. 
   *  \c T can be a primitive type or a type with a default constructor and assigment operator.
   *  \c T can also be a pointer type, which is particularly useful when copying the items is an 
   *  expensive operation.
   * 
   *  This implementation is based on the paper: "A Non-Blocking Buffer Mechanism for Real-Time Event
   *  %Message Communication" by K. H. Kim, published in Real-Time Systems 32(3), March 2006.
   */
  template<class T> 
  class Symmetric_nbb {

  public:

    /** 
     * Producer thread's access point. 
     * This object is used by the producer thread to insert items into the NBB. 
     */
    Symmetric_producer_axpoint<T> producer_axpoint; 

    /** 
     * Consumer thread's access point.  
     * This object is used by the consumer thread to read items from the NBB. 
     */
    Symmetric_consumer_axpoint<T> consumer_axpoint;

    /**
     *  Constructor.
     *  It creates a Symmetric_nbb with the specified capacity (i.e., number of slots).
     *  @param size size of the buffer. It panics if \c size is equal to or less than zero.
     *  @param hart_id Identifier for the home hardware thread (or hart) for NUMA-aware 
     *                 allocation of internal memory.
     */
    Symmetric_nbb(int size, int32_t hart_id) : _buffer(NULL) {
      if (size <= 0) {
        panic("Symmetric_nbb: The size of the NBB must be larger than 0.");
      }

      if (hart_id < 0) {
        panic("Symmetric_nbb: hart_id must be equal to or larger than 0.");
      }
    
      _hart_id = (uint32_t) hart_id;

      try {
        // Allocate buffer in NUMA aware memory.
        _buffer_alloc_size = round_up_page(sizeof(T) * size);
        void* p = numa_alloc_onnode(_buffer_alloc_size, numa_node_of_cpu(hart_id));
        assert(p);

        _buffer = new(p) T[size];
      }
      catch(...) {
        panic("Symmetric_nbb: Uncaught exception.");
      }
    
      assert(_buffer != NULL);

      _buffer_size = size;
    
      _uc = 0;
      _ac = 0; 
      _last_uc = 0;  
      _last_ac = 0;   


      // _count_limit is the largest even unsigned integer that is multiple of
      // _buffer_size. 
      unsigned k = 2 * (((unsigned) (~0x0)) / (4 * _buffer_size)); 
      assert(k >= 2); 
      _count_limit = k * _buffer_size; 

      this->configure_access_points(producer_axpoint, consumer_axpoint);
    }

    /** Destructor. */
    virtual ~Symmetric_nbb() {
      numa_free((void*)_buffer, _buffer_alloc_size);
    };
  
    /** 
     * Returns the identifier of the home hardware thread used for 
     * internally allocating NUMA-aware memory.
     */
    uint32_t get_hart_id() { return this->_hart_id; };

  private:

    /** Configures the access-point objects. */
    void configure_access_points(Symmetric_producer_axpoint<T>& prod_axpoint, 
                                 Symmetric_consumer_axpoint<T>& cons_axpoint) {
      prod_axpoint._nbb = this;
      cons_axpoint._nbb = this;
    };

    volatile unsigned _uc __attribute__ ((aligned)); //!< update counter
    volatile unsigned _ac __attribute__ ((aligned)); //!< acknowledgment counter
    unsigned _last_uc; 
    unsigned _last_ac;     

    unsigned _buffer_size; //!< size of the buffer
    size_t   _buffer_alloc_size; //!< actual size of allocation in bytes
    T*       _buffer; //!< pointer to the buffer

    unsigned _count_limit;

    /** 
     * Identifier of the home hardware thread used for allocating NUMA-aware
     * memory to the internal buffer.
     */
    uint32_t _hart_id; 


    /** Private empty copy constructor. */
    Symmetric_nbb(const Symmetric_nbb<T>& nbb) {};

    /** Private empty assignment operator. */
    Symmetric_nbb<T>& operator=(const Symmetric_nbb<T>& nbb) {};

    friend int Symmetric_producer_axpoint<T>::insert_item(T item);
    friend int Symmetric_consumer_axpoint<T>::read_item(T& rItem);

  };

}

#endif // _EXOKERNEL_SYMMETRIC_NBB_H_  
