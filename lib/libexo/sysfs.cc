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
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#include <sys/types.h>
#include <sys/mman.h>

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sstream>
#include <errno.h>

#include <common/logging.h>
#include <common/utils.h>

#include "exo/sysfs.h"
#include "exo/memory.h"

/**********************************************************************************
 * Pci_config_space
 **********************************************************************************/

/** 
 * Constructor
 * 
 * @param[in] root_fs Name of root sysfs directory
 * 
 */
Exokernel::Device_sysfs::Pci_config_space::Pci_config_space(std::string& root_fs)

{
  const std::string config_node = root_fs + "/config";

  if(verbose)
    PLOG("Opening config space (%s)",config_node.c_str());

  _fd = open(config_node.c_str(), O_RDWR|O_SYNC);

  if(!_fd) {
    PWRN("unable to open procfs file (%s)", config_node.c_str());
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open PCI config space");
  }

  interrogate_bar_regions();
}

/** 
 * Determine the BAR region size by writing 1's into the BAR and then reading
 * the return value
 * 
 */
void Exokernel::Device_sysfs::Pci_config_space::interrogate_bar_regions()
{
  for(unsigned i=0;i<6;i++) {

    uint32_t bar = read32(PCI_BAR_0+i*4);
    if (bar > 0) {
      write32(PCI_BAR_0+i*4,~(0U));
      uint32_t ss_value = read32(PCI_BAR_0+i*4);
      ss_value &= ~0xFU;
      _bar_region_sizes[i] = ss_value & ~( ss_value - 1 );
      
      write32(PCI_BAR_0+i*4,bar);  // replace original
    } 
    else {
      _bar_region_sizes[i] = 0;
    }
  }
}

/**********************************************************************************
 * Pci_mapped_memory_region
 **********************************************************************************/

/** 
 * Constructor
 * 
 * @param root_fs Name of root file system.
 * @param index Index of the region counting from 0.
 * 
 */
Exokernel::Device_sysfs::Pci_mapped_memory_region::
Pci_mapped_memory_region(std::string& root_fs, 
                         unsigned index, 
                         uint32_t bar,
                         uint32_t bar_region_size) 
  : _index(index), 
    _bar_region_size(bar_region_size)
{
  _bar = bar & ~(0xF);

  /* do some sanity checks */
  assert(!(bar & (1U << 0)));   // memory address decoder
  _64bit = (((bar & 0x4) >> 1) == 2);  // 64-bit register width

  {
    std::ostringstream s;
    s << root_fs << "/resource" << index;

    PLOG("Opening PCI memory space (%s)",s.str().c_str());
    _fd = open(s.str().c_str(), O_RDWR);

    if(!_fd) {   
      PWRN("Unable to open PCI config space (%s)",s.str().c_str());
      throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open PCI mapped memory space");
    }

    _fdm = open(s.str().c_str(), O_RDWR|O_SYNC);
  }

  if(_fdm != -1) {
    _mapped_memory = mmap(NULL, //addr
                          bar_region_size,
                          PROT_READ|PROT_WRITE, //prot
                          MAP_SHARED, //flags
                          _fdm, 
                          0);  //offset; special meaning with sysfs mmap'able objects
    
    if(_mapped_memory == MAP_FAILED) {
      perror("map failed:");
    }
    else {
      PLOG("Mapped region%u to _mapped_memory %p", index, _mapped_memory);
    }
  }
  else {
    _mapped_memory = NULL;
  }

}


/**********************************************************************************
 * File_reader class 
 **********************************************************************************/

uint8_t Exokernel::Device_sysfs::Sysfs_file_accessor::read8(unsigned offset) 
{
  assert(_fd > 0);
  uint8_t val;

  if(pread(_fd,&val,1,offset) != 1) 
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to read8 PCI config space");

  return val;
}

uint16_t Exokernel::Device_sysfs::Sysfs_file_accessor::read16(unsigned offset) 
{
  assert(_fd > 0);
  uint16_t val;

  if(pread(_fd,&val,2,offset) != 2) 
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to read16 PCI config space");

  return val;
}

uint32_t Exokernel::Device_sysfs::Sysfs_file_accessor::read32(unsigned offset) 
{
  assert(_fd > 0);
  uint32_t val;

  if(pread(_fd,&val,4,offset) != 4) {
    PLOG("offset = %u",offset);
    perror("foobar:");
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to read32 PCI config space");
  }

  return val;
}

void Exokernel::Device_sysfs::Sysfs_file_accessor::write8(unsigned offset, uint8_t val) 
{
  assert(_fd > 0);
  
  if(pwrite(_fd,&val,1,offset) != 1)
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to write8 PCI config space");
}

void Exokernel::Device_sysfs::Sysfs_file_accessor::write16(unsigned offset, uint16_t val) 
{
  assert(_fd > 0);
  
  if(pwrite(_fd,&val,2,offset) != 2)
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to write16 PCI config space");
}

void Exokernel::Device_sysfs::Sysfs_file_accessor::write32(unsigned offset, uint32_t val) 
{
  assert(_fd > 0);

  if(pwrite(_fd,&val,4,offset) != 4) 
    throw Exokernel::Fatal(__FILE__,__LINE__,"unable to write16 PCI config space");
}


/**------------------------------------------------------------- 
 * Memory access through parasitic kernel
 *-------------------------------------------------------------- 
 * TODO: add serialization 
 */

/** 
 * Destructor
 * 
 */
Exokernel::Device_sysfs::
~Device_sysfs() {
  
  PDBG("~Device_sysfs dtor.");

  /* release DMA allocations */
  for(std::map<void *, memory_mapping_t *>::iterator i=_dma_allocations.begin();
      i!=_dma_allocations.end(); i++) {
    assert(i->second);

    /* free the DMA allocation */
    __free_dma_mapping(i->first, i->second);
    
    /* free the data structure */
    delete i->second;
  }

  /* release instantiated objects */
  if(_pci_config_space) 
    delete _pci_config_space;
  
  for(unsigned i=0;i<6;i++) {
    if(_mapped_memory[i]) 
      delete(_mapped_memory[i]);
  }
}

static void touch(void * addr, size_t size) {

  SUPPRESS_NOT_USED_WARN volatile byte b;

  // Touch memory to trigger page mapping.
  for(volatile byte * p = (byte*) addr; 
      ((unsigned long)p) < (((unsigned long)addr)+size); 
      p+=PAGE_SIZE) {
    b = *p; // R touch.
  }
}


/** 
 * Allocate contiguous pages for DMA
 * 
 * @param num_pages Number of pages to allocate
 * @param phys_addr [out] physical address
 * @param numa_node NUMA node identifier
 * @param flags Additional mmap flags
 * 
 * @return Virtual address of allocated memory
 */
void * 
Exokernel::Device_sysfs::
alloc_dma_pages(size_t num_pages, 
                addr_t * phys_addr, 
                void * virt_hint,
                int numa_node, 
                int flags) 
{
  try {

    /* first allocate physical pages */
    std::fstream fs;
    assert(!_fs_root_name.empty());
    std::string n = _fs_root_name;
          
    n += "/dma_page_alloc";
 
    fs.open(n.c_str());

    std::stringstream sstr;
    sstr << num_pages << " " << numa_node << std::endl;
    fs << sstr.str();

    /* Reset file pointer and read allocation results.  When we do
       a read on dma_page_alloc we get a list of allocations in the 
       form of owner, numa zone, order, physical address.  A list
       of all allocation belonging to the calling process is given.
       The list is truncated when the text string goes beyond 4K.
     */
    fs.seekg(0);
    int owner, numa, order;
    addr_t paddr;

    try {
      if(!(fs >> std::hex >> owner >> std::ws >> numa >> std::ws >> order >> std::ws >> std::hex >> paddr)) {
        if(errno  == 34) 
          PDBG("DMA allocation order invalid for kernel.");
        else
          PDBG("unknown error in dma_page_alloc (%d)",errno);
      }
    }
    catch(...) {
      PERR("unexpected data on (%s)",n.c_str());
      throw Exokernel::Fatal(__FILE__,__LINE__,"unexpected data from dma_page_alloc - ran out of pages?");
    }


    assert(paddr > 0);
    *phys_addr = paddr;

    /* do mmap into virtual address space using parasitic module */
    void * p;
    {
      int fd;
      fd = open("/dev/parasite",O_RDWR);
      if(fd == -1)
        throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open /dev/parasite");
      
      p = mmap(virt_hint,
               num_pages * PAGE_SIZE, 
               PROT_READ | PROT_WRITE, // prot
               MAP_SHARED | flags,
               fd,
               paddr);
        
      if(p==MAP_FAILED) {
        close(fd);
        PLOG("mmap failed. errno=0x%x",errno);
        throw Exokernel::Fatal(__FILE__,__LINE__,"mmap failed unexpectedly");
      }
      /* zero memory */
      //      memset(p,0,num_pages * PAGE_SIZE);

      /* touch pages */
      touch((void*)p,(size_t)(num_pages * PAGE_SIZE));

      assert(check_aligned(p,PAGE_SIZE));          
      close(fd);
    }


    /* TODO: store mapping for later free */
    assert(p);
    _dma_allocations[p] = new memory_mapping_t(paddr, num_pages * PAGE_SIZE, flags);
          
    return p;
  }
  catch(Exokernel::Fatal e) {
    throw e;
  }
  catch(...) {
    throw Exokernel::Fatal(__FILE__,__LINE__,
                           "unexpected condition in alloc_dma_pages");
  }
}

/**
 * Allocate contiguous physical memory and map to huge virtual pages
 * Note: must allocate huge pages in the system with
 * echo 100 > /proc/sys/vm/nr_hugepages
 * 
 * @param num_pages Number of pages (2MB each)
 * @param phys_addr Returned physical address
 * @param addr_hint Returned virtual address hint
 * @param numa_node Numa node to allocate from
 * @param flags Additional flags
 * 
 * @return Virtual address of allocated memory
 */
void * 
Exokernel::Device_sysfs::
alloc_dma_huge_pages(size_t num_pages, addr_t * phys_addr, void * addr_hint, int numa_node, int flags) 
{
  assert(!_fs_root_name.empty());

  try {

    /* first allocate physical pages */
    std::fstream fs;
    std::string n = _fs_root_name;
    n += "/dma_page_alloc";
 
    fs.open(n.c_str());

    std::stringstream sstr;
    /* we scale pages to 2MB huge pagers.  the kernel module works in 4K pages. */
    sstr << (num_pages * 512)  << " " << numa_node << std::endl;
    fs << sstr.str();

    /* reset file pointer and read allocation results */
    fs.seekg(0);
    int owner, numa, order;
    addr_t paddr;

    try {
      if(!(fs >> std::hex >> owner >> std::ws >> numa >> std::ws >> order >> std::ws >> std::hex >> paddr)) {
        if(errno  == 34) 
          PDBG("DMA allocation order invalid for kernel.");
        else
        PDBG("unknown error in dma_page_alloc (%d)",errno);
      }
    }
    catch(...) {
      PERR("unexpected data on (%s)",n.c_str());
      assert(0);
      throw Exokernel::Fatal(__FILE__,__LINE__,"unexpected data from dma_page_alloc - ran out of pages?");
    }


    assert(paddr > 0);
    *phys_addr = paddr;

    /* do mmap into virtual address space using parasitic module */
    void * p;
    {
      int fd;
      fd = open("/dev/parasite",O_RDWR);
      if(fd == -1)
        throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open /dev/parasite");
          
      p = mmap(addr_hint,
               num_pages * HUGE_PAGE_SIZE, 
               PROT_READ | PROT_WRITE, // prot
               MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB | flags,     // flags
               fd,
               paddr);
        
      if(p==MAP_FAILED) {
        close(fd);
        PLOG("mmap with MAP_HUGETLB failed. errno=%d",errno);
        throw Exokernel::Fatal(__FILE__,__LINE__,"huge page mmap failed unexpectedly");
      }
      assert(check_aligned(p,PAGE_SIZE));
          
      close(fd);
    }

    assert(p);
    _dma_allocations[p] = new memory_mapping_t(paddr, num_pages * HUGE_PAGE_SIZE, flags);
          
    return p;
  }
  catch(Exokernel::Fatal e) {
    PERR("exception %s",e.cause());
    throw e;
  }
  catch(...) {
    throw Exokernel::Fatal(__FILE__,__LINE__,
                           "unexpected condition in alloc_dma_huge_pages");
  }
}


/** 
 * Grant access to allocated memory to all other processes
 * 
 * @param phys_addr Physical address of allocated region
 * 
 * @return 
 */
status_t
Exokernel::Device_sysfs::
grant_dma_access(addr_t phys_addr) 
{
  assert(!_fs_root_name.empty());

  PINF("granting access ...");
  try {

    /* first allocate physical pages */
    std::fstream fs;
    std::string n = _fs_root_name;
    n += "/grant_access";
 
    fs.open(n.c_str());

    std::stringstream sstr;
    sstr << "0x" << std::hex << phys_addr << std::endl;
    fs << sstr.str();
  }
  catch(Exokernel::Fatal e) {
    PERR("exception %s",e.cause());
    throw e;
  }
  catch(...) {
    throw Exokernel::Fatal(__FILE__,__LINE__,
                           "unexpected condition in grant_dma_access");
  }

  PINF("grant_dma_access OK.");
  return S_OK;
}



/** 
 * For debugging purposes, fetch the DMA allocation
 * list for the calling process.
 * 
 * 
 * @return DMA allocation list.  Entries of the form ownerpid, numa, order, physaddr.
 */
std::string
Exokernel::Device_sysfs::
debug_fetch_dma_allocations()
{
  /* first allocate physical pages */
  std::fstream fs;
  assert(!_fs_root_name.empty());
  std::string n = _fs_root_name;
  
  n += "/dma_page_alloc";
  
  fs.open(n.c_str());

  std::string line,result;
  while(std::getline(fs, line)) {
    result += line;
    result += "\n";
  }

  return result;
}


/** 
 * Helper to call /dma_page_free to free DMA physical pages
 * 
 * @param phys_addr 
 * 
 * @return 
 */
void 
Exokernel::Device_sysfs::
__free_dma_physical(addr_t phys_addr)
{
  /* free physical pages */
  try {
    std::fstream fs;
    std::string n = _fs_root_name;
          
    n += "/dma_page_free";
        
    fs.open(n.c_str(),std::ofstream::out);

    std::stringstream sstr;
    assert(phys_addr);
    sstr << "0x" << std::hex << phys_addr << std::endl;
    fs << sstr.str();

    fs.close();
  }
  catch(...) {
    throw Exokernel::Fatal(__FILE__,__LINE__,"unexpected condition in free_dma_pages (physical page free failed)");
  }
}

void 
Exokernel::Device_sysfs::
__free_dma_mapping(void * vptr, memory_mapping_t * mm) 
{
  /* free virtual pages */
  if(munmap(vptr, mm->_length))
    throw Exokernel::Fatal(__FILE__,__LINE__,
                           "unexpected condition in free_dma_pages (virtual page munmap failed)");      

  /* free physical */
  __free_dma_physical(mm->_phys_addr); 
}


void 
Exokernel::Device_sysfs::
free_dma_pages(void * vptr) 
{
  /* look up memory mapping */
  memory_mapping_t * mm = _dma_allocations[vptr];
  if(mm == NULL)
    throw Exokernel::Exception("free_dma_pages called with invalid parameter");

  __free_dma_mapping(vptr, mm);
}


void 
Exokernel::Device_sysfs::
free_dma_huge_pages(void * vptr) 
{
  /* look up memory mapping */
  memory_mapping_t * mm = _dma_allocations[vptr];
  if(mm == NULL)
    throw Exokernel::Exception("free_dma_huge_pages called with invalid parameter");

  assert(mm->_length % MB(2) == 0);

  __free_dma_mapping(vptr, mm);
}


/**------------------------------------------------------------- 
 * MSI-X interrupt services
 *-------------------------------------------------------------- 
 */

void
Exokernel::Device_sysfs::
irq_set_masking_mode(bool masking)
{
  std::string n = _fs_root_name;
  n += "/irq_mode";

  std::ofstream fs;
  fs.open(n.c_str());

  std::stringstream sstr;
  if(masking)
    sstr << 2 << std::endl; /* interrupts will be masked */
  else
    sstr << 1 << std::endl; /* interrupts will be unmasked */

  fs << sstr.str();
  
  fs.close();
}

/** 
 * Allocate N MSI-X interrupt vectors for the device.  This allocation
 * will establish entries in /proc/parasite/pkXXX/<vector>
 * 
 * @param qty 
 * @param vectors 
 * 
 * @return 
 */
status_t 
Exokernel::Device_sysfs::
allocate_msi_vectors(unsigned qty, std::vector<unsigned>& vectors)
{
  if (qty == 0) 
    return E_INVALID_REQUEST;

  vectors.clear();
  
  try
  {
    std::string n = _fs_root_name;
    n += "/msi_alloc";
   
    {
      std::ofstream fs;
      fs.open(n.c_str());
      
      /* write out <N> */
      std::stringstream sstr;
      sstr << qty << std::endl;
      fs << sstr.str();
      fs.close();
    }

    /* read allocation results */
    {
      std::ifstream fs;
      fs.open(n.c_str());
      int vector = -1;
      
      while(fs >> std::dec >> vector) {
        
        assert(vector > 0);
        
        vectors.push_back(vector);
      }
      fs.close();
    }
  }
  catch(...) {
    return E_FAIL;
  }

  return S_OK;
}


status_t
Exokernel::Device_sysfs::
query_msi_vectors(std::vector<unsigned>& vectors)
{
  std::string n = _fs_root_name;
  n += "/msi_alloc";

  std::ifstream fs;
  fs.open(n.c_str());
  int vector = -1;
  
  while(fs >> std::dec >> vector) {
    
    assert(vector > 0);
        
    vectors.push_back(vector);
  }
  fs.close();
  return Exokernel::S_OK;
}
 
