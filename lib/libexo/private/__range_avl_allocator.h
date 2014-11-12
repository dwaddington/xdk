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
  Copyright (C) 2011, Daniel G. Waddington <daniel.waddington@acm.org>
  Copyright (C) 2011, Chen Tian
*/

#ifndef __EXO_VIRT_ALLOCATOR_H__
#define __EXO_VIRT_ALLOCATOR_H__

#include <types.h>
#include <list.h>
#include "avl_allocator.h"


/** Virtual memory region. */
class Virt_Region : public AVL_Node<Virt_Region> {
public:
  addr_t addr; //>! region's starting address.  
  size_t size; //>! region's length. 
  size_t max_free; 
  bool used;

/*
 * The flag 'is_lazy' and the list 'map' are used to maintain virt-phys mappings.
 * Here is how it works:
 *
 * 1. Eager mapping: set is_lazy to false. That means the allocated 
 * virtual memory must be backed by physical memory. Page alignment 
 * is required. 
 *
 * 2. Lazy mapping: set is_lazy to true. That means an allocated virtual 
 * memory region does not need a real mapping until it is touched. 
 * The Page-Fault Handler should call set_mapping function to record the actual mapping.
 * Two typical use cases:
 *
 * A) A user-level memory allocator (e.g., malloc) requries an arena to play.
 * In this case, page alignment should be enforced by the user-level memory allocator.
 * As a result, each virtual region (i.e., a node in the AVL tree) is composed of 
 * certain number of pages. Set_mapping function simply adds a physical page to virtual 
 * page mapping upon a memory access.  It is also up to the user-level 
 * memory allocator to decide when to remove the mapping.
 * This is similar to anonymous mmap in Linux to some extent.
 * 
 *
 * B) An application directly call this allocator through env()->alloc(). 
 * In this case, page alignment is not required. This means it is possible 
 * to have one virtual page to be distributed across mulitple regions. 
 * A PF is triggered when any region is first accessed. The mapping is
 * inserted into the head region, i.e., the region that covers the address 
 * rounding down to a page aligned one (addr & ~(0xFFF)). It is removed only 
 * when all regions that has a piece of the page are conslidated into the head 
 * region and the head region is not in use. This can be checked by using 
 * containment search. This is not implemented in the system yet, because
 * we currently only allow users to use user-level memory allocator
 * for better performance.
 *
 * Chen Tian
 *
 */

  /** Physical-to-virtual memory mapping. */ 
  struct Mapping : public List_element<Mapping> {
    addr_t virt; //>! Virtual address. 
    addr_t phys; //>! Physical address.

    /** Constructor.*/
    Mapping(addr_t v, addr_t p) : virt(v), phys(p) {};
  };


  /** 
   * Flags that indicates if the region is used with lazy mapping (true) 
   * or eager mapping (false).
   */
  bool is_lazy; // debugging purpose

  /** List that stores the physical-to-virtual memory mappings. */
  List<Mapping> map;
    
  enum { USED = true, FREE = false };

  /** Default constructor. */
  Virt_Region(): addr(0), size(0), max_free(0), used(FREE) { }

  /** 
   * Constructor. 
   * @param a starting address. 
   * @param s size. 
   * @param u flag that indicates whether the region is in use. 
   */
  Virt_Region(addr_t a, size_t s, bool u): addr(a), size(s), used(u) {}

  /** Returns true if the memory region is free; false it is in use. */
  bool is_free() { return (used == FREE); }

  /** */
  bool higher(Virt_Region* n) { return (addr > n->addr); }

  /** */
  Virt_Region* left() { return static_cast<Virt_Region*> (subtree[LEFT]); }

  /** */
  Virt_Region* right() { return static_cast<Virt_Region*> (subtree[RIGHT]); }

  /** */
  Virt_Region* find_free_region(size_t size);

  /** */
  Virt_Region* find_free_region(size_t size, int alignment);

  /** */
  bool size_fit(size_t size, int alignment);

  /** 
   * Finds a region that contains the region of interest [a, a + s).
   * @param a starting address region of interest. 
   * @param s length of the region of interest.
   */
  Virt_Region* find_containment(addr_t a, size_t s);

  /** 
   * Finds a region that overlaps the region of interest [a, a + s).
   * @param a starting address region of interest. 
   * @param s length of the region of interest.
   */
  Virt_Region* find_overlap(addr_t a, size_t s);

  /** 
   * Finds a region with the given start address.
   * @param a starting address region of interest. 
   */
  Virt_Region* find_match(addr_t a);

  /** Updates the maximum amount of free memory under this region. */
  size_t update_max_free_size();

  /** */
  size_t total_size();

  /** */
  void dump();


  /** */
  void insert_mapping(bool lazy, Mapping* m) { is_lazy = lazy; map.insert(m);} 

  /** */
  List<Mapping>* get_mapping_list() { return &map; }
};



/** Type definition for the Virtual Memory Allocator. */
typedef class AVL_allocator<Virt_Region> Virt_allocator;

#endif 
