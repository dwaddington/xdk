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

#ifndef __EXT2_INODE_H__
#define __EXT2_INODE_H__

#include <types.h>
#include <list.h>
#include "block_device.h"
#include "byteorder.h"
#include "ext2_addr_block.h"

#define EXT2_INODE_MODE_FIFO            0x1000
#define EXT2_INODE_MODE_CHARDEV         0x2000
#define EXT2_INODE_MODE_DIRECTORY       0x4000
#define EXT2_INODE_MODE_BLOCKDEV        0x6000
#define EXT2_INODE_MODE_FILE            0x8000
#define EXT2_INODE_MODE_SOFTLINK        0xA000
#define EXT2_INODE_MODE_SOCKET          0xC000
#define EXT2_INODE_MODE_ACCESS_MASK     0x0FFF
#define EXT2_INODE_MODE_TYPE_MASK       0xF000
#define EXT2_INODE_DIRECT_BLOCKS        12

#define EXT2_INODE_ROOT_INDEX           2

/*
 * Inode flags (FS_IOC_GETFLAGS / FS_IOC_SETFLAGS)
*/
#define FS_SECRM_FL                     0x00000001 /* Secure deletion */
#define FS_UNRM_FL                      0x00000002 /* Undelete */
#define FS_COMPR_FL                     0x00000004 /* Compress file */
#define FS_SYNC_FL                      0x00000008 /* Synchronous updates */
#define FS_IMMUTABLE_FL                 0x00000010 /* Immutable file */
#define FS_APPEND_FL                    0x00000020 /* writes to file may only append */
#define FS_NODUMP_FL                    0x00000040 /* do not dump file */
#define FS_NOATIME_FL                   0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define FS_DIRTY_FL                     0x00000100
#define FS_COMPRBLK_FL                  0x00000200 /* One or more compressed clusters */
#define FS_NOCOMP_FL                    0x00000400 /* Don't compress */
#define FS_ECOMPR_FL                    0x00000800 /* Compression error */
/* End compression flags --- maybe not all used */
#define FS_BTREE_FL                     0x00001000 /* btree format dir */
#define FS_INDEX_FL                     0x00001000 /* hash-indexed directory */
#define FS_IMAGIC_FL                    0x00002000 /* AFS directory */
#define FS_JOURNAL_DATA_FL              0x00004000 /* Reserved for ext3 */
#define FS_NOTAIL_FL                    0x00008000 /* file tail should not be merged */
#define FS_DIRSYNC_FL                   0x00010000 /* dirsync behaviour (directories only) */
#define FS_TOPDIR_FL                    0x00020000 /* Top of directory hierarchies*/
#define FS_EXTENT_FL                    0x00080000 /* Extents */
#define FS_DIRECTIO_FL                  0x00100000 /* Use direct i/o */
#define FS_NOCOW_FL                     0x00800000 /* Do not cow file */
#define FS_RESERVED_FL                  0x80000000 /* reserved for ext2 lib */

#define FS_FL_USER_VISIBLE              0x0003DFFF /* User visible flags */
#define FS_FL_USER_MODIFIABLE           0x000380FF /* User modifiable flags */

namespace Ext2fs
{
  class Inode
  {
  private:
    union {
      struct {
        uint16_t mode;
        uint16_t user_id;
        uint32_t size; // file length in bytes
        uint32_t last_access_time;
        uint32_t creation_time;
        uint32_t last_mod_time;
        uint32_t deletion_time;
        uint16_t group_id;
        uint16_t links_count; // Hard link count, when 0 the inode is to be freed
        uint32_t blocks; // size of this inode in blocks
        uint32_t flags;
        uint8_t  unused2[4]; // OS specific stuff
        uint32_t direct_data_blocks[12]; // Direct block ids stored in this inode
        uint32_t indirect_data_blocks[3];
        uint32_t version;
        uint32_t file_acl;
        union {
          uint32_t dir_acl;
          uint32_t size_high; // For regular files in version >= 1
        } __attribute__ ((packed));
        uint8_t unused3[6];
        uint16_t mode_high; // Hurd only
        uint16_t user_id_high; // Linux/Hurd only
        uint16_t group_id_high; // Linux/Hurd only
      } __attribute__((packed));
      byte _raw[256];
    };
    aoff64_t _filesystem_offset; /* offset in the file system for this i-node */
      

  public:
    // ctor
    Inode(aoff64_t fs_offset) {
      _filesystem_offset = fs_offset;
    }

    aoff64_t fs_offset() { return _filesystem_offset; }
        
  public:
    /* endian safe accessors */
    uint16_t get_mode() { return uint16_t_le2host(mode); }
    uint16_t get_user_id() { return uint16_t_le2host(user_id); }
    uint32_t get_size() { return uint32_t_le2host(size); }
    uint32_t get_creation_time() { return uint32_t_le2host(creation_time); }
    uint32_t get_file_acl() { return uint32_t_le2host(file_acl); }
    uint32_t get_blocks() { return uint32_t_le2host(blocks); }
    uint32_t get_direct_data_block_ptr(unsigned n) { 
      assert(n < 12);
      return uint32_t_le2host(direct_data_blocks[n]); 
    }
    uint32_t get_singly_indirect() {
      return uint32_t_le2host(indirect_data_blocks[0]);
    }
    uint32_t get_doubly_indirect() {
      return uint32_t_le2host(indirect_data_blocks[1]);
    }
    uint32_t get_triply_indirect() {
      return uint32_t_le2host(indirect_data_blocks[2]);
    }

    bool is_fifo() { return mode & EXT2_INODE_MODE_FIFO; }
    bool is_dir() { return mode & EXT2_INODE_MODE_DIRECTORY; }
    bool is_file() { return mode & EXT2_INODE_MODE_FILE; }
    bool has_indirect() { 
      return ( indirect_data_blocks[0] || 
               indirect_data_blocks[1] || 
               indirect_data_blocks[2] );
    }

    Block_address_list * get_direct_blocks() {
      Block_address_list * r = new Block_address_list(48);
      /* there are 12 direct block pointers */
      block_address_t * b = r->raw_array();
      for(unsigned i=0;i<12;i++) {
        //        r[i] = uint32_t_le2host(direct_data_blocks[i]);
        b[i] = uint32_t_le2host(direct_data_blocks[i]);
      }
      return r;
    }
        
  public:

    void dump() 
    {
      info("--INODE-----------------------------\n");
      info("\tmode = 0x%x\n",get_mode());
      info("\tuser id = 0x%x\n",get_user_id());
      info("\tsize = %u\n",get_size());
      info("\tfileacl = %x\n",get_file_acl());
      info("\tcreation time = 0x%x\n",get_creation_time());
      info("\tblocks = %u\n",get_blocks());
      for(int i=0;i<12;i++)
        info("\tdata_ptr[%d] = %u\n",i,get_direct_data_block_ptr(i));
      info("\tindirect = %s\n", has_indirect() ? "yes" : "no");
      if(has_indirect()) {
        info("\tsingle indirect = %u\n",indirect_data_blocks[0]);
        info("\tdouble indirect = %u\n",indirect_data_blocks[1]);
        info("\ttriply indirect = %u\n",indirect_data_blocks[2]);
      }
    }
  } __attribute__((packed));

}

#endif
