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

#include "nvme_command_admin.h"
#include "nvme_device.h"

Command_admin_base::~Command_admin_base() {
  if(_cid) {
    _queues->release_slot(_cid-1);
  }
}


Single_page_command::Single_page_command(NVME_admin_queue * q) : _q(q) {

  assert(q);

  /* allocate prp */
  _prp1 = q->device()->alloc_dma_pages(1,&_prp1_phys);
  assert(_prp1);
  memset(_prp1,0,PAGE_SIZE);
}

Single_page_command::~Single_page_command() {
  if(_prp1)
    _q->device()->free_dma_pages(_prp1);
}


/** 
 * IDENTIFY COMMAND -------------------------------------------------
 * 
 */

/** 
 * Constructor
 * 
 * @param slot 
 * @param dev 
 */
Command_admin_identify::Command_admin_identify(NVME_admin_queue * q, 
                                               int nsid) : 
  Command_admin_base(q)
{
  signed slot_id;
  Submission_command_slot * sc = _queues->next_sub_slot(&slot_id);
  assert(sc);
  sc->clear();

  /* set up command */
  sc->opcode = nvme_admin_identify;

  if(nsid == 0) { /* perform identify for controller */
    sc->cdw10 = 1; 
  }
  else { /* perform identify for namespace */
    sc->cdw10 = 0; 
    sc->nsid = nsid;
  }
  sc->prp1 = _prp1_phys;

  /* assign unique command identifier */
  _cid = sc->command_id = slot_id;
}




/** 
 * Check the result for a namespace level identify
 * 
 * @param dev 
 * 
 * @return 
 */
status_t Command_admin_identify::check_ns_result(NVME_device * dev, unsigned nsid)
{
  assert(nsid > 0);

  /* check for command completion */
  _queues->check_command_completion(_cid);

  nvme_id_ns * id = (nvme_id_ns *) _prp1;
  NVME_INFO("[NID(%u):NSIZE] max %lu sectors (%lu GB)\n",nsid,id->nsze,REDUCE_GB(id->nsze*512));
  NVME_INFO("[NID(%u):NCAP] max %lu sectors\n",nsid,id->ncap);
  NVME_INFO("[NID(%u):NUSE] used %lu sectors (%lu GB)\n",nsid,id->nuse,REDUCE_GB(id->nuse*512));

  /* store NS information */  
  NVME_device::ns_info * info = new NVME_device::ns_info();
  info->_nsze = id->nsze;
  info->_ncap = id->ncap;

  uint8_t nlbaf = id->nlbaf;
  NVME_INFO("nlbaf = %u\n",id->nlbaf);
  NVME_INFO("flbas = %u\n",id->flbas);

  for(unsigned i=0;i<id->nlbaf;i++) {
    NVME_INFO("LBAF ms=%u ds=%u rp=%u\n",
         id->lbaf[i].ms,         
         id->lbaf[i].ds,
         id->lbaf[i].rp);
  }

  dev->_ns_ident.push_back(info);
  
  return Exokernel::S_OK;
}

/** 
 * Check the result for a controller level identify
 * 
 * @param dev 
 * 
 * @return 
 */
status_t Command_admin_identify::check_controller_result(NVME_device * dev)
{
  /* check for command completion */
  _queues->check_command_completion(_cid);

  nvme_id_ctrl * id = (nvme_id_ctrl *) _prp1;
  NVME_INFO("[ID:VID]   0x%x\n",id->vid);
  NVME_INFO("[ID:SSVID] 0x%x\n",id->ssvid);
  NVME_INFO("[ID:SN]    %s\n",id->sn);
  NVME_INFO("[ID:MN]    %s\n",id->mn);
  NVME_INFO("[ID:MIC]   %u\n",id->mic);
  NVME_INFO("[ID:MDTS]  %u\n",id->mdts);
  NVME_INFO("[ID:NPSS]  %u\n",id->npss);
  NVME_INFO("[ID:SQES]  %u\n",id->sqes);
  assert(((id->sqes >> 4) & 0xf) == 6); // 64 bytes
  assert((id->sqes & 0xf) == 6); // 64 bytes
  NVME_INFO("[ID:CQES]  %u\n",id->cqes);
  assert(((id->cqes >> 4) & 0xf) == 4); // 16 bytes
  assert((id->cqes & 0xf) == 4); // 16 bytes
  NVME_INFO("[ID:NN]    %u\n",id->nn);
  {
    uint16_t oacs = id->oacs;
    NVME_INFO("Supports firmware active/download : %s\n", (oacs & 0x4) ? "yes":"no"); 
    NVME_INFO("Supports format command :           %s\n", (oacs & 0x2) ? "yes":"no"); 
    NVME_INFO("Supports security send/recv :       %s\n", (oacs & 0x1) ? "yes":"no"); 
  }
  NVME_INFO("Supports fused operations :         %s\n", (id->fuses & 0x1) ? "yes":"no"); 

  /* store device information */
  dev->_ident._vid = id->vid;
  dev->_ident._ssvid = id->ssvid;
  memcpy(dev->_ident._firmware_rev, id->fr,8);
  dev->_ident._oacs = id->oacs;
  dev->_ident._nn = id->nn;
  
  return Exokernel::S_OK;
}



/** 
 * GET FEATURES COMMAND --------------------------------------------------
 * 
 */

Command_admin_get_features::Command_admin_get_features(NVME_admin_queue * q, 
                                                       feature_t fid, 
                                                       unsigned nsid) :
  Command_admin_base(q)
{
  signed slot_id;
  Submission_command_slot * sc = _queues->next_sub_slot(&slot_id);
  assert(sc);
  sc->clear();

  struct nvme_features * f = (struct nvme_features *) sc->raw();

  f->opcode = nvme_admin_get_features;
  f->prp1 = _prp1_phys;
  f->nsid = nsid;
  f->fid = fid;

  /* assign unique command identifier */
  sc->command_id = _cid = slot_id;
}

status_t Command_admin_get_features::extract_info(NVME_device::ns_info * nsinfo)
{
  /* check for command completion */
  _queues->check_command_completion(_cid);

  Completion_command_slot * cc = _queues->curr_comp_slot();  
  uint64_t * p = (uint64_t *) _prp1;

  unsigned num_ranges = cc->dw0 & 0x3f;
  PLOG("num_ranges = %u\n",num_ranges);
  // for(unsigned range=0;range < cc->
}



/** 
 * SET FEATURES COMMAND --------------------------------------------------
 * 
 */

Command_admin_set_features::Command_admin_set_features(NVME_admin_queue * q) 
  : Command_admin_base(q)
{
  signed slot_id;
  _sc = _queues->next_sub_slot(&slot_id);
  assert(_sc);
  _sc->clear();

  struct nvme_features * f = (struct nvme_features *) _sc->raw();
  f->opcode = nvme_admin_set_features;

  /* assign unique command identifier */
  _sc->command_id = _cid = slot_id;
}

status_t Command_admin_set_features::configure_num_queues(uint32_t num_queues)
{
  assert(_sc);
  struct nvme_features * f = (struct nvme_features *) _sc->raw();
  f->dword11 = ((num_queues-1) << 16) | (num_queues-1);
  f->fid = NVME_FEAT_NUM_QUEUES; 

  return Exokernel::S_OK;
}


status_t Command_admin_set_features::configure_interrupt_coalescing(bool turn_on, vector_t vector)
{
  assert(_sc);
  struct nvme_features * f = (struct nvme_features *) _sc->raw();
  f->fid = NVME_FEAT_IRQ_CONFIG;
  f->dword11 = (((uint32_t)vector) & 0xffff);

  if(!turn_on) 
    f->dword11 |= (1 << 16);

  return Exokernel::S_OK;
}


status_t Command_admin_set_features::configure_interrupt_coalescing_time(unsigned time, unsigned threshold)
{
  assert(_sc);
  struct nvme_features * f = (struct nvme_features *) _sc->raw();
  f->fid = NVME_FEAT_IRQ_COALESCE;
  f->dword11 = (((uint32_t)threshold) & 0xff) | ((((uint32_t)time) & 0xff) << 8);

  return Exokernel::S_OK;
}


/** 
 * CREATE IO QUEUE -------------------------------------------------------------
 * 
 */

Command_admin_create_io_sq::Command_admin_create_io_sq(NVME_admin_queue * q, 
                                                       unsigned queue_id,
                                                       size_t queue_size,
                                                       addr_t prp1,
                                                       unsigned cq_id,
                                                       queue_priority_t priority)
  : Command_admin_base(q)
{
  signed slot_id;
  Submission_command_slot * sc = _queues->next_sub_slot(&slot_id);
  assert(sc);
  sc->clear();

  assert(prp1);
  
  struct nvme_create_sq * c = (struct nvme_create_sq *) sc->raw();
  assert(c);

  c->opcode = nvme_admin_create_sq;
  c->prp1 = prp1;
  c->qsize = queue_size - 1;
  c->sqid = queue_id;
  c->cqid = cq_id;
  c->sq_flags = (((uint32_t)priority & 0x3)<<1) | 0x1;

  /* assign unique command identifier */
  sc->command_id = _cid = slot_id;
}



Command_admin_create_io_cq::Command_admin_create_io_cq(NVME_admin_queue * q, 
                                                       vector_t vector,
                                                       unsigned queue_id,
                                                       size_t queue_items,
                                                       addr_t prp1)
  : Command_admin_base(q)
{
  signed slot_id;
  Submission_command_slot * sc = _queues->next_sub_slot(&slot_id);
  assert(sc);
  sc->clear();

  assert(prp1);
  
  struct nvme_create_cq * c = (struct nvme_create_cq *) sc->raw();
  assert(c);

  c->opcode = nvme_admin_create_cq;
  c->prp1 = prp1;
  c->qsize = queue_items - 1;
  c->cqid = queue_id;
  c->irq_vector = vector; /* vector is logical */
  c->cq_flags = 0x3;      /* IEN and PC are set */

  PLOG("CREATE CQ size=%u qid=%u logical vector=%u",c->qsize, c->cqid, c->irq_vector);
  
  /* assign unique command identifier */
  sc->command_id = _cid = slot_id;
}



Command_admin_delete_io_queue::Command_admin_delete_io_queue(NVME_admin_queue * q, 
                                                             NVME::queue_type_t type,
                                                             unsigned queue_id)
  : Command_admin_base(q)
{
  signed slot_id;
  Submission_command_slot * sc = _queues->next_sub_slot(&slot_id);
  assert(sc);
  sc->clear();

  struct nvme_delete_queue * c = (struct nvme_delete_queue *) sc->raw();  
  assert(c);

  c->opcode = (type == NVME::QTYPE_SUB) ? nvme_admin_delete_sq : nvme_admin_delete_cq; 
  c->qid = queue_id;

  /* assign unique command identifier */
  sc->command_id = _cid = slot_id;
}


/** 
 * FORMAT  -------------------------------------------------------------
 * 
 */
Command_admin_format::Command_admin_format(NVME_admin_queue * q, 
                                           unsigned lbaf,
                                           unsigned nsid)
  : Command_admin_base(q)
{
  signed slot_id;
  Submission_command_slot * sc = _queues->next_sub_slot(&slot_id);
  assert(sc);
  sc->clear();

  struct nvme_format_cmd * c = (struct nvme_format_cmd *) sc->raw();  
  assert(c);

  c->opcode = nvme_admin_format_nvm;
  c->cdw10 = lbaf;
  c->nsid = nsid;

  /* assign unique command identifier */
  sc->command_id = _cid = slot_id;
}
