#ifndef __NVME_BATCH_MANAGER__
#define __NVME_BATCH_MANAGER__

#include <boost/atomic.hpp>
#include <common/utils.h>
#include <common/logging.h>
#include "notify.h"

template<typename Element, size_t Size> 
class RingBuffer {
private:
  size_t increment(size_t idx) const; 

  Element _array[Size];
  boost::atomic<size_t> _head;  // head(output) index
  boost::atomic<size_t> _tail;  // tail(input) index

public:

  RingBuffer() : _head(0), _tail(0){}
  virtual ~RingBuffer() {}

  bool push(const Element& item);
  bool pop(Element& item);

  bool wasEmpty() const;
  bool wasFull() const;
  bool isLockFree() const;

  //expose the data structure to the outside -- seems ugly
  size_t get_head() const {return _head.load(boost::memory_order_relaxed);}
  size_t get_tail() const {return _tail.load(boost::memory_order_relaxed);}
  Element *get_array() {return _array;}

  size_t get_last_entry() const {
    size_t head = _head.load(boost::memory_order_relaxed);
    size_t tail = _tail.load(boost::memory_order_relaxed);

    assert(head != tail);

    if(tail == 0)
      return (Size - 1);
    else
      return tail - 1;
  }

};


// Push on tail. Tail is only changed by producer and can be safely loaded using memory_order_relexed; Head is updated by consumer and must be loaded using at least memory_order_acquire
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

template<typename Element, size_t Size>
size_t RingBuffer<Element, Size>::increment(size_t idx) const
{
  size_t new_idx = idx + 1;
  assert(new_idx <= Size);
  if(_unlikely(new_idx == Size))
    return 0;
  else
    return new_idx;
}

/* data structure to record batch info */
typedef struct {
  uint16_t  start_cmdid;
  uint16_t  end_cmdid;
  uint16_t  total;
  uint16_t  counter;
  Notify*   notify;
  bool      ready;
  bool      complete;

  void dump() {
    printf("start_cmd = %u", start_cmdid);
    printf("end_cmd   = %u", end_cmdid);
    printf("total     = %u", total);
    printf("counter   = %u", counter);
    printf("ready     = %u", ready);
    printf("complete  = %u", complete);
  }
}__attribute__((aligned(64))) batch_info_t;


#define BATCH_INFO_BUFFER_SIZE 256 

typedef RingBuffer<batch_info_t, BATCH_INFO_BUFFER_SIZE> batch_info_buffer_t;

//each batch contains cmds with contiguous cmd ids
//cmd id wrap-around is not allowed in a single batch
class NVME_batch_manager {

  private:
    batch_info_buffer_t _buffer;
    batch_info_t* _array;
    //keep tracking the cmdid before the first batch's start cmdid
    //only consumer (CQ) updates this value after initialization
    //cmd id before this value (in terms of circle) is available.
    boost::atomic<uint16_t> _last_available_cmdid;

  public:
    NVME_batch_manager() : _last_available_cmdid(0x0) {
      _array = _buffer.get_array();
    }

    ~NVME_batch_manager() {}

    bool push(const batch_info_t& item) {return _buffer.push(item);}

    bool pop(batch_info_t& item) {return _buffer.pop(item);}

    //done by consumer
    status_t update(uint16_t cmdid) {
      unsigned long long attempts = 0;
      bool ret;
      while((ret = _update_single_iteration(cmdid)) != true) {
        attempts++;
        if(attempts > 1000ULL) {
          PERR("!!!! The command id is not within the range of any batch !!");
          assert(false);
        }
      }
      return Exokernel::S_OK;
    }

#if 0
    //done by producer
    void add_to_last_batch(uint16_t cmdid) {
      //get_last_entry() checks if the queue is empty
      size_t last_entry = _buffer.get_last_entry();

      assert(_array[last_entry].ready == false);

      _array[last_entry].end_cmdid = cmdid;
      _array[last_entry].total++;

      assert(
          (_array[last_entry].end_cmdid - _array[last_entry].start_cmdid + 1 == _array[last_entry].total) || 
          (_array[last_entry].end_cmdid + BATCH_INFO_BUFFER_SIZE - _array[last_entry].start_cmdid == _array[last_entry].total) // 0 is not used for cmd id
          );
    }

    //done by producer
    void finish_batch() {
      //Under a memory model where stores can be reordered, a memory barrier/fence is needed here
      size_t last_entry = _buffer.get_last_entry();
      assert(_array[last_entry].ready == false);
      _array[last_entry].ready = true;
    }
#endif

    /**
     * check if the cmd id has been released by the head batch
     * assuming this can only be called by producer, which alloc cmd ids by incrementing counters
     */
    bool is_available(uint16_t cmdid) {
      uint16_t val = _last_available_cmdid.load(boost::memory_order_relaxed);
      return (cmdid != val);
    }

    /**
     * check if the range of cmd id has been released by the head batch
     * assuming this can only be called by producer, which alloc cmd ids by incrementing counters
     **/
    bool is_available(uint16_t cmdid_start, uint16_t cmdid_end) {
      assert(cmdid_start <= cmdid_end);
      uint16_t val = _last_available_cmdid.load(boost::memory_order_relaxed);
      //PLOG("last_available = %u, start_cmdid = %u, end_cmdid = %u", val, cmdid_start, cmdid_end);
      return (  (cmdid_start < val && cmdid_end < val)
             || (cmdid_start > val && cmdid_end > val)
             );
    }

    /**
     * check if we can safely reset the cmd id counter to 0
     * the cmd id counter can be reset only when the values after(greater than) the current counter are not being used
     * Only called by producer
     */
    bool can_reset_cmdid(uint16_t cmdid_counter) {
      uint16_t val = _last_available_cmdid.load(boost::memory_order_relaxed);
      return (val <= cmdid_counter);
    }

    bool wasEmpty() const { return _buffer.wasEmpty(); }

    bool wasFull() const { return _buffer.wasFull(); }

    bool isLockFree() const {return _buffer.isLockFree();}


  private:
    bool _is_complete(size_t idx) {
      return (_array[idx].ready && _array[idx].counter == _array[idx].total);
    }

    bool _update_single_iteration(uint16_t cmdid) {
      size_t head = _buffer.get_head();
      size_t tail = _buffer.get_tail(); //may get a stale value, this is fine

      if(head > tail) tail += BATCH_INFO_BUFFER_SIZE;

      //FIXME: not true for flush command, as flush command is not recorded in the batch manager
      //assert(head != tail); //Should not be empty

      //TODO: maybe need a better search algo, e.g., binary search
      for(size_t idx = head; idx < tail; idx++){
        size_t idx2 = idx % BATCH_INFO_BUFFER_SIZE;

        assert(_array[idx2].start_cmdid <= _array[idx2].end_cmdid);
        if(cmdid >= _array[idx2].start_cmdid && cmdid <= _array[idx2].end_cmdid){
          _process_update_batch_info(idx2, head, tail);
          return true; //updated sucessfully
        }
      }
      //we may reach here?? because the tail we got can be a stale value
      PLOG("Did NOT find the right range!!");
      return true; //FIXME: flush command is not recorded in the batch manager
    }


    void _process_update_batch_info(size_t idx, size_t head, size_t tail) {

      _array[idx].counter++;
      assert(_array[idx].counter <= _array[idx].total);

      if(_is_complete(idx)) {
        assert(_array[idx].complete == false);
        if(_array[idx].notify) _array[idx].notify->action();
        _array[idx].complete = true;

        //if idx is the head, then pop all finished batch info
        if(idx == head) {
          batch_info_t bi;

          while(idx < tail) {

            size_t idx2 = idx % BATCH_INFO_BUFFER_SIZE;

            if(_is_complete(idx2)) {
              _buffer.pop(bi);
              _last_available_cmdid.store(bi.end_cmdid, boost::memory_order_relaxed);
              // release notify object
              if(bi.notify && bi.notify->free_notify_obj()) {
                PLOG("Free the notify object at entry %lu", idx2);
                delete bi.notify;
              }

              idx++;
              PLOG("Popped batch info entry %lu", idx2);
            }
            else break;
          }//while
        }
      }//_is_complete()   
    }

};


#endif


