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

#ifndef __AHCI_DEVICE_H__
#define __AHCI_DEVICE_H__

#include <stdio.h>
#include <libexo.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>

#include "ahci_hw.h"
#include "ahci_port.h"
#include "debug.h"

/** 
 * Devices that this driver works with
 * 
 */
static Exokernel::device_vendor_pair_t dev_tbl[] = {{0x8086,0x3a22}, // Intel ICH
                                                    {0x8086,0x2829}, // VirtualBox
                                                    {0x8086,0x2922}, // Qemu
                                                    {0,0}};


class AHCI_uddk_device : public Exokernel::Pci_device {

  enum Pci_config {
    AHCI_BASE_ID        = 0x5,  /* resource id of AHCI base addr <BAR 5> */
    AHCI_INTR_OFF       = 0x3c, /* offset of interrupt information in config space */
    AHCI_PORT_BASE      = 0x100,
    AHCI_DEV_MAX_PORTS  = 32,
  };

  ahci_ghc_t * _host_ctrl; /* client side allocation for generic host control */
  Pci_mapped_memory_region * _mmio;
  unsigned _num_ports;
  Ahci_device_port * _ports[AHCI_DEV_MAX_PORTS]; /* available instances of port devices */
  unsigned _irq;
  
public:

  /** 
   * Constructor
   * 
   */
  AHCI_uddk_device(unsigned instance = 0) : 
  Exokernel::Pci_device(instance, dev_tbl) { 

    _mmio = pci_memory_region(5);

    init_device();
  }

  ahci_ghc_t * ctrl() const { return _host_ctrl; }

  void enable_msi() {

    enum { PM_CAP_OFF = 0x34, /* power management cap */
           MSI_CAP = 0x5, 
           MSI_ENABLED = 0x1 };

    /* Read the power management cap
     */
    uint8_t cap = _mmio->mmio_read8(PM_CAP_OFF);

    /* Iterate through cap pointers. The next capability is bits 15:08 of PMCAP see section 2.2.1 
     */
    for (uint16_t val = 0; cap; cap = val >> 8) { 

      val = _mmio->mmio_read8(cap);

      if ((val & 0xff) != MSI_CAP)
        continue;                                

      uint8_t msi = _mmio->mmio_read8(cap + 2);
        
      if (!(msi & MSI_ENABLED)) { /* bit 0 of MSIMC */
        PLOG("Enabling MSI!!!");
        _mmio->mmio_write8(cap+2,msi | MSI_ENABLED);
      }
    }
    
    PLOG("MSI flags OK.");
  }

  void reset_controller()
  {
    /* configure PCS */
    if(pci_config()->vendor() == 0x8086) {

      //      uint16_t tmp16;
      //      tmp16 = pci_config()->read16(0x92);
      // enable all of the ports
      pci_config()->write16(0x92,0xFFFF);
      
    }
  }

  std::vector<unsigned> _msi_vectors;
  
  /** 
   * Allocate MSI interrupt for the device
   * 
   */
  void setup_msi_interrupt()
  {
    allocate_msi_vectors(1,_msi_vectors);
    _irq = _msi_vectors[0];
  }

  /** 
   * Initialize the device.
   * 
   */
  void init_device();

  /** 
   * Check for Native Command Queuing support
   * 
   */
  bool check_cap_ncq() {
    return (_host_ctrl->cap & (1 << 30));
  }

  /** 
   * Show the device capabilities
   * 
   */  
  void cap_show()
  {
    uint32_t caps = _host_ctrl->cap;
    //assert(caps > 0);
    AHCI_INFO("caps (%x)\n",caps);
    AHCI_INFO("\tPort count: %u\n", (caps & 0x1f) + 1);
    AHCI_INFO("\tCommand slots: %u\n", ((caps >> 8) & 0x1f) + 1);
    AHCI_INFO("\tAHCI only: %s\n", (caps & 0x20000) ? "yes" : "no");
    AHCI_INFO("\tNative command queuing: %s\n", (caps & 0x40000000) ? "yes" : "no");
    AHCI_INFO("\t64 Bit: %s\n", (caps & 0x80000000) ? "yes" : "no");
    AHCI_INFO("\tActivity LED: %s\n", (caps & 0x2000000) ? "yes" : "no");
    AHCI_INFO("\tSpeed LED (%x)\n", (caps & 0xE00000) >> 20);
    AHCI_INFO("\tCCC support: %s\n", (caps & (1 << 7)) ? "yes" : "no");

    if(!(caps & 0x40000000)) {
      PERR("[AHCI]: NCQ must be supported.");
      exit(0);
    }
  }

  /** 
   * Scan through ports and initialize active ones
   * 
   * @param irq 
   * 
   * @return 
   */
  status_t scan_ports(unsigned irq)
  {
    assert(_host_ctrl);


    _num_ports = (_host_ctrl->cap & 0x1f) + 1;
    AHCI_INFO("Ports implemented = 0x%x\n",_host_ctrl->pi);
    AHCI_INFO("Number of ports  = %u\n",_num_ports);

    if(_num_ports == 0) {
      panic("[AHCI]: error - no ports found on AHCI controller!!!\n");
      return Exokernel::E_FAIL;
    }

    assert(sizeof(ahci_port_t)*_num_ports < (PAGE_SIZE *2)); /* check if we have mapped enough in */

    ahci_port_t *ports = (ahci_port_t *)((char *)_host_ctrl + AHCI_PORT_BASE);
    assert(ports);

    for (unsigned i = 0; i < _num_ports; i++) {    
      _ports[i] = new Ahci_device_port(i,this,(void*)&ports[i],irq);
      _ports[i]->init(); /* initialize port */
    }

    AHCI_INFO("finished scanning ports.\n");
    return Exokernel::S_OK;
  }


  Ahci_device_port * port(unsigned i) const {
    assert(i < AHCI_DEV_MAX_PORTS);
    assert(_ports[i] != NULL);
    return _ports[i]; /* available instances of port devices */
  }

};

#endif // __AHCI_DEVICE_H__
