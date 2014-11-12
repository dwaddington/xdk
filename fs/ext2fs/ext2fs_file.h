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

#include "ext2fs.h"
#include "measure.h"
#include <md5.h>

namespace Ext2fs
{
  /* I'm not really sure what this should look like - need to think more, but for now get something working! */
  class File
  {
    friend class Ext2fs_core;

  private:
    Ext2fs_core *               _core;  /* core ext2fs services */
    Inode *                     _inode; /* file system inode */
    Block_address_collective *  _block_collective; /* block collective for quick access to block list */
    filepos_t                      _pos; /* current logical file position */
    Logical_range_tree *        _range_tree;

    Block_address_list *        _last_block_address_list;
    block_address_t *           _last_block_pointer;
    block_address_t *           _last_block_pointer_index;
    md5_byte_t                  _digest[16];    
  public:
    // ctor
    File(Ext2fs_core * c) : _inode(NULL), _block_collective(NULL), _pos(0) {      
      assert(c);
      _core = c;
      _last_block_address_list = NULL;
      _last_block_pointer = NULL;
      __builtin_memset(_digest,0,sizeof(_digest));
    }
    
    // dtor
    ~File() {
      if(_inode) delete _inode;
      if(_block_collective) delete _block_collective;
    }

  protected:    

    // system call like interface to ext2fs files
    //
    status_t open_file(const char * pathname);
    status_t unlink() { panic("unlink: not implemented."); return E_FAIL; }
    
  public:

    /** 
     * Read from file position 'read_pos', 'len' bytes and copy to 'buffer'.
     * 
     * @param read_pos File position (bytes)
     * @param len Number of bytes to read
     * @param buffer Destination buffer
     * 
     * @return S_OK on success
     */
    status_t read(filepos_t read_pos, size_t len, void * buffer);


    /** 
     * Read from the current file position 'len' bytes and copy to 'buffer'.
     * 
     * @param len Number of bytes to read
     * @param buffer Destination buffer
     * 
     * @return S_OK on success
     */
    status_t read(size_t len, void * buffer);


    /** 
     * Return the size in bytes of a this file
     * 
     * @return Size in bytes
     */        
    size_t size_in_bytes() {
      assert(_inode);
      return _inode->get_size();
    }    
    
    void seek(filepos_t pos = 0) {
      _pos = pos;
    }

    const Inode * get_inode() { 
      return _inode; 
    }
    
    filepos_t pos() { 
      return _pos;
    }

    /** 
     * Calculate the MD5 checksum for the file
     * 
     */
    status_t md5sum() {

      md5_state_t md5_state;      
      md5_init(&md5_state);
      seek(0);

      size_t bytes_remaining = _inode->get_size();
      const size_t block_size = 4 * 1024 * 1024;
      byte * buffer = (byte *) malloc(block_size);
      while(bytes_remaining) {
        if(bytes_remaining > block_size) {          
          read(block_size, buffer);
          md5_append(&md5_state,buffer,block_size);
          bytes_remaining -= block_size;
        }
        else {
          read(bytes_remaining, buffer);
          md5_append(&md5_state,buffer,bytes_remaining);
          break;
        }
      }
      
      md5_finish(&md5_state,_digest);
      free((void*) buffer);

      info("md5: %x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",
           _digest[0], _digest[1], _digest[2],  _digest[3],
           _digest[4], _digest[5], _digest[6],  _digest[7],
           _digest[8], _digest[9], _digest[10], _digest[11],
           _digest[12], _digest[13], _digest[14], _digest[15]);

      return S_OK;
    }
    

  };

}
