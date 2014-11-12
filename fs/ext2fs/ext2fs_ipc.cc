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

#include "ext2fs_ipc.h"
#include "ext2fs.h"
#include "ext2fs_file.h"

status_t Ext2fs::Filesystem_session_impl::open_file(String pathname, 
                                                    unsigned long, // flags, 
                                                    file_handle_t & file ) {
  
  EXT2FS_INFO("Ext2fs::Filesystem_session_impl::open_file [%s]\n",pathname.c_str());

  if(_curr_handle_index > MAX_ACTIVE_HANDLES) { assert(0); return E_INSUFFICIENT_RESOURCES; }

  File_handle * newfh = new File_handle;
  assert(newfh);
  file = newfh->_handle = _curr_handle_index++;

  assert(_core);
  newfh->_file_obj = _core->open(pathname.c_str());

  if(!newfh->_file_obj) { assert(0); return E_FAIL; }

  _active_handles.insert_front(newfh);
  
  EXT2FS_INFO("open file succeeded: handle=%lu\n",file);
  return S_OK;
}


status_t Ext2fs::Filesystem_session_impl::close_file(file_handle_t file) {

  /* locate file in list */
  List_element<File_handle> * e = _active_handles.head();
  assert(e);
  while(e) {
    if(((File_handle*)e)->_handle == file) {
      Ext2fs::File * fobj =  ((File_handle*)e)->_file_obj;
      _active_handles.remove(e); /* remove from list */
      delete e;
      return _core->close(fobj);
    }
    e = e->next();
  }
  return E_BAD_PARAM;
}

status_t Ext2fs::Filesystem_session_impl::read_file_from_offset(file_handle_t file, 
                                                                unsigned long byte_count, 
                                                                offset_t file_offset) {

  assert(byte_count > 0);

  if((file_offset + byte_count) > _client_shmem_size) 
    return E_LENGTH_EXCEEDED;

  /* locate file in list */
  List_element<File_handle> * e = _active_handles.head();
  assert(e);
  while(e) {
    if(((File_handle*)e)->_handle == file) {
      Ext2fs::File * fobj =  ((File_handle*)e)->_file_obj;

      if((file_offset + byte_count) > fobj->size_in_bytes()) {
        return E_LENGTH_EXCEEDED;
      }

      fobj->seek(file_offset); /* seek to read position */

      return fobj->read(byte_count, _client_shmem);
    }
    e = e->next();
  }
  return E_BAD_PARAM;
}


status_t Ext2fs::Filesystem_session_impl::read_file(file_handle_t file, 
                                                    unsigned long byte_count)
{
  assert(byte_count > 0);

  /* locate file in list */
  List_element<File_handle> * e = _active_handles.head();
  assert(e);
  while(e) {
    if(((File_handle*)e)->_handle == file) {
      Ext2fs::File * fobj =  ((File_handle*)e)->_file_obj;


      if((fobj->pos() + byte_count) > fobj->size_in_bytes()) {
        return E_LENGTH_EXCEEDED;
      }
      
      return fobj->read(byte_count, _client_shmem);
    }
    e = e->next();
  }
  return E_BAD_PARAM;
}


status_t Ext2fs::Filesystem_session_impl::read_file_info(file_handle_t fh, file_info_t& info)
{
  /* locate file in list */
  List_element<File_handle> * e = _active_handles.head();
  assert(e);
  while(e) {
    if(((File_handle*)e)->_handle == fh) {
      Ext2fs::File * fobj =  ((File_handle*)e)->_file_obj;

      const Inode * inode = fobj->get_inode();
      assert(inode);
      info.mode = inode->mode;
      info.user_id = inode->user_id;
      info.size = inode->size;
      info.creation_time = inode->creation_time;
      info.last_mod_time = inode->last_mod_time;
      info.deletion_time = inode->deletion_time;
      info.group_id = inode->group_id;
      info.file_acl = inode->file_acl;
      
      return S_OK;
    }
    e = e->next();
  }
  return E_NOT_FOUND;
}

status_t Ext2fs::Filesystem_session_impl::read_directory_entries(String pathname, directory_query_flags_t flags, size_t & num_entries)
{
  assert(_client_shmem);
  char * result = (char *) _client_shmem;
  result[0] = '\0';

  num_entries = 0;

  /* locate inode for directory */
  Inode * dir_inode;
  if(pathname.c_str()[1] == '\0') {
    dir_inode = _core->get_root_directory();
  }
  else {
    dir_inode = _core->locate_directory(BString(pathname.c_str()),_core->get_root_directory());
  }

  if(dir_inode==NULL) return E_FAIL;

  size_t dir_size = 0;
  Directory_entry * entries;
  Directory_entry * entry = entries = _core->get_directory_entries(dir_inode, dir_size);
  size_t s = dir_size;
  while(s > 0) {

    /* process entry */
    { 
      /* files */
      if(flags & QUERY_FILES) {
        if(entry->get_file_type() == 0x1) {
          strcpy(result, entry->get_name());
          strcat(result,"\n");
          //          printf("copied name [%s]\n",entry->get_name());
          result += entry->get_name_length() + 1;
          num_entries++;
        }
      }
      /* dirs */
      if(flags & QUERY_DIRS) {
        if(entry->get_file_type() == 0x2) {
          strcpy(result, entry->get_name());
          strcat(result,"\n");
          //          printf("copied name [%s]\n",entry->get_name());
          result += entry->get_name_length() + 1;
          num_entries++;
        }
      }
      assert(result < (((char *)_client_shmem) + _client_shmem_size));
    }

    s -= entry->get_record_length();
    entry = entry->next();
  } 

  free(entries);

  return S_OK;
}


status_t Ext2fs::Filesystem_session_impl::seek(file_handle_t fh, offset_t offset)
{
  List_element<File_handle> * e = _active_handles.head();
  assert(e);
  while(e) {
    if(((File_handle*)e)->_handle == fh) {
      Ext2fs::File * fobj =  ((File_handle*)e)->_file_obj;

      if((fobj->pos() + offset) > fobj->size_in_bytes()) {
        return E_LENGTH_EXCEEDED;
      }
      else {
        fobj->seek(offset);
        return S_OK;
      }
    }
    e = e->next();
  }

  return E_BAD_PARAM;
}


