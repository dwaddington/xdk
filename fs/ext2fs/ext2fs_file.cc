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

#include "ext2fs_file.h"

status_t Ext2fs::File::open_file(const char * pathname) {

  EXT2FS_INFO("Ext2fs::File::open_file [%s]",pathname);
  assert(_core);
  status_t rc = _core->get_blocks_for_file(pathname, _block_collective, _inode);

  if(rc != S_OK) return rc;

  _range_tree = new Logical_range_tree(_block_collective,_core->fs_block_size());
  assert(_range_tree);

  if(rc == S_OK) {
    _pos = 0; /* reset position */
  }
  return rc;
}


status_t Ext2fs::File::read(size_t len, void * buffer) {
  return read(_pos,len,buffer);
}

status_t Ext2fs::File::read(filepos_t read_pos, size_t len, void * buffer) {
      
  status_t rc;
  //  info("read: (pos=%llu,size=%u)\n",read_pos,len);
  /* now read from the file system */

  {
    assert(buffer);
      
    unsigned fsbs = _core->fs_block_size(); // file system block size
    filepos_t curr_pos = read_pos;
    unsigned bytes_copied = 0;
    block_address_t block=0;
    unsigned offset=0;
    unsigned bytes_to_copy;
    unsigned bytes_remaining = len;
    char * tbuffer = (char *) buffer;

    // TURN ON PROFILING
    //    _core->block_device()->turn_on_profiling();

    do {

      rc = _range_tree->lookup_block_and_offset(curr_pos, block, offset);

      assert(rc==S_OK);
      assert(offset < fsbs);
      bytes_to_copy = MIN(fsbs - offset, bytes_remaining);

      /* perform read */
      aoff64_t absoff = _core->block_to_abs_offset(block) + offset;
                 
      /* change this to dummy_read to do a null call to server */
      _core->_block_cache_session->read(absoff,
                                        bytes_to_copy,
                                        &tbuffer[bytes_copied]);
      

      bytes_copied += bytes_to_copy;
      bytes_remaining -= bytes_to_copy;
      curr_pos += bytes_to_copy;
    }
    while(bytes_copied < len);
  }
  _pos = read_pos + len;
      
  return S_OK;
}
