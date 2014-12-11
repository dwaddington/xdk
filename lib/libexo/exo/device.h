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

#ifndef __EXO_DEVICE_H__
#define __EXO_DEVICE_H__

#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <string>
#include <pthread.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include "sysfs.h"

namespace Exokernel
{ 
  struct device_vendor_pair_t
  {
    unsigned vendor;
    unsigned device_id;
  };
    

  class Device;
  class Device : public Device_sysfs
  {
  private:
    unsigned _vendor;
    unsigned _device_id;

    //std::map<unsigned, FILE *> _cached_fp;
    typedef struct tag_fd_cache {
      FILE * handle;
      unsigned vector;
    } fd_cache_t;

    //std::vector<FILE *> _cached_fp[100];
    static __thread fd_cache_t _cached_fd;

  public:
    
    /** 
     * Constructor
     * 
     * @param instance Instance counting from zero
     * @param vendor Vendor id
     * @param device Device id
     */
    Device(unsigned instance, unsigned vendor, unsigned device)
    {
      
      if(!__locate_device(instance, vendor, device)) {
        throw Exokernel::Error_no_device();
      }

      _instance = instance;
      _vendor = vendor;
      _device_id = device;

      /* initialized inherited classes */
      Device_sysfs::init(_sys_fs_root_name, _sys_fs_pci_root_name);
    }


    /** 
     * Constructor (alternative)
     * 
     * @param instance Instance counting from zero
     * @param devices Table of device-vendor pairs
     */
    Device(unsigned instance, Exokernel::device_vendor_pair_t * devices) {
      
      unsigned i=0;
      while(devices[i].vendor > 0) {
        if(!__locate_device(instance, devices[i].vendor, devices[i].device_id)) {
          i++;
          continue;
        }
        else {
          _vendor = devices[i].vendor;
          _device_id = devices[i].device_id;
          break;
        }
      }

      _instance = instance;

      /* initialized inherited classes */
      Device_sysfs::init(_sys_fs_root_name, _sys_fs_pci_root_name);
    }

    /** 
     * RO accessors
     * 
     */
    unsigned vendor() const { return _vendor; }
    unsigned device_id() const { return _device_id; }


    /**------------------------------------------------------------- 
     * DMA contiguous memory management
     *-------------------------------------------------------------- 
    
     void * alloc_dma_pages(size_t num_pages, 
     addr_t * phys_addr, 
     unsigned numa_node, 
     int flags) = 0;

     void free_dma_pages(void * p)=0;

    */

    /**------------------------------------------------------------- 
     * MSI and MSI-X interrupt services (declared in base classes)
     *-------------------------------------------------------------- 
    
     status_t allocate_msi_vectors(unsigned qty, std::vector<unsigned>& vectors);
    */


    /** 
     * INTERRUPT HANDLING ROUTINES
     * 
     */


    /** 
     * Wait for a standard PCI interrupt. Blocks until IRQ is fired.
     * 
     */
    void wait_for_irq() {
      /* TO FIX AND OPTIMIZED WITH CACHED FD */
      std::ifstream fs;      
      std::string fname = _sys_fs_root_name + "/irq";
      fs.open(fname.c_str());
      
      std::getline(fs,fname);
      fs.close();
    }

    /** 
     * Wait for MSI based interrupt.  Blocks until IRQ is fired.
     * 
     * @param vector 
     */
    status_t wait_for_msi_irq(unsigned vector) {

      /* we use a cached open file handle if possible */
      if(_cached_fd.vector != vector) {

        std::stringstream ss;        
        ss << "/proc/parasite/" << _pk_device_name << "/msi-" << vector;
        
        if(_cached_fd.vector) {
          assert(_cached_fd.handle);
          fclose(_cached_fd.handle);
        }
        _cached_fd.vector = vector;
        _cached_fd.handle = fopen(ss.str().c_str(),"r+");
      }

      /* do read - don't forget its a string */
      char tmp[8];
      int ri;
      while(!fgets((char*) tmp,8,_cached_fd.handle)) {
        PLOG("retrying fread in wait_for_msix_irq ri=%d",ri);
      }
      assert((unsigned) atoi(tmp) == vector);
      return Exokernel::S_OK;
    }

    
    /** 
     * Wait for MSI-X based interrupt.  Blocks untils IRQ is fired.
     * 
     * @param vector 
     */
    status_t wait_for_msix_irq(unsigned vector) {

      /* we use a cached open file handle if possible */
      if(_cached_fd.vector != vector) {

        std::stringstream ss;        
        ss << "/proc/parasite/" << _pk_device_name << "/msix-" << vector;
        
        if(_cached_fd.vector) {
          assert(_cached_fd.handle);
          fclose(_cached_fd.handle);
        }
        _cached_fd.vector = vector;
        _cached_fd.handle = fopen(ss.str().c_str(),"r+");
        assert(_cached_fd.handle);
      }
      assert(_cached_fd.handle);
      /* do read - don't forget its a string */
      char tmp[8];
      int ri;
      while(!fgets((char *)tmp,8,_cached_fd.handle)) {
        PLOG("retrying fread in wait_for_msix_irq ri=%d",ri);
      }
      assert((unsigned) atoi(tmp) == vector);
      return Exokernel::S_OK;
    }

    /** 
     * Used to reenable an MSI-X interrupt after it has been masked due to
     * masking mode
     * 
     * @param vector 
     * 
     * @return 
     */
    status_t reenable_msix_irq(unsigned vector) {
      /* we use a cached open file handle if possible */
      if(_cached_fd.vector != vector) {

        std::stringstream ss;        
        ss << "/proc/parasite/" << _pk_device_name << "/msix-" << vector;
        
        if(_cached_fd.vector) {
          assert(_cached_fd.handle);
          fclose(_cached_fd.handle);
        }
        _cached_fd.vector = vector;
        _cached_fd.handle = fopen(ss.str().c_str(),"r+");
        assert(_cached_fd.handle);
      }
      assert(_cached_fd.handle);
      fputs("1\n",_cached_fd.handle);
      return Exokernel::S_OK;
    }

    /** 
     * Map physical memory previously allocated with alloc_dma_pages
     * 
     * @param paddr 
     * @param size 
     * 
     * @return 
     */
    void * remap_device_memory(addr_t paddr, size_t size) {

      /* do mmap into virtual address space using parasitic module */
      void * p;
      {
        int fd;
        fd = open("/dev/parasite",O_RDWR);
        if(!fd)
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open /dev/parasite");
        
        p = mmap(NULL,
                 size, 
                 PROT_READ|PROT_WRITE,// prot
                 MAP_SHARED, // flags
                 fd,
                 paddr);
        
        close(fd);
      }      
      assert(p);
      return p;
    }


  private:
  
    unsigned    _instance;             // device instance (defined by vendor,id pair) counting from 0
    std::string _sys_fs_root_name;     // root e.g., /sys/class/parasite/pk0
    std::string _sys_fs_pci_root_name; // pci root e.g., /sys/bus/pci/devices/0000:00:1f.2
    std::string _pk_device_name;       // PK device name, e.g., pk0
  
    bool __locate_device(unsigned instance, 
                         unsigned vendor, 
                         unsigned device);

  };  


  /** 
   * Basic PCI device
   * 
   */
  class Pci_device : public Device
  {
  public:
    Pci_device(unsigned instance, unsigned vendor, unsigned device) : 
      Device(instance, vendor, device) {
    }    
    Pci_device(unsigned instance, Exokernel::device_vendor_pair_t * a) :
      Device(instance, a) {
    }    

  };

  /** 
   * Class to support additional PCIe specific functionality
   * 
   * @param instance 
   * @param vendor 
   * @param device 
   * 
   * @return 
   */
  class Pci_express_device : public Device
  {
  private:
    struct PCI_status
    {
      uint16_t status;
      uint32_t bus_width;
      uint32_t bus_speed;
    } _pci_link;

    enum pcie_bus_width {
      pcie_bus_width_unknown = 0,
      pcie_bus_width_pcie_x1 = 1,
      pcie_bus_width_pcie_x2 = 2,
      pcie_bus_width_pcie_x4 = 4,
      pcie_bus_width_pcie_x8 = 8,
      pcie_bus_width_32      = 32,
      pcie_bus_width_64      = 64,
      pcie_bus_width_reserved
    };

    enum {
      pci_link_status_reg = 0xb2,
      pci_link_width_reg  = 0x3f0,
      pci_link_width_1 = 0x10,
      pci_link_width_2 = 0x20,
      pci_link_width_4 = 0x40,
      pci_link_width_8 = 0x80,
    };

  public:

    /** 
     * Sanity check to verify PCI link for this device
     * 
     */
    void check_pci_link() {
      /* Get the negotiated link width and speed from PCI config space */
      _pci_link.status = pci_config()->read16(pci_link_status_reg); // is this STANDARD?

      switch (_pci_link.status & pci_link_width_reg) {
      case pci_link_width_1:
        _pci_link.bus_width = pcie_bus_width_pcie_x1;
        break;
      case pci_link_width_2:
        _pci_link.bus_width = pcie_bus_width_pcie_x2;
        break;
      case pci_link_width_4:
        _pci_link.bus_width = pcie_bus_width_pcie_x4;
        break;
      case pci_link_width_8:
        _pci_link.bus_width = pcie_bus_width_pcie_x8;
        break;
      default:
        _pci_link.bus_width = pcie_bus_width_unknown;
        break;
      }

      PINF("PCIE BUS WIDTH       %s\n", (_pci_link.bus_width == pcie_bus_width_pcie_x8 ? 
                                         "Width x8" :
                                         _pci_link.bus_width == pcie_bus_width_pcie_x4 ? 
                                         "Width x4" :
                                         _pci_link.bus_width == pcie_bus_width_pcie_x1 ? 
                                         "Width x1" : "Unknown"));
    }


    /** 
     * Retrieve the offset for a given capability index
     * 
     * @param requested_id Capability index requested
     * 
     * @return Offset in PCI configuration space in bytes
     */
    int get_cap(unsigned requested_id) {

      uint8_t cap_ptr_offset = pci_config()->cap_pointer();
      uint8_t next_cap_id = 0;

      do {
        next_cap_id = pci_config()->read8(cap_ptr_offset);
        if(next_cap_id == requested_id) {
          return cap_ptr_offset;
        }
        cap_ptr_offset = pci_config()->read8(cap_ptr_offset+1);

      } while(next_cap_id > 0);
      
      PDBG("ERROR: get_cap request cap index not found!");
      return -1;
    }
      
  public:
    
    /** 
     * Constructor
     * 
     * @param instance Device instance counting from 0
     * @param vendor PCI vendor identifier
     * @param device PCI device identifier
     * 
     */
    Pci_express_device(unsigned instance, unsigned vendor, unsigned device) : Device(instance, vendor, device) {
      check_pci_link();
    }

    /** 
     * Constructor
     * 
     * @param instance Device instance counting from 0 (to support multiple of the same device)
     * @param a List of valid device-vendor pairs
     * 
     */
    Pci_express_device(unsigned instance, Exokernel::device_vendor_pair_t * a) :
      Device(instance, a) {
    }    

    
  };

}

#endif // __EXO_DEVICE_H__
