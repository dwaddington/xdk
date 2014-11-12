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






 #ifndef __EXT2FS_COMMON_H__
 #define __EXT2FS_COMMON_H__
 #include <idl-ipc-common.h>
 using namespace IDL;
 #include "file/types.h"
 namespace Module { namespace Ext2fs { enum  { ModuleId = 0x4A2 } ; class Object
: public  IPC::Object_base { 
public:
 Object (  ) : Object_base ( ModuleId ) {  } enum  { OP_open_session = 2
, OP_close_session = 3 , OP_open_file = 4 , OP_close_file = 5 ,
OP_read_file_from_offset = 6 , OP_read_file = 7 , OP_read_file_info = 8 ,
OP_read_directory_entries = 9 , OP_seek = 10 } ; }; class Filesystem_interface :
public  Module::Ext2fs::Object { 
public:
 enum  { InterfaceId = 0x9F2 } ;
static uint32_t interface_id() { return InterfaceId; } 
public:
 virtual status_t  open_session ( Shmem_handle shmem , IPC_handle &
ipc_endpoint ) = 0 ; 
public:
 virtual status_t  close_session ( IPC_handle ipc_endpoint ) = 0 ; };
class Filesystem_session_interface : public  Module::Ext2fs::Object { 
public:
 enum  { InterfaceId = 0xD55 } ;
static uint32_t interface_id() { return InterfaceId; } 
public:
 virtual status_t  open_file ( String pathname , unsigned long flags ,
file_handle_t & file ) = 0 ; 
public:
 virtual status_t  close_file ( file_handle_t file ) = 0 ; 
public:
 virtual status_t  read_file_from_offset ( file_handle_t file ,
unsigned long byte_count , offset_t offset ) = 0 ; 
public:
 virtual status_t  read_file ( file_handle_t file , unsigned long
byte_count ) = 0 ; 
public:
 virtual status_t  read_file_info ( file_handle_t file , file_info_t &
finfo ) = 0 ; 
public:
 virtual status_t  read_directory_entries ( String pathname ,
directory_query_flags_t flags , size_t & entries ) = 0 ; 
public:
 virtual status_t  seek ( file_handle_t file , offset_t offset ) = 0 ;
}; } } 
#endif
