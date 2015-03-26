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
  Author(s):
  @author Jilong Kuang (jilong.kuang@samsung.com)
*/

#ifndef _MEM_ITF_H_
#define _MEM_ITF_H_

#include "../base.h"
#include <common/types.h>
#include <common/cpu_mask.h>

namespace Component
{
  /**
   * Slab allocator IDs.
   */
  enum {
    DESC_ALLOCATOR        = 0,  /**< The network ring descriptor allocator. */
    PACKET_ALLOCATOR      = 1,  /**< The packet buffer allocator. */
    MBUF_ALLOCATOR        = 2,  /**< The mbuf struct allocator. */
    META_DATA_ALLOCATOR   = 3,  /**< The meta data allocator for an array of burst packets. */
    NET_HEADER_ALLOCATOR  = 4,  /**< The network header allocator (42 bytes). */
    PBUF_ALLOCATOR        = 5,  /**< The pbuf allocator for IP packet meta data. */
    IP_REASS_ALLOCATOR    = 6,  /**< The IP reassembly data buffer allocator. */
    UDP_PCB_ALLOCATOR     = 7,  /**< The UDP protocol control block allocator. */
    // Always at the end  
    TOTAL_ALLOCATOR_NUM         /**< Total number of allocators. */
  };
  
  
  /** Returns a string representation of a given slab allocator ID. */
  static inline 
  const char* slab_alloc_2_str(int a) {
    // Note: Keep this function in sync with the constants above. 
    if      (a == DESC_ALLOCATOR)      { return "Ring Desc. Allocator"; }
    else if (a == PACKET_ALLOCATOR)    { return "Packet Buffer Allocator"; }
    else if (a == MBUF_ALLOCATOR)      { return "MBUF Allocator"; }
    else if (a == META_DATA_ALLOCATOR)      { return "Meta Data Allocator"; }
    else if (a == PBUF_ALLOCATOR)      { return "PBUF Allocator"; }
    else if (a == NET_HEADER_ALLOCATOR) { return "Net Header Allocator"; }
    else if (a == IP_REASS_ALLOCATOR)  { return "IP Reass. Buffer Allocator"; }
    else if (a == UDP_PCB_ALLOCATOR)   { return "UDP Control Block Allocator"; }
    else { return "Unknown slab allocator"; }
  }
  
  
  /** Indices of values in an Shm_table's entry for a slab allocator. */
  enum {
    /** Index of the starting virtual address. */
    VIRT_ADDR_ALLOC_ENTRY_VAL_IDX = 0,  
    /** Index of the shared memory key. */
    SHM_KEY_ALLOC_ENTRY_VAL_IDX = 1,
    /** Index of the shared memory length. */
    SHM_LEN_ALLOC_ENTRY_VAL_IDX = 2,
    /** NUMA node. */
    NUMA_NODE_ENTRY_VAL_IDX = 3,
    /** Total number of indices allocators. */
    MAX_ALLOC_ENTRY_VAL_IDX
  };
  
  /** Shared memory table types and subtypes shared by NIC driver and app */
  enum {
    SMT_MEMORY_AREA    = 0,
    SMT_CHANNEL_NAME   = 1,
  };
  
  enum {
    SMT_DESC_ALLOCATOR       = 0,      /**< The network ring descriptor allocator. */
    SMT_PACKET_ALLOCATOR     = 1,      /**< The packet buffer allocator. */
    SMT_MBUF_ALLOCATOR       = 2,      /**< The mbuf struct allocator. */
    SMT_META_DATA_ALLOCATOR  = 3,      /**< The meta data allocator for an array of burst packets. */
    SMT_NET_HEADER_ALLOCATOR = 4,      /**< The network header allocator (42 bytes). */
    SMT_PBUF_ALLOCATOR       = 5,      /**< The pbuf allocator for IP packet meta data. */
    SMT_IP_REASS_ALLOCATOR   = 6,      /**< The ip reassembly data buffer allocator */
    SMT_UDP_PCB_ALLOCATOR    = 7,      /**< The UDP protocol control block allocator */
  };
  
  /**
   * Memory component state type.
   */
  enum {  
    MEM_INIT_STATE = 0,    /**< The initial state. */
    MEM_READY_STATE = 1,   /**< The state when memory componet is up running. */
  };
  
  /**
   * Slab allocator configuration struct.
   */
  typedef struct {
      unsigned block_size;       /**< The block size. */
      unsigned num_blocks;       /**< The total number of blocks. */
      unsigned alignment;        /**< The block alignment. */
      allocator_t allocator_id;  /**< The allocator ID. */
  } alloc_config_t;
  
  /**
   * Memory component init argument struct.
   */
  typedef struct {
    unsigned num_allocators;        /**< The number of slab allocators managed by the memory component. */
    alloc_config_t ** config_list;  /**< An array of pointers to the configuration struct for each slab allocator. */
    params_config_t params;         /**< The params config struct */
  } mem_arg_t;
    
  enum { MAX_MEM_INSTANCE = 4 };


  /** 
   * Interface definition for IMem.
   * 
   */
  class IMem 
  {
  state_t  _comp_state[MAX_MEM_INSTANCE];

  public:
    DECLARE_INTERFACE_UUID(0xb7343bf4,0x0636,0x47b4,0xb3d2,0xe0,0x72,0x52,0x03,0xce,0x45);
  
    /** 
      * To allocate a memory block from a given allocator.
      *
      * @param p The pointer to the address of a newly allocated memory block.
      * @param id The allocator ID.
      * @param device The device ID.
      * @param core The cpu core ID.
      * @return The return status.
      */
    virtual status_t alloc(addr_t *p, allocator_t id, unsigned device, core_id_t core) = 0;
  
    /**
      * To allocate a memory block with size n (NOT IMPLEMENTED YET).
      *
      * @param n The size of the memory block.
      * @return The pointer to the newly allocated memory block.
      */
    virtual void * alloc(size_t n) = 0;
  
    /**
      * To free a memory block (NOT IMIPLEMENTED YET).
      *
      * @param p The pointer to the memory block to be freed.
      * @return The return status.
      */
    virtual status_t free(void * p) = 0;
  
    /**
      * To free a memory block back to a given allocator.
      *
      * @param p The pointer to the memory block to be freed.
      * @param id The allocator ID.
      * @param device The device ID.
      * @return The return status.
      */
    virtual status_t free(void * p, allocator_t id, unsigned device) = 0;
  
    /**
      * To obtain the physical address of a memory block allocated from a given allocator.
      *
      * @param virt_addr The virtual address of that memory block.
      * @param id The allocator ID.
      * @param device The device ID.
      * @return The physical address.
      */
    virtual addr_t get_phys_addr(void *virt_addr, allocator_t id, unsigned device) = 0;
  
    /**
      * To obtain the available blocks in an allocator per core.
      *
      * @param id The allocator ID.
      * @param device The device ID.
      * @param core The cpu core ID.
      * @return The available block number per core.
      */
    virtual uint64_t get_num_avail_per_core(allocator_t id, unsigned device, core_id_t core) = 0;
  
    /**
      * To obtain the total available blocks in an allocator.
      *
      * @param id The allocator ID.
      * @param device The device ID.
      * @return The total available block number.
      */
    virtual uint64_t get_total_avail(allocator_t id, unsigned device) = 0;
    
    /**
      * To obtain the allocator handle.
      *
      * @param id The allocator ID.
      * @param device The device ID.
      * @return The allocator handle.
      */
    virtual allocator_handle_t get_allocator(allocator_t id, unsigned device) = 0;
   
    /** 
     * To initialize the component interface.
     *
     * @param arg A struct pointer for configuration arguments.
     * @return The return status.
     */
    virtual status_t init(arg_t arg) = 0;

    /** 
     * To specify the cpu allocation for this interface.
     *
     * @param mask The CPU mask to indicate which CPUs are used.
     * @return The return status.
     */
    virtual status_t cpu_allocation(cpu_mask_t mask) = 0;

    /** 
     * To start the interface.
     */
    virtual void run() = 0;

    /** 
     * To obtain the state of component.
     *
     * @return The runtime state of current component.
     */
    state_t get_comp_state(unsigned idx) {
      return _comp_state[idx];
    }

    /** 
     * To set the component state.
     *
     * @param s The component state to be set.
     */
    void set_comp_state(state_t s, unsigned idx) {
      _comp_state[idx] = s;
    }
  };
}
#endif
