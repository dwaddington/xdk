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






#include "nvme_device.h"

#undef CONFIG_FORMAT_ON_INIT
#define CONFIG_IRQ_COAL

/** 
 * Destructor
 * 
 */
NVME_device::~NVME_device() {

  if(_regs)
    delete _regs;


  for(unsigned i=0;i<_ns_ident.size();i++) {
    assert(_ns_ident[i]);
    delete _ns_ident[i];
  }

  /* get rid of admin queues last */
  if(_admin_queues)
    delete _admin_queues;
}


/** 
 * HW reset and bring-up
 * 
 */
void NVME_device::hw_reset() {

  /* disable device */
  _regs->cc_disable(); 
  while(_regs->csts_ready()) {
    usleep(100000);
    NVME_INFO("waiting for disable...\n");
  }

  /* create admin queues */
  if(_admin_queues)
    delete _admin_queues;
  
  assert(_msi_vectors[0] > 0);
  _admin_queues = new NVME_admin_queues(this, _msi_vectors[0]); 
  _admin_queues->setup_doorbells();

  /* configure CC */
  _regs->cc(0x460001);
  
  /* re-enabled device */
  _regs->cc_enable();

  NVME_INFO("waiting for controller to ready...\n");

  unsigned attempts = _regs->timeout(); /* timeout value is in 500ms units */
  while(!_regs->csts_ready() && (attempts > 0))  {
    usleep(50000);
    attempts--;
  }

  if(!_regs->csts_ready())
    throw Exokernel::Exception("Failed to initialize controller\n");

  assert(_regs->csts() == 0x1);

}

/** 
 * Initialize the device
 * 
 */
void NVME_device::init_device() {

  /* map in the memory mapped device registers */
  _mmio = pci_memory_region(0);
  _regs = new NVME_registers(_mmio);

  /* sanity check registers */
  _regs->check();

  /* allocate vectors, one per IO queue +1 for admin */
  _num_io_queues = _config.num_io_queues();
  NVME_INFO("Num IO queues:%u\n",_num_io_queues);
  unsigned num_vectors = _num_io_queues + 1;

  /* allocate MSI vectors */ 
  _msi_vectors.clear();
  allocate_msi_vectors(num_vectors,_msi_vectors);
  NVME_INFO("allocated MSI-X vector to admin queue:%u\n",_msi_vectors[0]);

  /* set up steering */
  for(unsigned i=1;i<num_vectors;i++) {
    NVME_INFO("allocated MSI-X vector to IO queue:   %u\n",_msi_vectors[i]);

    /* route interrupt to appropriate core */
    Exokernel::route_interrupt(_msi_vectors[i],_config.get_core(i-1));
  }

  /* reset and bring up device */
  hw_reset();

  /* set number of queues */
  _admin_queues->set_queue_num(_num_io_queues);

  if(vendor()!=0x8086) { /* exclude QEMU */
    /* get PCIe MSI and MSIX capabilities - debugging purposes */
    {
      int msicap = get_cap(0x5);
      uint16_t mc = pci_config()->read16(msicap+0x2);
      NVME_INFO("MSI Capabilities (0x%x)\n",mc);
      NVME_INFO(" PVM  :      %s\n", mc & (1<<8) ? "yes" : "no");
      NVME_INFO(" 64bit:      %s\n", mc & (1<<7) ? "yes" : "no");
      NVME_INFO(" MMC:        %d\n", (mc >> 1) & 0x7);
      NVME_INFO(" MME:        %d\n", (mc >> 4) & 0x7);
      NVME_INFO(" MSI Enable: %s\n", mc & 0x1 ? "yes" : "no");
    }
    {
      int msixcap = get_cap(0x11);
      uint16_t mc = pci_config()->read16(msixcap+0x2);
      NVME_INFO("MSIX Capabilities (0x%x)\n",mc);
      NVME_INFO(" FM:              %s\n", mc & (1<<14) ? "yes" : "no");
      NVME_INFO(" MSIX Enable:     %s\n", mc & (1<<15) ? "yes" : "no");
      assert(mc & (1<<15));
    }
  }

  /* collect device information */
  _admin_queues->issue_identify_device();

#ifdef CONFIG_FORMAT_ON_INIT
  /* format disk */
  _admin_queues->issue_format(2);
#endif

  NVME_INFO("Creating %u IO queues (size=%u)....\n",
            _num_io_queues, 
            _config.get_io_queue_len());

  // BUG CAUSING BAD COMPLETION HEAD
  //_admin_queues->feature_info();


#ifdef CONFIG_IRQ_COAL
  _admin_queues->set_irq_coal_options(2, /* 10 * 100ms */
                                      1); /* min # of completion entries to coalesce */
#endif

  unsigned io_queue_len = _config.get_io_queue_len();

  /* limited IO queue length */
  if(io_queue_len >= _regs->mqes()) {
    io_queue_len = _regs->mqes() + 1;
    NVME_INFO("Reduced IO queue length from configuration to HW limit (%u)\n", 
              io_queue_len);
  }

  /* create IO queues */
  for(unsigned i=0;i<_num_io_queues;i++) {
    assert(i < _msi_vectors.size());
    
    core_id_t core;
    unsigned qid = _config.io_queue_get_assigned_core(i,&core);

    NVME_INFO("Assigning queue id (%u) to core (%u)\n",qid,core);

    NVME_IO_queues * ioq = 
      new NVME_IO_queues(this,
                         qid,
                         _msi_vectors[0],   /* base vector as logical is needed */
                         _msi_vectors[i+1], /* vector for this queue */
                         core,              /* affinity for cq thread */
                         io_queue_len);     /* length of queue in iterms */

    ioq->setup_doorbells();
    ioq->start_cq_thread();

    assert(ioq);
    _io_queues[i] = ioq;

#ifdef CONFIG_IRQ_COAL
    if(vendor() != 0x8086) {
      /* turn on IRQ coalescing */
      _admin_queues->set_irq_coal(true,_msi_vectors[i+1]);
    }
#endif
  }

}

/** 
 * First-stage shutdown before deconstruction of the object
 * 
 */
void NVME_device::destroy() {  
  for(unsigned i=0;i<_num_io_queues;i++) {
    assert(_io_queues[i]);
    delete _io_queues[i];
  }
}
