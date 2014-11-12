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

#ifndef __EXT2FS_H__
#define __EXT2FS_H__

#define MAX_PATH_SIZE 255
//#define VERBOSE

#ifdef VERBOSE
void EXT2FS_INFO(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else 
#define EXT2FS_INFO(...)
#endif

#include <basicstring.h>
#include <list.h>
#include <slab_allocator.h>

#include "mbr.h"
#include "ext2_superblock.h"
#include "ext2_dir.h"
#include "ext2_addr_block.h"
#include "ext2fs_ipc.h"
#include "block_cache.h"

using namespace OmniOS;

#if defined(__cplusplus)
namespace Ext2fs
{
  typedef uint32_t inode_idx_t;
  typedef uint64_t block_num_t;
  typedef uint64_t size64_t;

  // forward decls
  class File;

  class Ext2fs_core
  {
    friend class File;

  private:
    block_device_session_t * _physical_device_session;
    block_device_session_t * _block_cache_session;
    Ext2fs::Superblock *     _super_block;
    Mbr                      _mbr;
    uint64_t                 _partition_begin_lba;
    size_t                   _fs_block_size;
    size_t                   _inode_size;

  public:
    Ext2fs_core(block_device_session_t * session, unsigned instance) :
      _physical_device_session(session),
      _mbr(session)
      //      _super_block(session)
    {
      EXT2FS_INFO("Ext2fs_core ctor.\n");
      
      assert(session);
      _partition_begin_lba = _mbr.get_partition_lba(0x83 /* Linux ext2 */,instance);
      assert(_partition_begin_lba > 0); /* LBA of partition > 0 */

      /* read in the super block */
      _super_block = new Ext2fs::Superblock(_physical_device_session,_partition_begin_lba);

      _fs_block_size = _super_block->get_block_size();
      _inode_size = _super_block->get_inode_size();

      /* set up block cache */
      _block_cache_session = (block_device_session_t *) new Block_cache(_physical_device_session);
      assert(_block_cache_session);
    }

  private:
    aoff64_t block_to_abs_offset(block_num_t block) { 
      assert(_super_block); 
      return _super_block->block_to_abs_offset(block); 
    }

  public:
    Inode * get_root_directory();
    Inode * locate_directory(BString pathname, Inode * current_dir);
    Directory_entry * get_directory_entries(Inode * dir_inode, size_t& size_of_content);

  private:
    Inode * find_local_directory(BString dirname, Inode * current_dir);
    Inode * get_inode(unsigned block);
    void read_inode(uint32_t inode_index, Inode * inode);
    Inode * locate_file(const char * filename, Inode * dir);
    aoff64_t get_offset_for_file(Inode * inode, unsigned dbp_index);
    void dump_directory_entries(Inode * dir_inode);
    Block_address_list * get_block_addresses(unsigned block);
    Block_address_collective * get_singly_indirect_block_addresses(Inode * i_file);
    Block_address_collective * get_doubly_indirect_block_addresses(Inode * i_file);
    Block_address_collective * get_triply_indirect_block_addresses(Inode * i_file);    
    Block_address_collective * get_blocks_for_inode(Inode * i_file);

  protected:
    status_t get_blocks_for_file(const char * pathname, Block_address_collective *&, Inode *&);
    inline block_device_session_t * block_device() { return _block_cache_session; }
    
  public:
    void test();
    void test2();

    /** 
     * Return the size of the filesystem blocks in bytes
     * 
     * 
     * @return Size of blocks in bytes
     */
    unsigned fs_block_size() const { return _fs_block_size; }

    Ext2fs::File * open(const char * pathname);
    status_t close(Ext2fs::File * handle);
  };



} // namespace Ext2fs

#endif


#endif // __EXT2FS_H__
