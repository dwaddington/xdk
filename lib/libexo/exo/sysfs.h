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

#ifndef __EXO_SYSFS_H__
#define __EXO_SYSFS_H__

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vector>
#include <map>

#include <common/utils.h>
#include <common/types.h>
#include <common/logging.h>

#include "errors.h"
#include "pci.h"
#include <private/__pci_config.h>

namespace Exokernel
{

  class Device_sysfs {

  public:

    /** 
     * Generic class to support mmap'ed memory from sysfs
     * 
     */
    class Sysfs_mmap_accessor {

    protected:
      void * _mapped_memory;
      
    public:

      /** 
       * Constructor
       * 
       */
      Sysfs_mmap_accessor() : _mapped_memory(NULL) {	
      }

      void * mmap_ptr() const {
        assert(_mapped_memory);
        return _mapped_memory;
      }

      /** 
       * Mapped read memory accessors
       * 
       */
      uint8_t mmio_read8(uint32_t reg) {
        return *((volatile uint8_t *)(((addr_t)_mapped_memory+reg)));
      }

      uint8_t mmio_read16(uint32_t reg) {
        return *((volatile uint16_t *)(((addr_t)_mapped_memory+reg)));
      }

      uint32_t mmio_read32(uint32_t reg) {
        return *((volatile uint32_t *)(((addr_t)_mapped_memory+reg)));
      }

      uint64_t mmio_read64(uint32_t reg) {
        return *((volatile uint64_t *)(((addr_t)_mapped_memory+reg)));
      }

      /** 
       * Mapped write memory accessors
       * 
       */
      void mmio_write8(uint32_t reg, uint8_t val) {
        *((volatile uint8_t *)(((addr_t)_mapped_memory+reg))) = val;
      }

      void mmio_write16(uint32_t reg, uint16_t val) {
        *((volatile uint16_t *)(((addr_t)_mapped_memory+reg))) = val;
      }
      
      void mmio_write32(uint32_t reg, uint32_t val) {
        *((volatile uint32_t *)(((addr_t)_mapped_memory+reg))) = val;
      }

      void mmio_write64(uint32_t reg, uint64_t val) {
        *((volatile uint64_t *)(((addr_t)_mapped_memory+reg))) = val;
      }
      
    };

    /** 
     * Generic class to support offset-based R/W from a file handle
     * 
     */
    class Sysfs_file_accessor {

    protected:
      int    _fd, _fdm;
      
    public:

      /** 
       * Return the size of the file
       * 
       * @return Size of file in bytes
       */
      off_t file_size() {
        struct stat file_info;
        if(fstat(_fd, &file_info)) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to fstat PCI region");
        return file_info.st_size;
      }

      /** 
       * Read accessors
       * 
       */
      uint8_t  read8(unsigned offset);
      uint16_t read16(unsigned offset);
      uint32_t read32(unsigned offset);

      /** 
       * Write accessors
       * 
       */
      void     write8(unsigned offset, uint8_t val);
      void     write16(unsigned offset, uint16_t val);
      void     write32(unsigned offset, uint32_t val);
    };


    /** 
     * Class to map PCI memory mapped regions.  This is
     * exposed by the kernel PCI subsystem through the 
     * resourceXXX node.
     * 
     * @param root_fs Root sysfs node for this device
     */
    class Pci_mapped_memory_region : public Sysfs_file_accessor,
                                     public Sysfs_mmap_accessor
    {
    private:
      unsigned _index;
      uint32_t _bar;
      uint32_t _bar_region_size;
      bool     _64bit;

    public:
      /** 
       * Constructor
       * 
       * @param root_fs Root node for PCI device in sysfs
       * @param index Counting from 0 index
       * @param bar_region_size Size of region as probe from PCI BAR
       */
      Pci_mapped_memory_region(std::string& root_fs, 
                               unsigned index, 
                               uint32_t bar, 
                               uint32_t bar_region_size);

      /** 
       * Dump information about memory region
       * 
       */
      void dump_memory_region_info() {
        PLOG("Memory region %d: %x size %lu bytes (probed size = %u)",_index, _bar, file_size(), _bar_region_size);
      }

      /** 
       * Return Base Address Register (BAR)
       * 
       */
      uint32_t bar() const {
        return _bar;
      }

    }; // nested class

  private:
    struct memory_mapping_t  {
      addr_t   _phys_addr;
      size_t   _length;
      int      _flags;
      
      memory_mapping_t(addr_t p, size_t s, int f) : 
        _phys_addr(p), _length(s), _flags(f) {
      }

    };

  private:

    Pci_config_space *           _pci_config_space;

    Pci_mapped_memory_region *   _mapped_memory[6];
    std::string                  _fs_root_name;

    std::map<void *, memory_mapping_t *> _dma_allocations;

    void __free_dma_physical(addr_t phys_addr);
    void __free_dma_mapping(void * vptr, memory_mapping_t * mm);



  public:

    /** 
     * Constructor
     * 
     */
    Device_sysfs() : _pci_config_space(NULL) {
      __builtin_memset(_mapped_memory,0,sizeof(_mapped_memory));
    }


    /** 
     * Post-constructor initialization routine
     * 
     * @param pk_root 
     * @param pci_root 
     * @param auto_map Whether to automically map 32bit bar registers (if > 0xffff)
     */    
    void init(std::string& pk_root, std::string& pci_root, bool automap=true) {

      _fs_root_name = pk_root;

      /* instantiate PCI configuration space memory mapping */
      _pci_config_space = new Pci_config_space(pk_root);

      /* instantiate Mapped Memory mappings pointed to by each BAR */
      for(unsigned i=0;i<6;i++) {
        uint32_t b = _pci_config_space->bar(i);

        /* exclude port IO space */
        if(automap && (b > 0xFFFF)) {
          _mapped_memory[i] = new Pci_mapped_memory_region(pci_root,
                                                           i,
                                                           b,
                                                           _pci_config_space->bar_region_size(i));
        }
        else {
          _mapped_memory[i] = NULL;
        }
      }
    }

    /** 
     * Destructor
     * 
     */
    ~Device_sysfs();

    /**------------------------------------------------------------- 
     * DMA contiguous memory management
     *-------------------------------------------------------------- 
     */

    enum dma_direction_t {
      DMA_BIDIRECTIONAL = 0,
      DMA_TO_DEVICE = 1,
      DMA_FROM_DEVICE = 2,
      DMA_NONE = 3,
    };

    /** 
     * Allocate physically contiguous memory and map with 4K TLB entries
     * 
     * @param num_pages Number of 4K pages to allocate
     * @param phys_addr Return physical address
     * @param virt_hint Request mapping to this virtual address if possible
     * @param numa_node NUMA node from which to allocate memory
     * 
     * @return Virtual address of allocated memory
     */
    void * alloc_dma_pages(size_t num_pages, 
                           addr_t * phys_addr, 
                           dma_direction_t direction = DMA_BIDIRECTIONAL,
                           void * virt_hint = NULL,
                           int numa_node = -1,
                           int flags = 0);

    /** 
     * Free memory allocated with alloc_dma_pages
     * 
     * @param p Pointer to previously allocated memory
     */
    void free_dma_pages(void * p);


    /** 
     * Allocate physically contiguous memory and map with 2MB TLB (huge page) entries
     * 
     * @param num_pages Number of 2MB pages
     * @param phys_addr Return physical address
     * @param addr_hint Return virtual address hint
     * @param numa_node NUMA node from which to allocate memory
     * 
     * @return Virtual address of allocated memory
     */
    void * alloc_dma_huge_pages(size_t num_pages, 
                                addr_t * phys_addr,
                                void * addr_hint = NULL,
                                int numa_node = -1,
                                int flags = 0); 

    /** 
     * Free memory allocated with alloc_dma_huge_pages
     * 
     * @param p Pointer to previously allocated memory
     */
    void free_dma_huge_pages(void * p);


    /** 
     * Grant all processes access to allocated memory
     * 
     * @param phys_addr Physical address of memory to grant access to
     * 
     * @return 
     */
    status_t grant_dma_access(addr_t phys_addr);

    /** 
     * For debugging purposes, fetch the DMA allocation
     * list for the calling process.
     * 
     * 
     * @return DMA allocation list.  Entries of the form ownerpid, numa, order, physaddr.
     */
    std::string debug_fetch_dma_allocations();


    /**----------------------------------------------------------- 
     * MSI and MSI-X interrupt services
     *-------------------------------------------------------------- 
     */

    /** 
     * Allocate MSI vectors for the device.  Call defaults to MSI-X
     * in precedence to MSI.  This method can only be called once
     * on the device.
     * 
     * @param qty Number of vectors to allocate
     * @param vectors [inout] List of allocated vectors
     * 
     * @return S_OK on success
     */
    status_t allocate_msi_vectors(unsigned qty, std::vector<unsigned>& vectors);

    /** 
     * Set IRQ handling to mask the interrupt in the kernel; this requires
     * explicit clearing of the mask through a subsequent write to the 
     * irq handle in /proc. A call to this will mask all interrupts
     * currently allocated to the device.
     * 
     */
    void irq_set_masking_mode(bool masking = true);

    /** 
     * Lookup previously allocated MSI/MSI-X vectors on a device
     * 
     * @param vectors [inout] List of allocated vectors
     * 
     * @return S_OK on success
     */
    status_t query_msi_vectors(std::vector<unsigned>& vectors);

    /** 
     * Get hold of the PCI configuration block for the device.
     * 
     * 
     * @return Pci_config_space wrapper which has accessor methods 
     *         for the PCI configuration space
     */
    INLINE Pci_config_space * const pci_config() {
      return _pci_config_space;
    }

    /** 
     * Get hold of memory regions defined by PCI BAR registers
     * 
     * @param index BAR index
     * 
     * @return Pointer to wrapper class for accessing mapped region
     */
    INLINE Pci_mapped_memory_region * const pci_memory_region(unsigned index) {
      if(index > 5) return NULL;
      else return _mapped_memory[index];
    }
  };


  typedef Device_sysfs::Pci_mapped_memory_region Pci_region;
}

#endif // __EXO_SYSFS_H__

