#ifndef __NVME_BATCH_MANAGER__
#define __NVME_BATCH_MANAGER__

#include <boost/atomic.hpp>
#include <common/utils.h>
//#include <cstddef>

template<typename Element, size_t Size> 
class RingBuffer {
private:
  size_t increment(size_t idx) const; 

  boost::atomic <size_t>  _tail;  // tail(input) index
  Element    _array[Size];
  boost::atomic<size_t>   _head; // head(output) index

public:

  RingBuffer() : _tail(0), _head(0){}   
  virtual ~RingBuffer() {}

  bool push(const Element& item);
  bool pop(Element& item);

  bool wasEmpty() const;
  bool wasFull() const;
  bool isLockFree() const;

  //expose the data structure to the outside -- seems ugly
  size_t get_head() const {return _head.load(boost::memory_order_relaxed);}
  size_t get_tail() const {return _tail.load(boost::memory_order_relaxed);}
  Element *get_array() const {return _array;}

};


// Push on tail. TailHead is only changed by producer and can be safely loaded using memory_order_relexed
// head is updated by consumer and must be loaded using at least memory_order_acquire
template<typename Element, size_t Size>
bool RingBuffer<Element, Size>::push(const Element& item)
{	
  const size_t current_tail = _tail.load(boost::memory_order_relaxed); 
  const size_t next_tail = increment(current_tail); 
  if(next_tail != _head.load(boost::memory_order_acquire))
  {
    _array[current_tail] = item;
    _tail.store(next_tail, boost::memory_order_release); 
    return true;
  }
  
  return false;  // full queue
}


// Pop by Consumer can only update the head 
template<typename Element, size_t Size>
bool RingBuffer<Element, Size>::pop(Element& item)
{
  const size_t current_head = _head.load(boost::memory_order_relaxed);  
  if(current_head == _tail.load(boost::memory_order_acquire))  
    return false;   // empty queue

  item = _array[current_head]; 
  _head.store(increment(current_head), boost::memory_order_release); 
  return true;
}

// snapshot with acceptance of that this comparison function is not atomic
// (*) Used by clients or test, since pop() avoid double load overhead by not
// using wasEmpty()
template<typename Element, size_t Size>
bool RingBuffer<Element, Size>::wasEmpty() const
{
  return (_head.load() == _tail.load());
}

// snapshot with acceptance that this comparison is not atomic
// (*) Used by clients or test, since push() avoid double load overhead by not
// using wasFull()
template<typename Element, size_t Size>
bool RingBuffer<Element, Size>::wasFull() const
{
  const size_t next_tail = increment(_tail.load());
  return (next_tail == _head.load());
}


template<typename Element, size_t Size>
bool RingBuffer<Element, Size>::isLockFree() const
{
  return (_tail.is_lock_free() && _head.is_lock_free());
}

typedef struct {
  uint16_t start_cmdid;
  uint16_t end_cmdid;
  uint16_t total;
  uint16_t counter;
  bool fixed;

  void dump() {
    printf("start_cmd = %u", start_cmdid);
    printf("end_cmd   = %u", end_cmdid);
    printf("total     = %u", total);
    printf("counter   = %u", counter);
    printf("fixed     = %u", fixed);
  }
} batch_info_t;

#define BATCH_INFO_BUFFER_SIZE 16

typedef RingBuffer<batch_info_t, BATCH_INFO_BUFFER_SIZE> batch_info_buffer_t;

class NVME_batch_manager {

  private:
    batch_info_buffer_t buffer;
    batch_info_t* array;

  public:
    NVME_batch_manager() {
      array = buffer.get_array();
    }
    ~NVME_batch_manager() {}

    bool push(const batch_info_t& item) {return buffer.push(item);}
    bool pop(batch_info_t& item) {return buffer.pop(item);}

    //done by consumer
    void update(uint16_t cmdid) {
      unsigned long long attempts = 0;
      bool ret;
      while((ret = _update_single_loop(cmdid)) != true) {
        attempts++;
        if(attempts > 1000ULL) {
          PERR("!!!! The command id is not within the range of any batch !!");
          assert(false);
        }
      }
    }

    //done by producer
    void add_to_last_batch(uint16_t cmdid) {
      size_t tail = buffer.get_tail();
      array[tail].end_cmdid = cmdid;
      array[tail].total++;
      assert(
          array[tail].end_cmdid - array[tail].start_cmdid + 1 == array[tail].total || 
          array[tail].end_cmdid - array[tail].start_cmdid + 1 + BATCH_INFO_BUFFER_SIZE == array[tail].total
          );
    }

    //done by producer
    void finish_batch() {
      size_t tail = buffer.get_tail();
      assert(array[tail].fixed == false);
      array[tail].fixed = true;
    }

    bool wasEmpty() const { return buffer.wasEmpty(); }
    bool wasFull() const { return buffer.wasFull(); }
    bool isLockFree() const {return buffer.isLockFree();}


  private:
    bool _update_single_loop(uint16_t cmdid) {
      size_t head = buffer.get_head();
      size_t tail = buffer.get_tail(); //may get a stale value, this is fine

      if(head > tail) tail += BATCH_INFO_BUFFER_SIZE;

      for(size_t idx = head; idx < tail; idx++){
        size_t idx2 = idx % BATCH_INFO_BUFFER_SIZE;

        if(cmdid >= array[idx2].start_cmdid && cmdid <= array[idx2].end_cmdid){
          array[idx2].counter++;
          assert(array[idx2].counter <= array[idx2].total);
          if(array[idx2].counter == array[idx2].total) {
            //TODO: callback
            batch_info_t bi;
            buffer.pop(bi);
          }
          return true; //updated sucessfully
        }
        //we may reach here, because the tail we got can be a stale value
        PLOG("Did NOT find the right range!!");
        return false;
      }
    }
};


#endif


