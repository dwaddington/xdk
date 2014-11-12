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
  @author Daniel Waddington (d.waddington@samsung.com)
  @date: Oct 1, 2012
*/

#ifndef __EXT2_BLOCK_H__
#define __EXT2_BLOCK_H__

#include <types.h>
#include <list.h>
#include <range_tree.h>
#include <file/types.h>

#include "block_device.h"
#include "ext2_superblock.h"
#include "ext2fs.h"
#include "byteorder.h"


namespace Ext2fs
{
  /** 
   * Class to manage a list of block addresses
   * 
   */
  class Block_address_list;
  class Block_address_list : public List_element<Block_address_list>
  {
  private:

    block_address_t * _addresses;
    unsigned          _length;

  public:

    Block_address_list(unsigned block_size) {      
      assert((block_size % sizeof(block_address_t))==0);
      _length = block_size / sizeof(block_address_t);
      _addresses = (block_address_t *) malloc(block_size);
      __builtin_memset(_addresses,0,block_size);
    }

    ~Block_address_list() {
      free(_addresses);
    }
    
  public:

    void * raw() { return (void *) _addresses; }
    block_address_t * raw_array() const { return _addresses; }
    block_address_t * list() const { return _addresses; }
    block_address_t get_block_address(unsigned index) { assert(index < _length); return _addresses[index]; }
    unsigned len() const { return _length; }

    unsigned count_blocks() const { 
      unsigned count = 0;
      for(unsigned i=0;i<_length;i++) {
        if(_addresses[i] > 0)
          count++;
      }
      return count;
    }            
      
      
    void dump() 
    {
      bool contig = false;
      block_address_t last = _addresses[0];
      unsigned i=0;
      info("Dump->\n");
      /* dump ranges to reduce output */
      for(;i<_length;i++) {
        if(!contig) {
          if(_addresses[i] > 0) {
            info("Blocks: [%u-",_addresses[i]);
            contig=true;
          }
        }
        if(i>0) {
          if(_addresses[i] == (last + 1)) {
            contig=true;
          }
          else {
            if(_addresses[i] > 0) {
              info("%u]\n",_addresses[i]);
              contig=false;
            }
          }
        }
        if(_addresses[i] > 0) 
          last = _addresses[i];
      }
      if(contig && (_addresses[i] > 0)) {
        info("%u]\n",last);
      }        
    }

  };


  typedef struct tag_block_offset_pair {
    unsigned long _block_index;
    aoff64_t _offset;
  } block_offset_pair_t;

  /** 
   * Class used to manage groups of block address lists
   * 
   */
  class Block_address_collective
  {
  private:
    List<Block_address_list> _list;
    unsigned                 _list_count;

  public:

      
    Block_address_collective() : _list_count(0) {
    }

    ~Block_address_collective() {
      List_element<Block_address_list> * p = _list.head();
      assert(p);
      while(p) { 
        List_element<Block_address_list> * q = p;
        p = p->next();
        assert(q);
        delete q;
      }
    }
    
    unsigned list_count() { return _list_count; }
    
    void insert(Block_address_list * newentry) {
      assert(newentry);
      _list.insert((List_element<Block_address_list> *)newentry);
      _list_count++;
    }

    Block_address_list * get_block_list(unsigned index) {
      if(index > _list_count) return NULL;
      List_element<Block_address_list> * p = _list.head();
      assert(p);
      while(index > 0) { 
        p = p->next();
        if(p==NULL) return NULL;
        index--;
      }
      return (Block_address_list *) p;
    }

    /** 
     * Convert a file offset value (in bytes) to a block-offset pair
     * 
     * @param fileoffset 
     * @param bop 
     * 
     * @return S_OK on success or E_OUT_OF_BOUNDS
     */
    // status_t get_location_for_fileoffset(filepos_t fileoffset, block_offset_pair_t& bop) {
      
    //   List_element<Block_address_list> * p = _list.head();
    //   while(p) {
    //     p = p->next();
    //   }
    //   if(!p) return E_OUT_OF_BOUNDS;
    //   return S_OK;
    // }

    List<Block_address_list> * list() { 
      return &_list; 
    }

    Block_address_collective * append(Block_address_collective * list_to_append)
    {
      List_element<Block_address_list> * p = list_to_append->list()->head();
      while(p) {
        this->insert((Block_address_list *)p);
        p = p->next();
      }
      // List<Block_address_list> * p = list_to_append->list();
      // _list.append(p);
      // _list_count += list_to_append->list_count();
      return this;
    }

    void dump() 
    {
      List_element<Block_address_list> * p = _list.head();
      info("Block address collective (%u entries) ---------------------\n",_list.length());
      while(p) {
        ((Block_address_list *)p)->dump();
        p = p->next();          
      }
    }

    unsigned total_count()
    {
      unsigned total_count = 0;
      List_element<Block_address_list> * p = _list.head();
      while(p) {
        total_count += ((Block_address_list *)p)->count_blocks();
        p = p->next();          
      }
      return total_count;
    }
  };



  class Logical_range_tree_node
  {
  private:
    Block_address_list * _bal;

  public:
    void attach_bal(Block_address_list * b) { _bal = b; }
    Block_address_list * bal() { return _bal; }
  };

  /** 
   * Tree-structure used to keep a mapping between the block address lists and the logical ranges they represent
   * 
   * @param collective List of block ranges that are collected from the file system
   */
  class Logical_range_tree : public Range_tree<filepos_t, Logical_range_tree_node>
  {
  private:
    Block_address_collective * _collective;
    size_t _fs_block_size_bytes;

  public:
    // ctor
    Logical_range_tree(Block_address_collective * collective,
                       size_t block_size_in_bytes) : _collective(collective) {
      assert(block_size_in_bytes > 0);
      _fs_block_size_bytes = block_size_in_bytes;
      uint64_t curr_block = 0;
      assert(collective);
      Block_address_list * lbal = collective->list()->head();

      while(lbal) {
        unsigned num_blocks_in_list = lbal->count_blocks();
        if(num_blocks_in_list > 0) {
          // info("num_blocks = %u\n",num_blocks_in_list);
          // info("inserting range %llu->%llu\n",
          //                curr_block*block_size_in_bytes,
          //                (curr_block + num_blocks_in_list)*block_size_in_bytes-1);

          Range_node<filepos_t, Logical_range_tree_node> * newnode = 
            new Range_node<filepos_t, Logical_range_tree_node>(curr_block*block_size_in_bytes, // base
                                                            num_blocks_in_list*block_size_in_bytes); // range

          assert(newnode);
          newnode->attach_bal(lbal); // connect in block list
          insert(newnode);
        }
        curr_block += num_blocks_in_list;
        lbal = lbal->next();
      }
    }

    ~Logical_range_tree() {      
    }

    status_t lookup_block_and_offset(filepos_t pos, block_address_t& block, unsigned& offset ) 
    {
      Range_node<filepos_t,Logical_range_tree_node> * r = search_region(REG_CONTAINMENT,pos,1);
      if(!r) return NULL;

      assert(pos >= r->lower() && pos <= r->upper());
      
      Block_address_list * bal = r->bal();
      assert(bal);

      // info("lower = %llu\n",r->lower());
      // info("upper = %llu\n",r->upper());

      /* each block is _fs_block_size_bytes - first find block */
      unsigned delta = (pos - r->lower());
      unsigned block_idx = delta / _fs_block_size_bytes;

      block = bal->get_block_address(block_idx);      
      offset = delta % _fs_block_size_bytes;
      return S_OK;
    }
  };
}

#endif
