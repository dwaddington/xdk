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

#ifndef _EXT2_BLOCK_GROUP_H__
#define _EXT2_BLOCK_GROUP_H__

#include <types.h>
#include <list.h>
#include "block_device.h"
#include "byteorder.h"

namespace Ext2fs
{
  class Block_group
  {
  private:
    typedef struct ext2_block_group {
      uint32_t block_bitmap_block; // phys block address for block bitmap (which is one block long)
      uint32_t inode_bitmap_block; // phys block address for inode bitmap (which is one block long)
      uint32_t inode_table_first_block; // Block ID of first block of inode table 
      uint16_t free_block_count; // Count of free blocks
      uint16_t free_inode_count; // Count of free inodes
      uint16_t directory_inode_count; // Number of inodes allocated to directories
      uint16_t pad;
      uint8_t reserved[12];
    } __attribute__((packed)) ext2_block_group_t;

    ext2_block_group_t _block_group;

  public:
    /* endian-safe accessors */
    uint32_t get_block_bitmap_block() { return uint32_t_le2host(_block_group.block_bitmap_block); }
    uint32_t get_inode_bitmap_block() { return uint32_t_le2host(_block_group.inode_bitmap_block); }
    uint32_t get_inode_table_first_block() { return uint32_t_le2host(_block_group.inode_table_first_block); }
    uint16_t get_free_block_count() { return uint16_t_le2host(_block_group.free_block_count); }
    uint16_t get_free_inode_count() { return uint16_t_le2host(_block_group.free_inode_count); }
    uint16_t get_directory_inode_count() { return uint16_t_le2host(_block_group.directory_inode_count); }

  public:
    Block_group() {}

    void * raw() { return (void*) &_block_group; }
    void dump_block_group() {
      info("-Block group:--------------------------\n");
      info("\tLBA for block bitmap: 0x%x\n",get_block_bitmap_block());
      info("\tLBA Inode bitmap: 0x%x\n",get_inode_bitmap_block());
      info("\tFree block count: %d\n",get_free_block_count());
      info("\tFree inode count: %d\n",get_free_inode_count());
      info("\tDirectory inode count: %d\n",get_directory_inode_count());
      info("\tFirst block of inode table: 0x%x\n",get_inode_table_first_block());
    }

    
  };
}

#if 0

//#include <libblock.h>


#if 0
typedef struct ext2_block_group_ref {
	block_t *block; // Reference to a block containing this block group descr
	ext2_block_group_t *block_group;
} ext2_block_group_ref_t;
#endif

#define EXT2_BLOCK_GROUP_DESCRIPTOR_SIZE 32

extern uint32_t	ext2_block_group_get_block_bitmap_block(ext2_block_group_t *);
extern uint32_t	ext2_block_group_get_inode_bitmap_block(ext2_block_group_t *);
extern uint32_t	ext2_block_group_get_inode_table_first_block(ext2_block_group_t *);
extern uint16_t	ext2_block_group_get_free_block_count(ext2_block_group_t *);
extern uint16_t	ext2_block_group_get_free_inode_count(ext2_block_group_t *);
extern uint16_t	ext2_block_group_get_directory_inode_count(ext2_block_group_t *);

extern void	ext2_block_group_set_free_block_count(ext2_block_group_t *, uint16_t);

#endif

/** @}
 */
#endif
