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

#include <libexo.h>
#include <sys/param.h>
#include <common/cycles.h>
#include <exo/pagemap.h>

#include "nvme_queue.h"
#include "nvme_device.h"
#include "nvme_command.h"
#include "nvme_command_admin.h"
#include "nvme_command_io.h"

static Exokernel::Pagemap pm;

using namespace Exokernel;

/**---------------------------------------------------------------------------------------- 
 * BASE QUEUES
 * ---------------------------------------------------------------------------------------- 
 */


/** 
 * Constructor
 * 
 * @param reg 
 * @param queue_id 
 */
NVME_queues_base::NVME_queues_base(NVME_device * dev, unsigned queue_id, unsigned irq, size_t queue_length) : 
  _dev(dev),  _queue_id(queue_id),
  _sq_dma_mem(NULL), _sq_dma_mem_phys(0), 
  _cq_dma_mem(NULL), _cq_dma_mem_phys(0), 
  _sq_head(0),
  _sq_tail(0),
  _cq_head(0),
  _cq_phase(1),  
  _irq(irq),
  _queue_items(queue_length),
  _cmdid_counter(0)
{
  //   static unsigned stride = NVME_CAP_STRIDE(_registers->cap);
  //   unsigned db_offset = NVME_OFFSET_COMPLETION_DB(cap,queue_id,stride);
  //   *(offset<uint32_t>(db_offset)) = p;
  
  /* allocate bitmap */
  _bitmap = new Exokernel::Bitmap_tracker_threadsafe(queue_length*4);
  assert(_bitmap);
  _bitmap->next_free(); // use first slot to avoid cmdid==0

  /* allocate batch manager */
  _batch_manager = new NVME_batch_manager();
  assert(_batch_manager);

  _stat_mean_send_time_samples = 0;
  _stat_mean_send_time = 0;
  _stat_max_send_time = 0;
  _cycles_per_us = get_tsc_frequency_in_mhz();
  _bad_count = 0;
}

void NVME_queues_base::setup_doorbells()
{
  NVME_registers * r = _dev->regs();  

  /* set pointer to SQ doorbell */
  unsigned stride = NVME_CAP_STRIDE(r->cap());
  _sq_db = r->offset<uint32_t>(NVME_OFFSET_SUBMISSION_DB(r->cap(),_queue_id,stride));

  /* set pointer to CQ doorbell */
  _cq_db = r->offset<uint32_t>(NVME_OFFSET_COMPLETION_DB(r->cap(),_queue_id,stride));
}


NVME_queues_base::~NVME_queues_base() 
{
  /* delete bitmap */
  delete _bitmap;
  delete _batch_manager;
}



/** 
 * Increment the submission tail; deal with wrap-around and full queue conditions
 * 
 * 
 * @return S_OK on success, E_FULL on full
 */
status_t NVME_queues_base::increment_submission_tail(queue_ptr_t * tptr) {

  /* check if the SQ is full */
  uint16_t new_tail = _sq_tail + 1;

  if(_unlikely(new_tail >= _queue_items)) 
    new_tail -= _queue_items;

  if( new_tail == _sq_head ) {
    PLOG("Queue %d is full (sq_head = %u, sq_tail = %u) !!", _queue_id, _sq_head, _sq_tail);
    //printf("Queue %d is full (sq_head = %u, sq_tail = %u) !!\n", _queue_id, _sq_head, _sq_tail);
    return Exokernel::E_FULL;
  }

  *tptr = _sq_tail++;

  /* check for wrap-around */
  if(_sq_tail == _queue_items)
    _sq_tail = 0;
  
  return Exokernel::S_OK;
}


/** 
 * Increment the completion head pointer and wrap-around if needed
 * 
 * @param hptr 
 * 
 * @return 
 */
unsigned NVME_queues_base::increment_completion_head() {

  if(++_cq_head == _queue_items) {
    _cq_head = 0;
    _cq_phase = !_cq_phase;
  }

  return _cq_head;
}


/** 
 * Get next completed slot *providing* that it is the expected phase.  If the 
 * current head has not completed return NULL.  Each successful completion 
 * implicitly increments the completion head (_cq_head).
 * 
 * 
 * @return Pointer to the next completion slot or NULL if no more entries
 */
Completion_command_slot * NVME_queues_base::get_next_completion() 
{
  unsigned curr_head = _cq_head;

  if(_comp_cmd[curr_head].phase_tag != _cq_phase) {
    PLOG("slot at head=%u is not phase changed (phase=0x%x) (Q:%u)",curr_head, _comp_cmd[curr_head].phase_tag, _queue_id);
    return NULL;
  }

  PLOG("get_next_completion looking at head=%u (Q:%u)",curr_head, _queue_id);

  PLOG("completion head: slot=%u phase=0x%x status=0x%x (Q:%u)",
       curr_head,
       _comp_cmd[curr_head].phase_tag,
       _comp_cmd[curr_head].status,
       _queue_id);

  /* check status code */
  unsigned status =  _comp_cmd[curr_head].status;

  if(status > 0) {
    unsigned DNR  = 0x1  & (status >> 14);
    unsigned More = 0x1  & (status >> 13);
    unsigned SCT  = 0x7  & (status >>  8);
    unsigned SC   = 0xff & status;

    PLOG("DNR = 0x%x, More = 0x%x, SCT = 0x%x, SC = 0x%x",
         DNR,
         More,
         SCT,
         SC
         );

    if(SCT == 0x0) {
      //PDBG("Generic Command Status");
      if(_unlikely(SC != 0x0)) {  /* not Successful Completion */
        PERR("DNR = 0x%x, More = 0x%x, SCT = 0x%x, SC = 0x%x",
             DNR,
             More,
             SCT,
             SC
             );
        assert(false);
      }
    } else if (SCT == 0x1) {
      PDBG("Command Specific Status");
      assert(false);
    } else if (SCT == 0x2) {
      PDBG("Media Error");
      assert(false);
    } else if (SCT == 0x7) {
      PDBG("Vendor Specific");
      assert(false);
    } else {
      PDBG("Reserved");
      assert(false);
    }
  }

  /* indicate that completion has been dealt with */
  increment_completion_head();

  if(verbose)
    PDBG("CURR_HEAD = %u",curr_head);

  PLOG("get_next_completion returning %u",curr_head);
  return &_comp_cmd[curr_head];
}



#if 0
/** 
 * Debugging
 * 
 */
void NVME_queues_base::dump_queue_info() {

  PINF("SQ_Tail=%u CQ_Head=%u",_sq_tail,_cq_head);
  PINF("CQ hd vaddr=%p (paddr=0x%lx)",_comp_cmd, (unsigned long) pm.virt_to_phys(_comp_cmd));

  __sync_synchronize();

  for(unsigned i=0;i < 20;i++) { // _queue_items
    PINF("SQCMD[%u][cid=%d](0x%x) CQCMD[%u][cid=%u](status=0x%x)(sct=0x%x)(phase=%u)(sqhd=%u)(result=%x)",
         i,
         _sub_cmd[i].command_id,
         _sub_cmd[i].opcode,
         i,
         _comp_cmd[i].command_id,
         _comp_cmd[i].status & 0xff,
         (_comp_cmd[i].status >> 9) & 7,
         _comp_cmd[i].phase_tag,
         _comp_cmd[i].sq_hdptr,
         _comp_cmd[i].dw0
         );
  }
}
#endif

Submission_command_slot * NVME_queues_base::next_sub_slot(signed * cmdid) {

  queue_ptr_t curr_ptr;
  
  *cmdid = alloc_cmdid();
  assert(*cmdid > 0);

  status_t st;

  NVME_LOOP( ((st = increment_submission_tail(&curr_ptr)) != Exokernel::S_OK), false);

  _sub_cmd[curr_ptr].clear();

  PLOG("sub_slot = %u (Q:%u)", curr_ptr, _queue_id);
  return &_sub_cmd[curr_ptr];
}

/** 
 * Get hold of the current completion slot.  Don't forget completion
 * might be out-of-order.
 * 
 * 
 * @return Pointer to completion slot
 */
Completion_command_slot * NVME_queues_base::curr_comp_slot() {
  assert(_cq_head < _queue_items);
  return &_comp_cmd[_cq_head];
}





/**---------------------------------------------------------------------------------------- 
 * ADMIN QUEUES
 * ---------------------------------------------------------------------------------------- 
 */

/** 
 * Constructor
 * 
 * @param reg Pointer to device registers
 * @param dev Pointer to top-level device structure
 */
NVME_admin_queue::NVME_admin_queue(NVME_device * dev, unsigned irq) : 
  NVME_queues_base(dev, 0 /* admin is queue 0 */, irq, Admin_queue_len)
{
  NVME_INFO("creating NVME admin queues (IRQ=%u)\n", irq);
  size_t num_pages;
  assert(dev);

  NVME_registers * reg = dev->regs();

  _queue_items = Admin_queue_len; 

  /* allocate memory for the completion queue */
  num_pages = round_up_page(CQ_entry_size_bytes * Admin_queue_len) / PAGE_SIZE;

  PLOG("allocating %ld pages for admin completion queue",num_pages);
  _cq_dma_mem = dev->alloc_dma_pages(num_pages, &_cq_dma_mem_phys);  
  memset(_cq_dma_mem, 0, num_pages * PAGE_SIZE);
  NVME_INFO("NVME_completion_queue virt=%p phys=0x%lx pages=%lu\n",_cq_dma_mem,_cq_dma_mem_phys,num_pages);
  assert((_cq_dma_mem_phys & 0xfffUL) == 0UL);

  /* allocate memory for the submission queue */
  num_pages = round_up_page(SQ_entry_size_bytes * Admin_queue_len) / PAGE_SIZE;
  _sq_dma_mem = dev->alloc_dma_pages(num_pages, &_sq_dma_mem_phys);
  PLOG("allocating %ld pages for admin submission queue",num_pages);
  memset(_sq_dma_mem,0,num_pages * PAGE_SIZE);
  NVME_INFO("NVME_submission_queue virt=%p phys=0x%lx pages=%lu\n",_sq_dma_mem,_sq_dma_mem_phys,num_pages);
  assert((_sq_dma_mem_phys & 0xfffUL) == 0UL);

  /* set base registers */
  reg->asq_base(_sq_dma_mem_phys); 
  reg->acq_base(_cq_dma_mem_phys);

  /* set size registers */
  reg->asqs(Admin_queue_len - 1);
  reg->acqs(Admin_queue_len - 1);

  /* set up pointers into queues (queues are contiguous)  */
  assert(sizeof(Submission_command_slot) == 64);
  _sub_cmd = (Submission_command_slot *) _sq_dma_mem;
  PDBG("Admin IO queue _sub_cmd = %p",(void *) _sub_cmd);

  assert(sizeof(Completion_command_slot) == 16);
  _comp_cmd = (Completion_command_slot *) _cq_dma_mem;
  PDBG("Admin IO queue _comp_cmd = %p",(void *) _comp_cmd);

  assert(_comp_cmd);
  assert(_sub_cmd);
}

/** 
 * Destructor
 * 
 */
NVME_admin_queue::~NVME_admin_queue()
{
  /* free DMA memory */
  _dev->free_dma_pages(_sq_dma_mem);
  _dev->free_dma_pages(_cq_dma_mem);
}

/** 
 * Admin queues are serial only. Each command is 
 * issued with an individual doorbell press.
 * 
 */
void NVME_admin_queue::ring_doorbell_single_completion()
{
  assert(_queue_id == 0);
  //FIXME: the completion queue entries seem empty?!
  //dump_queue_info(Admin_queue_len);
  //update_sq_head( curr_comp_slot() );
  update_admin_sq_head(); /*_sq_head = _cq_head --  assuming admin queue are only synchronous */

  ring_completion_doorbell();   /* current slot is handled */
  increment_completion_head();  /* record next free */
}

/** 
 * Issue identify device command
 * 
 */
void NVME_admin_queue::issue_identify_device()
{
  PLOG("Issuing identify device ...");

  /* construct command */
  Command_admin_identify ctrl_cmd(this);

  /* submit */
  ring_submission_doorbell();
  _dev->wait_for_msix_irq(irq());

  /* verify result */
  ctrl_cmd.check_controller_result(_dev);

  /* ring completion doorbell and increment completion head*/
  ring_doorbell_single_completion();

  /* next issue for each namespace */
  for(unsigned n=1; n<=_dev->_ident._nn; n++) {

    PLOG("Identifying namespace %u", n);

    Command_admin_identify cmd(this,n);

    /* ring doorbell */
    ring_submission_doorbell();
    _dev->wait_for_msix_irq(irq());
    
    /* verify result */
    cmd.check_ns_result(_dev,n);
    
    /* ring completion doorbell and increment completion head*/
    ring_doorbell_single_completion();
  }  

}

void NVME_admin_queue::feature_info()
{
  assert(!_dev->_ns_ident.empty());

  /* for each namespace on the device */
  for(unsigned n=1; n<=_dev->_ident._nn; n++) {
    Command_admin_get_features cmd(this, NVME_FEAT_LBA_RANGE, n);

    /* ring doorbell */
    ring_submission_doorbell();
    _dev->wait_for_msix_irq(irq());

    cmd.extract_info(_dev->_ns_ident[n]);

    /* ring completion doorbell and increment completion head*/
    ring_doorbell_single_completion();
  }
}

status_t NVME_admin_queue::check_command_completion(uint16_t cid)
{
  Completion_command_slot * cc = curr_comp_slot();
  
  /* NOTE: there is an issue with the interrupt triggering
     before the CQ registers have been updated.  I am not sure
     if this is by design or if there is some other issue 
     here with the logic.
  */
  {
    unsigned long long attempts = 0;
    while(cc->command_id != cid) {
      attempts++;
      usleep(1000);
      if(attempts > 1000000000ULL) {
        PERR("!!!! Check command completion timed out.");
        return Exokernel::E_FAIL;
      }
    }
  }

  /* check status code */
  if(cc->status != 0x0) {
    NVME_INFO("Bad Status! (status=0x%x) (cid=%u)\n",cc->status,cc->command_id);
    dump_queue_info();
  }

  if(cc->command_id != cid) {
    NVME_INFO("ERROR: completion response command_id mismatch (cc->cid:%u!=%u)\n",cc->command_id, cid);
    PLOG("_cq_head=%u\n",head());
    dump_queue_info();
    return Exokernel::E_FAIL;
  }
  if(cc->phase_tag != phase()) {
    NVME_INFO("Error: completion response phase mismatch (phase:%u!=%u)\n",cc->phase_tag, phase());
    return Exokernel::E_FAIL;
  }

  memset(cc,0,sizeof(Completion_command_slot));
  return Exokernel::S_OK;
}

uint32_t NVME_admin_queue::ring_wait_complete(Command_admin_base& cmd)
{
  /* submit */
  ring_submission_doorbell();
  _dev->wait_for_msix_irq(irq());

  uint32_t r = cmd.get_result();  

  /* ring completion doorbell and increment completion head*/
  ring_doorbell_single_completion();

  return r;
}

status_t NVME_admin_queue::create_io_completion_queue(vector_t vector,
                                                      unsigned queue_id,
                                                      size_t queue_items,
                                                      addr_t prp1)
{
  assert(prp1);

  /* construct command */
  Command_admin_create_io_cq cmd(this,vector,queue_id,queue_items,prp1);

  if(ring_wait_complete(cmd)!=0) 
    assert(0);

  /* verify result */
  if(cmd.get_status() != 0) {
    PERR("unexpected status from create_io_completion_queue (0x%x)",cmd.get_status());
    assert(0);
  }

  return Exokernel::S_OK;
}


status_t NVME_admin_queue::create_io_submission_queue(unsigned queue_id,
                                                       size_t queue_size,
                                                       addr_t prp1,
                                                       unsigned cq_id)
{
  assert(prp1);

  /* construct command */
  Command_admin_create_io_sq cmd(this,queue_id,queue_size,prp1,cq_id);

  if(ring_wait_complete(cmd)!=0)
    assert(0);

  /* verify result - the specification says that a result of 0 is bad,
     but its not clear what a good result is in this case.
   */
  if(cmd.get_status() != 0) {
    PERR("unexpected status from create_io_submission_queue (0x%x)",cmd.get_status());
    assert(0);
  }

  return Exokernel::S_OK;
}


status_t NVME_admin_queue::delete_io_queue(NVME::queue_type_t type,
                                            unsigned queue_id)
{
  /* construct command */
  Command_admin_delete_io_queue cmd(this,type,queue_id);

  if(ring_wait_complete(cmd)!=0)
    assert(0);

  return Exokernel::S_OK;
}

status_t NVME_admin_queue::set_queue_num(unsigned num_queues)
{
  Command_admin_set_features cmd(this);
  cmd.configure_num_queues(num_queues);

  uint32_t r = ring_wait_complete(cmd);
  uint32_t result = MIN(r & 0xffff, ((uint32_t)r) >> 16) + 1;
  PLOG("result=0x%x status=0x%x",r, cmd.get_status());

  return Exokernel::S_OK;
}


status_t NVME_admin_queue::set_irq_coal(bool state, vector_t vector)
{
  /* construct command */
  Command_admin_set_features cmd(this);
  cmd.configure_interrupt_coalescing(state, vector);
  
  ring_wait_complete(cmd);
  return Exokernel::S_OK;
}

status_t NVME_admin_queue::set_irq_coal_options(unsigned agg, unsigned threshold)
{
  /* construct command */
  Command_admin_set_features cmd(this);
  cmd.configure_interrupt_coalescing_time(agg,threshold);
  
  ring_wait_complete(cmd);
  return Exokernel::S_OK;
}



status_t NVME_admin_queue::issue_format(unsigned lbaf, unsigned nsid)
{
  assert(nsid==1); 

  /* construct command */
  Command_admin_format cmd(this,lbaf,nsid);

  NVME_INFO("Formatting disk? are you sure? [enter to continue]\n");
  getchar();
  uint32_t result = ring_wait_complete(cmd);
  NVME_INFO("format result=0x%x\n",result);

  return Exokernel::S_OK;
}


/**---------------------------------------------------------------------------------------- 
 * IO QUEUES
 * ---------------------------------------------------------------------------------------- 
 */

/** 
 * Constructor
 * 
 * @param reg 
 * @param dev 
 * @param vector 
 * @param core 
 */
NVME_IO_queue::NVME_IO_queue(NVME_device * dev, 
                             unsigned queue_id,
                             vector_t base_vector,
                             vector_t vector, 
                             unsigned core,
                             size_t queue_length) : 
  NVME_queues_base(dev, queue_id, vector, queue_length),
  _cq_thread(NULL)
{
  assert(dev);
  assert(vector);
  assert(queue_length > 0);
  
  status_t rc;
  size_t num_pages;

  NVME_registers * reg = dev->regs();
  assert(reg);
  NVME_admin_queue * admin = dev->admin_queues();
  assert(admin);

  _queue_items = queue_length;
  _queue_id = queue_id; 
  assert(_queue_id > 0); /* admin Q is id 0 */

  /* init batch counters */
  _sq_batch_counter = 0;
  prev_tsc = 0;
  cur_tsc = 0;
  drain_tsc = (unsigned long long)get_tsc_frequency_in_mhz() * US_PER_RING;

  /* allocate memory for the completion queue */
  num_pages = (round_up_page(CQ_entry_size_bytes * _queue_items)/PAGE_SIZE)*2;

  PLOG("allocating %ld pages for IO completion queue",num_pages);
  _cq_dma_mem = dev->alloc_dma_pages(num_pages, &_cq_dma_mem_phys);  
  assert(_cq_dma_mem);
  assert(_cq_dma_mem_phys);
  memset(_cq_dma_mem,0,num_pages * PAGE_SIZE); /* important to zero phase bits */

  NVME_INFO("creating IO completion queue (%u) virt=%p phys=0x%lx pages=%lu vector=%d\n",
            _queue_id,_cq_dma_mem,_cq_dma_mem_phys,num_pages,vector);
  assert((_cq_dma_mem_phys & 0xfffUL) == 0UL);

  /* create IO completion queue */  
  rc = admin->create_io_completion_queue(_queue_id, /* logical vector */
                                         _queue_id,
                                         _queue_items,
                                         _cq_dma_mem_phys);
  assert(rc==Exokernel::S_OK);

  /* allocate memory for the submission queue */
  num_pages = (round_up_page(SQ_entry_size_bytes * _queue_items)/PAGE_SIZE)*2;

  PLOG("allocating %ld pages for IO submission queue",num_pages);
  _sq_dma_mem = dev->alloc_dma_pages(num_pages, &_sq_dma_mem_phys);

  assert(_sq_dma_mem);
  assert(_sq_dma_mem_phys);
  memset(_sq_dma_mem,0,num_pages * PAGE_SIZE);

  NVME_INFO("create IO_submission_queue (%u) virt=%p phys=0x%lx pages=%lu\n",
            _queue_id,_sq_dma_mem,_sq_dma_mem_phys,num_pages);
  assert((_sq_dma_mem_phys & 0xfffUL) == 0UL);
  
  /* create IO submission queue */
  rc = admin->create_io_submission_queue(_queue_id,
                                         _queue_items,
                                         _sq_dma_mem_phys,
                                         _queue_id);
  assert(rc==Exokernel::S_OK);

  /* set up pointers into queues (queues are contiguous)  */
  assert(sizeof(Submission_command_slot) == 64);
  _sub_cmd = (Submission_command_slot *) _sq_dma_mem;
   
  assert(sizeof(Completion_command_slot) == 16);
  _comp_cmd = (Completion_command_slot *) _cq_dma_mem;

  /* finally create CQ thread */
  _cq_thread = new CQ_thread(this, core, vector, _queue_id);

  NVME_INFO("NVME_IO_queues ctor'ed\n");
}

void NVME_IO_queue::start_cq_thread()
{
  assert(_cq_thread);
  _cq_thread->start();
}

/** 
 * Destructor
 * 
 */
NVME_IO_queue::~NVME_IO_queue() {

  status_t rc;
  NVME_admin_queue * admin = _dev->admin_queues();
  assert(admin);

  /* cancel waiting threads */
  assert(_cq_thread);
  _cq_thread->cancel();
  delete _cq_thread;
  
  /* delete IO queues */
  rc = admin->delete_io_queue(NVME::QTYPE_SUB, _queue_id);
  assert(rc==Exokernel::S_OK);
  rc = admin->delete_io_queue(NVME::QTYPE_COMP, _queue_id);
  assert(rc==Exokernel::S_OK);

  NVME_INFO("~NVME_IO_queues deleted queues OK.\n");
}


void NVME_IO_queue::dump_info() {
  NVME_INFO("IO QUEUE [%u]\n",_queue_id);
}

static unsigned issued = 0;

uint16_t NVME_IO_queue::issue_async_read(addr_t prp1, 
                                          off_t offset, 
                                          size_t num_blocks,
                                          bool sequential, 
                                          unsigned access_freq, 
                                          unsigned access_lat,
                                          unsigned nsid) 
{

  signed slot_id;
  Submission_command_slot * sc;

  cpu_time_t start = rdtsc();


  Completion_command_slot * ccs;
  if(!(sc = next_sub_slot(&slot_id))) {
    panic("no sub slots! issued=%u -- this should not happen!!",issued);
  }  

  assert(slot_id >= 0);
  assert(slot_id > 0);

  Command_io_rw cmd(sc,
                    slot_id, /* command_id */
                    nsid, /* device namespace */
                    prp1, 
                    offset, 
                    num_blocks, 
                    sequential, 
                    access_freq, 
                    access_lat,
                    false);  

#if (SQ_MAX_BATCH_TO_RING > 1)
  cur_tsc = rdtsc();
  _sq_batch_counter++;
  if(_unlikely(_sq_batch_counter >= SQ_MAX_BATCH_TO_RING || cur_tsc - prev_tsc >= drain_tsc)) {
#endif

    ring_submission_doorbell();

#if (SQ_MAX_BATCH_TO_RING > 1)
    prev_tsc = cur_tsc;
    _sq_batch_counter = 0;
  }
#endif

#if COLLECT_STATS
  /* collect statistics */
  cpu_time_t delta = rdtsc() - start;
  if(_stat_mean_send_time_samples > 0) {
    _stat_mean_send_time_samples++;
    _stat_mean_send_time = ((_stat_mean_send_time*(_stat_mean_send_time_samples-1)) + delta) / _stat_mean_send_time_samples;
  }
  else {
    _stat_mean_send_time = delta;
  }
  if(delta > _stat_max_send_time)
    _stat_max_send_time = delta;
  if(delta > 2000) 
    _bad_count++;
  issued++;
#endif 

  return slot_id & 0xffff;
}




uint16_t NVME_IO_queue::issue_async_write(addr_t prp1, 
                                           off_t offset, 
                                           size_t num_blocks,
                                           bool sequential, 
                                           unsigned access_freq, 
                                           unsigned access_lat,
                                           unsigned nsid) 
{
  signed slot_id;
  Submission_command_slot * sc = next_sub_slot(&slot_id);
  assert(sc);

  Command_io_rw cmd(sc,
                    slot_id, /* command id */
                    nsid, /* device namespace */
                    prp1, 
                    offset, 
                    num_blocks, 
                    sequential, 
                    access_freq, 
                    access_lat,
                    true);
#if (SQ_MAX_BATCH_TO_RING > 1)
  cur_tsc = rdtsc();
  _sq_batch_counter++;
  if(_unlikely(_sq_batch_counter >= SQ_MAX_BATCH_TO_RING || cur_tsc - prev_tsc >= drain_tsc)) {
#endif

    ring_submission_doorbell();

#if (SQ_MAX_BATCH_TO_RING > 1)
    prev_tsc = cur_tsc;
    _sq_batch_counter = 0;
  }
#endif

  return slot_id;
}

uint16_t NVME_IO_queue::issue_async_io_batch(io_descriptor_t* io_desc,
                                              uint64_t length,
                                              Notify *notify
                                              )
{
  batch_info_t bi;
  memset(&bi, 0, sizeof(batch_info_t));
  assert(length > 0);
  assert(length < 0xffff); //otherwise, we run out of cmd ids. Keep it simple.
  // init the batch info block, and add it to the buffer
  // only one-time batch is considered now
  uint16_t start_cmdid = next_cmdid();
  uint16_t end_cmdid = next_cmdid(length);
  //PLOG("start_cmdid = %u, end_cmdid = %u", start_cmdid, end_cmdid);

  if(end_cmdid < start_cmdid) { //handling wrap-around
    PLOG("start_cmdid = %u, end_cmdid = %u", start_cmdid, end_cmdid);

    reset_cmdid();
    start_cmdid = next_cmdid();
    end_cmdid = next_cmdid(length);
  
    PLOG("Reset: start_cmdid = %u, end_cmdid = %u", start_cmdid, end_cmdid);
  }
  assert(end_cmdid >= start_cmdid);
  
#ifndef DISABLE_BATCH_MANAGER_FOR_TESTING
  //check availability
  NVME_LOOP( (!_batch_manager->is_available(start_cmdid, end_cmdid)), false);

  bi.start_cmdid = start_cmdid;
  bi.end_cmdid = end_cmdid;
  bi.total = length;
  bi.counter = 0;
  bi.notify = notify;
  bi.ready = true;
  bi.complete = false;

  //push to the buffer
  NVME_LOOP( (!(_batch_manager->push(bi))), false);
#endif

  //issue all IOs
  uint16_t cmdid = 0;
  assert(io_desc);

  for(int idx = 0; idx < length; idx++) {
    io_descriptor_t* io_desc_ptr = io_desc + idx;
    if(io_desc_ptr->action == NVME_READ) {
      cmdid = issue_async_read(io_desc_ptr->buffer_phys, 
                               io_desc_ptr->offset,
                               io_desc_ptr->num_blocks);

      PLOG("READ: issued cmdid = %u, offset = %lu(0x%lx), page_offset = %lu(0x%lx)\n", cmdid, io_desc_ptr->offset, io_desc_ptr->offset, io_desc_ptr->offset/8, io_desc_ptr->offset/8);

    } else if (io_desc_ptr->action == NVME_WRITE) {

      cmdid = issue_async_write(io_desc_ptr->buffer_phys, 
                                io_desc_ptr->offset,
                                io_desc_ptr->num_blocks);

      PLOG("WRITE: issued cmdid = %u, offset = %lu(0x%lx), page_offset = %lu(0x%lx)\n", cmdid, io_desc_ptr->offset, io_desc_ptr->offset, io_desc_ptr->offset/8, io_desc_ptr->offset/8);

    } else {
      PERR("Unrecoganized Operaton !!");
      assert(false);
    }
    //printf("issue cmdid = %u, offset = %lu(%lx, %lx)\n", cmdid, io_desc_ptr->offset, io_desc_ptr->offset, io_desc_ptr->offset/8);
  }

#ifndef DISABLE_BATCH_MANAGER_FOR_TESTING
  assert(cmdid == bi.end_cmdid);
#endif

}

status_t NVME_IO_queue::wait_io_completion()
{
  ring_submission_doorbell(); /* make sure the doorbell is rang */

#ifndef DISABLE_BATCH_MANAGER_FOR_TESTING
  NVME_LOOP( ( !(_batch_manager->wasEmpty()) ), false);
#endif

  return Exokernel::S_OK;
}

uint16_t NVME_IO_queue::issue_flush() 
{
  signed slot_id;
  Submission_command_slot * sc = next_sub_slot(&slot_id);
  assert(sc);

  Command_io_flush cmd(sc, slot_id, 1 /* nsid */);

  ring_submission_doorbell();

  return slot_id;
}
