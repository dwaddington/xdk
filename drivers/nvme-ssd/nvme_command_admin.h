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

#ifndef __NVME_COMMAND_ADMIN_H__
#define __NVME_COMMAND_ADMIN_H__

#include <vector>
#include <common/types.h>

#include "nvme_command_structs.h"
#include "nvme_device.h"
#include "nvme_types.h"

class NVME_device;
class NVME_admin_queue;


/** 
 * Class to handle DMA page allocation for admin commands. 
 * TODO: make this pool based.
 * 
 * @param q Need pointer to outer queue to get hold of device
 */
class Single_page_command
{
protected:
  void * _prp1;
  addr_t _prp1_phys;
  NVME_admin_queue * _q;

public:
  Single_page_command(NVME_admin_queue * q);
  ~Single_page_command();
};

/** 
 * Base class for all admin commands
 * 
 * @param q Back pointer to queues to get hold of device
 * 
 */
class Command_admin_base : public Single_page_command
{
protected:
  uint16_t _cid;
  NVME_admin_queue * _queues;

public:
  Command_admin_base(NVME_admin_queue * q) : Single_page_command(q), _queues(q), _cid(0) {
  }

  ~Command_admin_base();

  /** 
   * Read result word from current completion slot
   * 
   * 
   * @return 
   */
  uint32_t get_result() {
    /* check for command completion */
    _queues->check_command_completion(_cid);
    struct nvme_completion * comp = (struct nvme_completion*) _queues->curr_comp_slot()->raw();
    assert(comp);
    return comp->result;
  }

  uint32_t get_status() {
    /* check for command completion */
    struct nvme_completion * comp = (struct nvme_completion*) _queues->curr_comp_slot()->raw();
    assert(comp);
    return comp->status;
  }

  

};



/** 
 * IDENTIFY command
 * 
 * @param q Back pointer to queue
 * @param nsid Namespace id
 * 
 */
class Command_admin_identify : public Command_admin_base
{
public:

  Command_admin_identify(NVME_admin_queue * q, int nsid = 0);

  status_t check_controller_result(NVME_device *);
  status_t check_ns_result(NVME_device * dev, unsigned nsid);
};


typedef enum {
  NVME_FEAT_ARBITRATION	 = 0x01,
  NVME_FEAT_POWER_MGMT	 = 0x02,
  NVME_FEAT_LBA_RANGE	   = 0x03,
  NVME_FEAT_TEMP_THRESH	 = 0x04,
  NVME_FEAT_ERR_RECOVERY = 0x05,
  NVME_FEAT_VOLATILE_WC	 = 0x06,
  NVME_FEAT_NUM_QUEUES	 = 0x07,
  NVME_FEAT_IRQ_COALESCE = 0x08,
  NVME_FEAT_IRQ_CONFIG	 = 0x09,
  NVME_FEAT_WRITE_ATOMIC = 0x0a,
  NVME_FEAT_ASYNC_EVENT	 = 0x0b,
  NVME_FEAT_SW_PROGRESS	 = 0x0c,
} feature_t;

/** 
 * GET FEATURES command
 * 
 * @param q 
 * @param nsid 
 */
class Command_admin_get_features : public Command_admin_base
{
public:
  Command_admin_get_features(NVME_admin_queue * q, feature_t fid, unsigned nsid);

  status_t extract_info(NVME_device::ns_info * nsinfo);
};

/** 
 * SET FEATURES command
 * 
 * @param q 
 * @param nsid 
 */
class Command_admin_set_features : public Command_admin_base
{
private:
  Submission_command_slot * _sc;

public:
  Command_admin_set_features(NVME_admin_queue * q);

  status_t configure_num_queues(uint32_t num_queues);
  status_t configure_interrupt_coalescing(bool turn_on, vector_t vector);
  status_t configure_interrupt_coalescing_time(unsigned aggregation_time, unsigned threshold);
};




/** 
 * CREATE IO SUBMISSION QUEUE command
 * 
 * @param q Pointer to admin queues
 * @param queue_id Queue identifier for this new queue
 * @param queue_size Size of queue in items
 * @param prp1 Physical memory for queue
 * @param cq_id Completion queue id
 * @param priority Priority for weighted round-robin
 * 
 */
class Command_admin_create_io_sq : public Command_admin_base
{
public:
  /* weighted RR priorities */
  typedef enum { 
    PRIO_URGENT=0x0,
    PRIO_HIGH=0x1,
    PRIO_MED=0x2,
    PRIO_LOW=0x3,
  } queue_priority_t;

public:
  Command_admin_create_io_sq(NVME_admin_queue * q, 
                             unsigned queue_id,
                             size_t queue_size,
                             addr_t prp1,
                             unsigned cq_id,
                             queue_priority_t priority=PRIO_HIGH);

};


/** 
 * CREATE IO COMPLETION QUEUE command
 * 
 * @param q Admin queues
 * @param vector Interrupt vector for queue
 * @param queue_id Queue identifier to use for new queue
 * @param queue_size Size of queue in items
 * @param prp1 Physical memory for queue
 * 
 */
class Command_admin_create_io_cq : public Command_admin_base
{
public:
  Command_admin_create_io_cq(NVME_admin_queue * q, 
                             vector_t vector,
                             unsigned queue_id,
                             size_t queue_size,
                             addr_t prp1);

};


/** 
 * DELETE IO SUBMISSION QUEUE command
 * 
 * @param q Pointer to admin queues
 * @param queue_id Identifier of the queue to delete
 * 
 */
class Command_admin_delete_io_queue : public Command_admin_base
{
public:
  Command_admin_delete_io_queue(NVME_admin_queue * q, 
                                NVME::queue_type_t type,
                                unsigned queue_id);

};


/** 
 * FORMAT command
 * 
 * @param q Pointer to admin queues
 * @param lbaf LBA format type
 * 
 */
class Command_admin_format : public Command_admin_base
{
public:
  Command_admin_format(NVME_admin_queue * q,
                       unsigned lbaf,
                       unsigned nsid);
};
#endif // __NVME_COMMAND_ADMIN_H__
