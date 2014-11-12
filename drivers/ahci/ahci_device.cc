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

#include "ahci_device.h"

/** 
 * Initialize the device.
 * 
 */
void AHCI_uddk_device::init_device() {

  AHCI_INFO("Initializing device...\n");

  /* get the AHCI base address */
  addr_t abar = pci_config()->bar(5);
    

  /* map in Generic host control HBA memory registers */
  addr_t addr_to_map = abar & (~0xfff);
  _host_ctrl = (ahci_ghc_t *)pci_memory_region(5)->mmap_ptr();

  //    _host_ctrl = (ahci_ghc_t *) Exokernel::map_io_memory(addr_to_map, 512);
  assert(_host_ctrl);

  AHCI_INFO("IO memory mapped (bar=%lx)\n",addr_to_map);

  /* indicate AHCI aware, but interrupts disabled */
  _host_ctrl->ghc |= AHCI_GHC_GHC_AE;

  /* disable command completeion coalescing (CCC) */
  ahci_ghc_ccc_ctl_t ccc;
  ccc.u32 = _host_ctrl->ccc_ctl;
  ccc.en = 0;
  _host_ctrl->ccc_ctl = ccc.u32;

  AHCI_INFO("CCC disabled.");

  /* enable message signalled interrutps */
  enable_msi();

  /* set master latency timer */
  //  _mmio->mmio_write8(AHCI_PCI_MLT,64);

  /* enable interrupts and AHCI mode */
  _host_ctrl->is = ~0; /* clear interrupts */
  _host_ctrl->ghc |= AHCI_GHC_GHC_AE | AHCI_GHC_GHC_IE;
    
  if(_host_ctrl->cap & (1<<27))
    AHCI_INFO("Staggered spin up not implemented!\n");
  else
    AHCI_INFO("Stagger spin off.\n");
    
  PLOG("cap show");
  cap_show();

  reset_controller();

  setup_msi_interrupt();
    
  scan_ports(_irq);
}

