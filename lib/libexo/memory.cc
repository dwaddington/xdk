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

#include <fcntl.h>
#include <assert.h>
#include <numaif.h>

#include <string.h>
#include <errno.h>

#include "exo/memory.h"
#include "exo/pagemap.h"
#include "exo/device.h"

/** 
 * Mapping table for IO memory mappings
 * 
 */
static std::map<void *, size_t> __vm_mapping_size_table;
static std::map<void *, size_t> __alloc_pages_mappings;

/**
 * Mapping table for huge memory allocations
 *
 */
static std::map<void *, size_t> __huge_page_allocations;

/** 
 * Alloc an area of memory using huge pages (2MB)
 * 
 * @param size Size of allocation in bytes
 * 
 * @return Pointer to newly allocated memory
 */
void * Exokernel::Memory::huge_malloc(size_t alloc_size) {

  void * p = mmap(NULL, 
                  alloc_size, 
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB, 
                  -1, 0);

  assert(p);
  __huge_page_allocations[p] = alloc_size;

  return p;
}


/** 
 * Map in a previously allocate huge page allocation
 *
 * WARNING: this implementation is not using huge pages
 * to perform the mapping - fixed needed.
 *
 * @param size Size in bytes
 * @param paddr Physical address of previous allocation
 * 
 * @return 
 */
void * Exokernel::Memory::huge_mmap(size_t size, addr_t paddr) {

  void * p;
  {
    int fd;
    fd = open("/dev/parasite",O_RDWR);
    if(fd == -1)
      throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open /dev/parasite");
                  
    p = mmap(NULL,
             size, 
             PROT_READ | PROT_WRITE, // prot
             MAP_SHARED,     // flags
             fd,
             paddr);
        
    if(p==MAP_FAILED) {
      close(fd);
      throw Exokernel::Fatal(__FILE__,__LINE__,"huge_mmap failed unexpectedly");
    }
          
    close(fd);
  }

  return p;
}


/** 
 * Free a huge page allocation
 * 
 * @param ptr Pointer to memory area originally allocated through huge_malloc
 * 
 * @return E_NOT_FOUND, munmap error code or (0 on success, -1 on failure)
 */
int Exokernel::Memory::huge_free(void * ptr) {

  std::map<void*,size_t>::iterator i = __huge_page_allocations.find(ptr);

  if(i == __huge_page_allocations.end())
    return E_NOT_FOUND;

  size_t allocation_size = __huge_page_allocations[ptr];
  __huge_page_allocations.erase(i);

  return munmap(ptr,allocation_size);
}



/** 
 * Allocate N 4K pages and return physical address
 * 
 * @param num_pages Number of pages to allocate
 * @param phys_addr [out] Physical address
 * 
 * @return Virtual address
 */
/* WARNING issue!!! this will not allocate contiguous pages as is */
void * Exokernel::Memory::alloc_page(addr_t* phys_addr) 
{
  /* on-demand static initialization - to do handle ctor errors */
  static Exokernel::Pagemap __page_map;

  void * ptr = mmap(NULL, 
                    PAGE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | MAP_NORESERVE | MAP_POPULATE,
                    -1, 0);

  if(ptr == MAP_FAILED) {
    PWRN("alloc_pages mmap failed");
    perror("alloc_pages mmap failed:");
    return NULL;
  }

  __alloc_pages_mappings[ptr] = PAGE_SIZE;
  *phys_addr = __page_map.virt_to_phys(ptr);
  assert(phys_addr > 0);

  return ptr;

} __attribute__((deprecated))

/** 
 * Free all pages associated with a previous alloc_pages call
 * 
 * @param ptr Virtual pointer previously returned by alloc_pages
 * 
 * @return 0 on success, -1 on error
 */
int Exokernel::Memory::free_page(void * p)
{
  size_t s = __alloc_pages_mappings[p];
  if(s == 0) {
    PWRN("free_pages: cannot locate virtual address in mapping table");
    return -1;
  }
  return munmap(p,s);
}


/** 
 * Configure the number of huge pages in the system
 * 
 * @param pages 
 * 
 * @return 
 */
status_t Exokernel::Memory::huge_system_configure_nrpages(size_t pages)
{
  try {
    std::ofstream fs;
      
    fs.open("/proc/sys/vm/nr_hugepages");  
    fs << pages << std::endl;
    fs.close();
  }
  catch(...) {
    throw Exokernel
      ::Exception("Unable to open parasite dir (/proc/sys/vm/nr_hugepages)");

    return Exokernel::E_FAIL;
  }    

  return Exokernel::S_OK;
}



Exokernel::Slab_allocator::Slab_allocator(Device * device, size_t bs, size_t n) : 
  _device(device), _block_size(bs), _num_entries(n)
{
  assert(_num_entries > 0);
  _taken = new bool[_num_entries];
  size_t s = Exokernel::Memory::align_to_page(_block_size * _num_entries) / PAGE_SIZE;
  //  _virt_base = (byte *) alloc_pages(s,&_phys_base);
  _virt_base = (byte *) _device->alloc_dma_pages(s,&_phys_base);
  assert(_virt_base);
  assert(_phys_base);
  __builtin_memset(_taken,0,sizeof(bool[_num_entries]));
}

Exokernel::Slab_allocator::~Slab_allocator() {
  _device->free_dma_pages(_virt_base);
  delete _taken;
}



void Exokernel::Memory::huge_shmem_set_region_max(size_t size)
{
  try {
    std::ofstream fs;
    fs.open("/proc/sys/kernel/shmmax");
    fs << size << std::endl;
    fs.close();
  }
  catch(...) {
    throw Exokernel
      ::Exception("Unable to set shmmax");
  }
}

void Exokernel::Memory::huge_shmem_set_system_max(size_t size)
{
  try {
    std::ofstream fs;
    fs.open("/proc/sys/kernel/shmall");
    fs << size << std::endl;
    fs.close();
  }
  catch(...) {
    throw Exokernel
      ::Exception("Unable to set shmmax");
  }
}


void * Exokernel::Memory::huge_shmem_alloc(key_t key, 
                                           size_t size, 
                                           unsigned numa_node, 
                                           int * handle, 
                                           void * shmaddr_hint)
{
  assert(handle);

  int shmid;
  int flags = SHM_HUGETLB | SHM_R | SHM_W | IPC_CREAT;

  if(key == 0) {
    PERR("key must be greater than zero.");
    throw Exokernel::Exception("bad key");
  }

  if(size < HUGE_PAGE_SIZE) {
    PERR("size too small");
    throw Exokernel::Exception("size too small");
  }

  if((shmid = shmget(key, size, flags))==-1) {
    PERR("shmget(...) failed. Reason: %s", strerror(errno));
    throw Exokernel::Exception("shmget call failed in huge_shmem_alloc");    
  }

  void * shmaddr;
  shmaddr = shmat(shmid, shmaddr_hint,0);
  if(shmaddr == (void*)-1) {
    shmctl(shmid, IPC_RMID, NULL);    
    throw Exokernel::Exception("shmat call failed in huge_shmem_attach");    
  }
  
  struct shmid_ds dsinf;
  int rc = shmctl(shmid, IPC_STAT, &dsinf);
  assert(rc != -1);

#if MBIND_IS_IMPLEMENTED
  {
    /* set NUMA policy for the allocated rang */
    unsigned long node_mask = 0x1;

    rc = mbind(shmaddr, 
               dsinf.shm_segsz, 
               MPOL_BIND,
               &node_mask,
               sizeof(unsigned long) * 8,
               MPOL_MF_STRICT);
    if(rc) {
      perror("mbind failed");
      throw Exokernel::Exception("shmat call failed in huge_shmem_attach");    
    }
  }
#else
  {
    numa_tonode_memory(shmaddr, dsinf.shm_segsz,numa_node);
    /* touch memory to trigger page mapping */
    touch_huge_pages(shmaddr, dsinf.shm_segsz);
  }
#endif
  *handle = shmid;                               
  return shmaddr;
  
}


void * Exokernel::Memory::huge_shmem_alloc_at_core(key_t key, size_t size, core_id_t core, int * handle, void * shmaddr_hint)
{
  return huge_shmem_alloc(key,size,numa_node_of_cpu(core),handle,shmaddr_hint);
}

void * Exokernel::Memory::huge_shmem_attach(key_t key, 
                                            size_t size, 
                                            void * shmaddr_hint)
{

  int shmid;
  int flags = SHM_HUGETLB | SHM_R | SHM_W;

  if(key == 0) {
    PERR("key must be greater than zero.");
    throw Exokernel::Exception("bad key");
  }

  if(size < HUGE_PAGE_SIZE) {
    PERR("size too small");
    throw Exokernel::Exception("size too small");
  }

  /* get hold of existing allocation */
  if((shmid = shmget(key, size, flags))==-1) {
    PLOG("key=%d",key);
    throw Exokernel::Exception("shmget call failed in huge_shmem_attach");    
  }

  /* attach virtual memory space */
  void * shmaddr = shmat(shmid, shmaddr_hint, 0);
  if(shmaddr == (void*)-1)
    throw Exokernel::Exception("shmat call failed in huge_shmem_attach");    

  assert(shmaddr);
  return shmaddr;
}


void Exokernel::Memory::huge_shmem_free(void * shmaddr, int shmid)
{
  if(shmdt(shmaddr)==-1) 
    throw Exokernel::Exception("shmctl call failed in huge_shmem_free");    

  if(shmctl(shmid, IPC_RMID, NULL)==-1)
    throw Exokernel::Exception("shmctl call failed in huge_shmem_free");    
}


void Exokernel::Memory::huge_shmem_detach(void * shmaddr)
{
  if(shmdt(shmaddr)==-1) 
    throw Exokernel::Exception("shmctl call failed in huge_shmem_free");    
}

