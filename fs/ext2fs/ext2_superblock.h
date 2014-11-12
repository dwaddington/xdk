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







#ifndef _EXT2_SUPERBLOCK_H_
#define _EXT2_SUPERBLOCK_H_

#include <types.h>
#include <list.h>

#include "block_device.h"
#include "ext2fs.h"
#include "ext2_block_group.h"
#include "ext2_inode.h"
#include "ext2_addr_block.h"
#include "byteorder.h"

#define LBA2OFFSET(X) (X * _block_size)

namespace Ext2fs
{  
  class Superblock
  {
  private:
    typedef struct tag_superblock_t {
      
      union {
        struct {
          uint32_t	total_inode_count; // Total number of inodes
          uint32_t	total_block_count; // Total number of blocks
          uint32_t	reserved_block_count; // Total number of reserved blocks
          uint32_t	free_block_count; // Total number of free blocks
          uint32_t	free_inode_count; // Total number of free inodes
          uint32_t	first_block; // Block containing the superblock (either 0 or 1)
          uint32_t	block_size_log2; // log_2(block_size)
          int32_t		fragment_size_log2; // log_2(fragment size)
          uint32_t	blocks_per_group; // Number of blocks in one block group
          uint32_t	fragments_per_group; // Number of fragments per block group
          uint32_t	inodes_per_group; // Number of inodes per block group
          uint8_t		unused2[12];
          uint16_t	magic; // Magic value
          uint16_t	state; // State (mounted/unmounted)
          uint16_t	error_behavior; // What to do when errors are encountered
          uint16_t	rev_minor; // Minor revision level
          uint8_t		unused3[8];
          uint32_t	os; // OS that created the filesystem
          uint32_t	rev_major; // Major revision level
          uint8_t		unused4[4];
          
          // Following is for ext2 revision 1 only
          uint32_t	first_inode;
          uint16_t	inode_size;
          uint16_t	unused5;
          uint32_t	features_compatible;
          uint32_t	features_incompatible;
          uint32_t	features_read_only;
          uint8_t		uuid[16]; // UUID TODO: Create a library for UUIDs
          uint8_t		volume_name[16];
        };
        byte _padding[1024];
      };
    } __attribute__ ((packed)) superblock_t;

  private:
    block_device_session_t * _block_device;
    superblock_t _sb;
    unsigned     _num_block_groups;
    unsigned     _block_size;
    aoff64_t     _sb_start_offset; /* physical offset for start of super block */
    aoff64_t     _bg_start_offset; /* physical offset for start of block group descriptors */
    aoff64_t     _partition_start_offset;

  public:
    Superblock(block_device_session_t * block_device, uint32_t start_lba) 
      : _block_device(block_device) {

      memset(&_sb,0,sizeof(superblock_t));
      assert(block_device);

      /* ext2 superblock is located at 1024 bytes offset from the start of 
         the volume (which we got as an LBA) and is 1K long */
      _partition_start_offset = start_lba * 512;
      _sb_start_offset = 1024 + _partition_start_offset; // after MBRs

      block_device->read(_sb_start_offset,1024,(void*)&_sb); 

      sanity_checks();
      _block_size = get_block_size();

      _num_block_groups = get_total_block_count() / get_blocks_per_group();
      if(get_total_block_count() % get_blocks_per_group() > 0) _num_block_groups++;
      assert(_num_block_groups == (get_total_inode_count() / get_inodes_per_group()));

      /* the block group descriptor table is block 2 */
      if(_block_size > 1024)
        _bg_start_offset = _block_size + _partition_start_offset;
      else
        _bg_start_offset = _block_size + _sb_start_offset;

      dump_sb_info();
    }

    void dump_sb_info() {
      info("EXT2 Superblock (at byte %llx):\n",_sb_start_offset);
      info("\tMagic: %x\n",get_magic());
      info("\tBlock size: %u\n",get_block_size());
      info("\tInode size: %u\n",get_inode_size());
      info("\tTotal inodes: %d\n",get_total_inode_count());
      info("\tTotal blocks: %d\n",get_total_block_count());
      info("\tFree inodes: %d\n",get_free_inode_count());
      info("\tFree blocks: %d\n",get_free_block_count());
      info("\tCreator OS: 0x%x\n",get_os());
      info("\tBlocks per group: %d\n",get_blocks_per_group());
      info("\tInodes per group: %d\n",get_inodes_per_group());
      info("\tNumber of block groups: %d\n",_num_block_groups);
      info("\tBlock groups at byte %llx\n",_bg_start_offset);
      info("\tPartition start offset (bytes) %llx\n",_partition_start_offset);
    }

    void sanity_checks() {
      assert(get_magic()==0xEF53); /* check magic number is ext2 */
      assert(get_rev_major() == 1);
      assert(get_first_inode() > 10);
    }

    /** 
     * Convert from a logical block to a physical byte offset
     * 
     * @param block Logical block number
     * 
     * @return Offset in bytes
     */
    aoff64_t block_to_abs_offset(block_off_t block) {
      assert(_block_size);
      return ((aoff64_t)block * (aoff64_t)_block_size) + _partition_start_offset;
    }

    /** 
     * Locate block group; create an object and populate.
     * 
     * @param bgid 
     * 
     * @return 
     */
    Block_group * get_block_group_ref(unsigned bgid) {
      if(bgid > _num_block_groups) {
        panic("bgid (%u) > _num_block_groups(%u)",bgid,_num_block_groups);
        return NULL;
      }
      /* each block group descriptor is 32 bytes */
      aoff64_t offset = _bg_start_offset + (32 * bgid);

      Block_group * bgref = new Block_group();
      assert(bgref);
      assert(_block_device);

      _block_device->read(offset,32,bgref->raw());
      return bgref;
    }


    /** 
     * Read inode structure at a given block id
     * 
     * @param block_id 
     * 
     * @return 
     */
    Inode * get_inode_from_block(block_off_t block_id) {

      aoff64_t inode_offset = block_to_abs_offset(block_id);
      Inode * inode_ref = new Inode(inode_offset);      
      assert(inode_ref);
      _block_device->read(inode_offset,sizeof(Inode),inode_ref);
      return inode_ref;

    }


    /** 
     * Return list of block addresses 
     * 
     * @param block_id 
     * 
     * @return 
     */
    Block_address_list * get_block_address_list(block_off_t block_id) {
      aoff64_t offset = block_to_abs_offset(block_id);
      Block_address_list * result = new Block_address_list(_block_size);
      assert(result);
      _block_device->read(offset,_block_size,result->raw());
      return result;
    }
      

    /** 
     * Locate an inode; create an object and populate
     * 
     * @param index Inode index
     * 
     * @return Pointer to newly created Inode object (client will delete)
     */
    Inode * get_inode_ref(inode_idx_t inode_index) {

      //      info("get_inode_ref(%u)\n",inode_index);
      /*  inode addresses start at 1! */
      assert(inode_index > 0);
      assert(inode_index <= get_total_inode_count());

      block_off_t block_group_id = (inode_index - 1) / get_inodes_per_group();
      unsigned group_offset = (inode_index - 1) % get_inodes_per_group();

      /* get hold of the block group */
      Block_group * block_group_ref = get_block_group_ref(block_group_id);

      assert(block_group_ref);
      //      block_group_ref->dump_block_group();

      /* translate block position to absolute byte offset on device */
      aoff64_t inode_pos_abs = block_to_abs_offset(block_group_ref->get_inode_table_first_block());
      inode_pos_abs += (get_inode_size() * group_offset);

      // /* find the byte location of the node */
      // aoff64_t logical_block = block_group_ref->get_inode_table_first_block();
      // info("logical block = %llu\n",logical_block);
      // logical_block += (get_inode_size() * group_offset);
      // info("logical block = %llu\n",logical_block);

      // aoff64_t inode_block_offset = block_to_abs_offset(logical_block);

      // if(inode_index == 409602) {
      //    panic("inode block offset = %llu\n",inode_block_offset);
      //  }

      Inode * inode_ref = new Inode(inode_pos_abs);      
      assert(inode_ref);
      assert(_block_device);

      _block_device->read(inode_pos_abs,sizeof(Inode),inode_ref);
      //      inode_ref->dump();

      delete block_group_ref;

      return inode_ref;
    }

    // Inode * get_inode_ref2(unsigned inode_index) {

    //   info("get_inode_ref2(%u)\n",inode_index);
    //   /*  inode addresses start at 1! */
    //   assert(inode_index > 0);
    //   assert(inode_index <= get_total_inode_count());

    //   unsigned block_group_id = (inode_index - 1) / get_inodes_per_group();
    //   unsigned group_offset = (inode_index - 1) % get_inodes_per_group();

    //   info("block_group_id = %u\n",block_group_id);
    //   panic("group_offset = %u\n",group_offset);

    //   /* get hold of the block group */
    //   Block_group * block_group_ref = get_block_group_ref(block_group_id);

    //   assert(block_group_ref);
    //   block_group_ref->dump_block_group();

    //   panic("block group OK.");

    //   /* translate block position to absolute byte offset on device */
    //   //      aoff64_t inode_pos_abs = block_to_abs_offset(block_group_ref->get_inode_table_first_block());
    //   //      inode_pos_abs += (get_inode_size() * group_offset);
    //   aoff64_t first_block_abs = block_to_abs_offset(block_group_ref->get_inode_table_first_block());
    //   aoff64_t inode_pos_abs = first_block_abs; // + (get_inode_size() * group_offset);
        

    //   // /* find the byte location of the node */
    //   // aoff64_t logical_block = block_group_ref->get_inode_table_first_block();
    //   // info("logical block = %llu\n",logical_block);
    //   // logical_block += (get_inode_size() * group_offset);
    //   // info("logical block = %llu\n",logical_block);

    //   // aoff64_t inode_block_offset = block_to_abs_offset(logical_block);

    //   // if(inode_index == 409602) {
    //   //    panic("inode block offset = %llu\n",inode_block_offset);
    //   //  }

    //   //      assert(inode_pos_abs % 512 == 0);
    //   info("BLOCK NUMBER = %llu\n",inode_pos_abs / 512);
    //   Inode * inode_ref = new Inode(inode_pos_abs);      
    //   assert(inode_ref);
    //   assert(_block_device);
    //   info("reading from absolute disk position %llu\n",inode_pos_abs);
    //   _block_device->read(inode_pos_abs,sizeof(Inode),inode_ref);
    //   inode_ref->dump();
    //   if(inode_index == 409602)
    //     panic("check it");
    //   delete block_group_ref;

    //   return inode_ref;
    // }


    /** 
     * Populate inode data from the disk
     * 
     * @param inode_index 
     * @param inode 
     */
    void read_inode(inode_idx_t inode_index, Inode * inode) {
      /*  inode addresses start at 1! */
      assert(inode_index > 0);
      assert(inode_index <= get_total_inode_count());
      block_off_t block_group_id = (inode_index - 1) / get_inodes_per_group();
      unsigned group_offset = (inode_index - 1) % get_inodes_per_group();

      /* get hold of the block group */
      Block_group * block_group_ref = get_block_group_ref(block_group_id);
      assert(block_group_ref);
      block_group_ref->dump_block_group();

      /* find the byte location of the node */
      aoff64_t inode_pos_abs = block_to_abs_offset(block_group_ref->get_inode_table_first_block());
      inode_pos_abs += (get_inode_size() * group_offset);
      
      _block_device->read(inode_pos_abs,sizeof(Inode),inode);
      delete block_group_ref;
    }


    /* endian-safe accessors */
    uint16_t get_magic() { return _sb.magic; }
    uint32_t get_first_block() { return uint32_t_le2host(_sb.first_block); }
    uint32_t get_block_size_log2() { return uint32_t_le2host(_sb.block_size_log2); }
    uint32_t get_block_size() { return 1024 << get_block_size_log2(); }
    uint32_t get_fragment_size_log2() { return uint32_t_le2host(_sb.fragment_size_log2); }
    uint32_t get_fragment_size() { return 1024 << uint32_t_le2host(_sb.fragment_size_log2); }
    uint32_t get_blocks_per_group() { return uint32_t_le2host(_sb.blocks_per_group); }
    uint32_t get_fragments_per_group() { return uint32_t_le2host(_sb.fragments_per_group); }
    // uint16_t	get_state();
    uint32_t get_rev_major() { return uint32_t_le2host(_sb.rev_major); }
    uint16_t get_rev_minor() { return uint16_t_le2host(_sb.rev_minor); }
    uint32_t get_os() { return uint32_t_le2host(_sb.os); }
    uint32_t get_first_inode() { return uint32_t_le2host(_sb.first_inode); }
    uint16_t get_inode_size() { return uint16_t_le2host(_sb.inode_size); }
    uint32_t get_total_inode_count() { return uint32_t_le2host(_sb.total_inode_count); }
    uint32_t get_total_block_count()  { return uint32_t_le2host(_sb.total_block_count); }
    // uint32_t get_reserved_block_count();
    uint32_t get_free_block_count() { return uint32_t_le2host(_sb.free_block_count); }
    uint32_t get_free_inode_count() { return uint32_t_le2host(_sb.free_inode_count); }
    // uint32_t get_block_group_count();
    uint32_t get_inodes_per_group() { return uint32_t_le2host(_sb.inodes_per_group); }
    // uint32_t get_features_compatible();
    // uint32_t get_features_incompatible();
    // uint32_t get_features_read_only();
    const char * get_volume_name() { return ((const char *)_sb.volume_name); }
  };
}

#define EXT2_SUPERBLOCK_MAGIC		0xEF53
#define EXT2_SUPERBLOCK_SIZE		1024
#define EXT2_SUPERBLOCK_OFFSET		1024
#define EXT2_SUPERBLOCK_LAST_BYTE	(EXT2_SUPERBLOCK_OFFSET + \
                                   EXT2_SUPERBLOCK_SIZE -1)
#define EXT2_SUPERBLOCK_OS_LINUX	0
#define EXT2_SUPERBLOCK_OS_HURD		1


  

#endif

  /** @}
   */
