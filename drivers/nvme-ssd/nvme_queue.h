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






#ifndef __NVME_QUEUE_H__
#define __NVME_QUEUE_H__

#include <exo/spinlocks.h>
#include <common/utils.h>
#include "nvme_registers.h"
#include "nvme_command.h"
#include "nvme_types.h"
#include "cq_thread.h"
#include "nvme_batch_manager.h"

/*
  The maximum size for either an I/O Submission Queue or an I/O
  Completion Queue is defined as 64K entries, limited by the maximum
  queue size supported by the controller that is reported in the
  CAP.MQES field. The maximum size for the Admin Submission and Admin
  Completion Queue is defined as 4K entries. One entry in each queue is
  not available for use due to Head and Tail entry pointer definition.
*/

class NVME_device;
class NVME_registers;

typedef uint32_t queue_ptr_t;

class NVME_queues_base
{
protected:
  Exokernel::Bitmap_tracker_threadsafe * _bitmap;
  NVME_batch_manager * _batch_manager;
  
protected:
  enum { 
    SQ_entry_size_bytes = 64,
    CQ_entry_size_bytes = 16,
  };

  enum {
    verbose = false
  };

  NVME_device *        _dev;

  unsigned             _queue_id;      /* queue identifer, admin is 0 */

  void *               _sq_dma_mem;    /* memory for submission queue */
  addr_t               _sq_dma_mem_phys;
  volatile uint32_t *  _sq_db;         /* pointer to SQ doorbell */

  void *               _cq_dma_mem;    /* memory for completion queue */
  addr_t               _cq_dma_mem_phys;
  volatile uint32_t *  _cq_db;         /* pointer to CQ doorbell */

  unsigned             _queue_items;   /* maximum number of items for each queue */

  Submission_command_slot * _sub_cmd;  /* array of submission commands */
  Completion_command_slot * _comp_cmd; /* array of completion commands */

private:

  uint16_t  _sq_tail  __attribute__((aligned(8)));
  uint16_t  _cq_head  __attribute__((aligned(8)));
  uint16_t  _cq_phase __attribute__((aligned(8)));
  volatile uint16_t  _sq_head  __attribute__((aligned(8))); /*get updated from completion command, which indicates the most recent SQ entry fetched*/
  uint16_t _cmdid_counter;

  unsigned _irq;  

  status_t increment_submission_tail(queue_ptr_t *);

protected:
  /* stats counters */
  unsigned long _stat_mean_send_time;
  unsigned long _stat_mean_send_time_samples;
  unsigned long _stat_max_send_time;
  unsigned long _cycles_per_us;
  unsigned long _bad_count;

protected:
  
  /** 
   * Constructor
   * 
   * @param reg 
   * @param queue_id 
   */
  NVME_queues_base(NVME_device * dev, unsigned queue_id, unsigned irq, size_t queue_length);

  ~NVME_queues_base();

  /** 
   * Late set of queue id
   * 
   * @param qid Identifier for the queue.
   */
  void set_queue_id(unsigned qid) {
    _queue_id = qid;
  }
  
public:

  void setup_doorbells();

  uint16_t alloc_cmdid() {
    //TODO: check availability
    _cmdid_counter++;
    if( unlikely(_cmdid_counter == 0) ) _cmdid_counter++;
    return _cmdid_counter;
  }

  void release_slot(uint16_t slot) {
    //status_t s = _bitmap->mark_free(slot);
    //assert(s==Exokernel::S_OK);
  }

  //used to check next available cmdid, but not really alloc the id
  uint16_t next_cmdid() {
    //TODO: check availability
    uint16_t next_id = _cmdid_counter + 1;
    if( unlikely(next_id == 0) ) next_id = 1;
    return next_id;
  }

  void update_batch_manager(uint16_t cmdid) {
    _batch_manager->update(cmdid);
  }

  unsigned queue_length() const {
    return _queue_items;
  }


  /** 
   * Get hold of next available submission command slot.  Only
   * increment the local submission tail pointer - do not press
   * the doorbell (this must be done explicitly)
   * 
   * 
   * @return Pointer to next slot or NULL on full queue.
   */
  Submission_command_slot * next_sub_slot(signed * slot_id);

  /** 
   * Get hold of current completion slot
   * 
   * 
   * @return Pointer to completion queue entry.
   */
  Completion_command_slot * curr_comp_slot();


  /** 
   * Doorbell functions
   * 
   */
  INLINE void ring_submission_doorbell() {
    *(volatile uint32_t *) (_sq_db) = _sq_tail;
  }

  INLINE void ring_completion_doorbell() {
    *(volatile uint32_t *) (_cq_db) = _cq_head;
  }

  /** 
   * Get hold of IRQ vector assigned to this queue
   * 
   * 
   * @return 
   */
  INLINE unsigned irq() const { return _irq; }


  /** 
   * Get phase tag
   * 
   * 
   * @return Phase tag
   */
  INLINE unsigned phase() const { return _cq_phase; }
  
  /** 
   * Debug functions
   * 
   */
  void dump_queue_info();


  /** 
   * Get completion queue head
   * 
   * 
   * @return 
   */
  INLINE unsigned head() const { return _cq_head; }

  /** 
   * Increment the completion head
   * 
   * 
   * @return 
   */
  unsigned increment_completion_head();

  /** 
   * Get hold of next completion if it is complete
   * 
   * 
   * @return Pointer to completion slot
   */
  Completion_command_slot * get_next_completion();

  /** 
   * Helper to get hold of device reference
   * 
   * 
   * @return 
   */
  INLINE NVME_device * device() const { return _dev; }


  /** 
   * Dump queue statisitics
   * 
   */
  void dump_stats() {
    PLOG("Queue(%u): mean_send = %lu usec (%lu cycles), max_send = %lu usec (%lu cycles), bad = %lu",
         _queue_id,
         _stat_mean_send_time / _cycles_per_us,
         _stat_mean_send_time,
         _stat_max_send_time / _cycles_per_us,
         _stat_max_send_time,
         _bad_count);
  }

};


class Command_admin_base;

/**-------------------------------------------------------------------------------------------- 
 * Class to support admin queues (submission and completion)
 * 
 * @param reg Pointer to device register access
 */
class NVME_admin_queue : public NVME_queues_base
{

private:
  enum { 
    Admin_queue_len  = 8,
  };

  void ring_doorbell_single_completion();
  
public:
  NVME_admin_queue(NVME_device * dev, unsigned irq);
  ~NVME_admin_queue();

  void issue_identify_device();
  void feature_info();
  status_t check_command_completion(uint16_t cid);

  /** 
   * Ring submission doorbell, wait for interrupt, then 
   * ring completion doorbell
   * 
   * @param cmd Command 
   */
  uint32_t ring_wait_complete(Command_admin_base& cmd);


  /** 
   * Create a IO completion queue through admin command issue
   * 
   * @param vector Vector to assign to the queue
   * @param queue_id Identifier to use for the queue
   * @param queue_size Size of the queue in items
   * @param prp1 Physical memory area for the queue
   * 
   * @return S_OK on success
   */
  status_t create_io_completion_queue(vector_t vector,
                                      unsigned queue_id,
                                      size_t queue_size,
                                      addr_t prp1);

  /** 
   * Create an IO submission queue through admin command issue
   * 
   * @param queue_id Identifier to use for the queue
   * @param queue_size Size of the queue in items
   * @param prp1 Physical memory area for the queue
   * @param cq_id Identifier of the corresponding completion queue
   * 
   * @return S_OK on success
   */
  status_t create_io_submission_queue(unsigned queue_id,
                                      size_t queue_size,
                                      addr_t prp1,
                                      unsigned cq_id);

  /** 
   * Delete an IO submission queue through admin command issue
   * 
   * @param queue_id Queue identifier to delete
   * @param type Queue type
   * 
   * @return S_OK on success
   */
  status_t delete_io_queue(NVME::queue_type_t type, unsigned queue_id);

  /** 
   * Set the number of queues
   * 
   * @param num_queues 
   * 
   * @return S_OK on success
   */
  status_t set_queue_num(unsigned num_queues);


  /** 
   * Format the disk
   * 
   * @param nsid Namespace identifier
   * 
   * @return S_OK on success
   */
  status_t issue_format(unsigned lbaf, unsigned nsid=1);


  /** 
   * Configure the interrupt coalescing for a specific vector
   * 
   * @param state True to turn on coalescing
   * @param vector Corresponding vector
   * 
   * @return S_OK on success
   */
  status_t set_irq_coal(bool state, vector_t vector);


  /** 
   * Configure the coalescing behaviour
   * 
   * @param agg 
   * @param threshold 
   * 
   * @return 
   */
  status_t set_irq_coal_options(unsigned agg, unsigned threshold);

};


class NVME_IO_queue;

/**--------------------------------------------------------------------------------------------  
 * Class to support IO queues (submission and completion)
 * 
 * @param reg Pointer to device register access
 */
class NVME_IO_queue : public NVME_queues_base                       
{
private:

  CQ_thread *      _cq_thread;
  Callback_manager _callback_manager;

public:
  NVME_IO_queue(NVME_device * dev, 
                 unsigned queue_id, 
                 vector_t base_vector, 
                 vector_t vector, 
                 unsigned core,
                 size_t queue_len);

  ~NVME_IO_queue();

  void dump_info();
  void start_cq_thread();

  INLINE Callback_manager * callback_manager() { return  &_callback_manager; }

  //  Exokernel::Event _pending_reader;
  // Exokernel::Event _wake_cq_thread;
  //  unsigned _cq_released;
  /** 
   * Issue a read command on the associated queue.
   * 
   * @param prp1 Physical destination address
   * @param offset LBA offset to read from
   * @param num_blocks Size of read in 512b blocks
   * @param callback Callback function
   * @param callback_param Cookie for callback
   * @param sequential Hint to whether this is part of a sequential read or not
   * @param access_freq Hint to access frequency for this data
   * @param access_lat Hint to access latency for this data
   * 
   * @return S_OK on success otherwise E_FAIL
   */
  uint16_t issue_async_read(addr_t prp1, 
                            off_t offset,
                            size_t num_blocks,
                            bool sequential=false, 
                            unsigned access_freq=0, 
                            unsigned access_lat=0,
                            unsigned nsid=1);


  /** 
   * Issue a write command on the associated queue.
   * 
   * @param prp1 Physical destination address
   * @param offset LBA offset to read from
   * @param num_blocks Size of read in 512b blocks
   * @param sequential Hint to whether this is part of a sequential read or not
   * @param access_freq Hint to access frequency for this data
   * @param access_lat Hint to access latency for this data
   * 
   * @return S_OK on success otherwise E_FAIL
   */
  uint16_t issue_async_write(addr_t prp1, 
                             off_t offset,
                             size_t num_blocks,
                             bool sequential=false, 
                             unsigned access_freq=0, 
                             unsigned access_lat=0,
                             unsigned nsid=1);


  uint16_t issue_async_io_batch(io_descriptor_t* io_desc,
                                uint64_t length
                                );


  /** 
   * Issue flush command
   * 
   * 
   * @return 
   */
  uint16_t issue_flush();
};

#endif // __NVME_QUEUE_H__
