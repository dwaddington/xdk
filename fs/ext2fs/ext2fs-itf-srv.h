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






 #ifndef __EXT2FS_SERVER_SIDE_PROXY_H__
 #define __EXT2FS_SERVER_SIDE_PROXY_H__
 #include <ipc_stream>
 #include "ext2fs-itf-common.h"
#include <namespace.h>

 using namespace IDL;
 namespace Module { namespace Ext2fs { template < class T > class
Filesystem_service : public  IPC::Interface_handler , public  T { 
private:
 Ipc::Iostream& _ipc; 
public:
 Filesystem_service ( IPC::Service_loop * svc_loop ) :
IPC::Interface_handler ( T::interface_id() ) , _ipc ( svc_loop->ipc() ) {
svc_loop->register_handler((IPC::Interface_handler *) this); } 
public:
 Filesystem_service ( IPC::Service_loop& svc_loop ) :
IPC::Interface_handler ( T::interface_id() ) , _ipc ( svc_loop.ipc() ) {
svc_loop.register_handler((IPC::Interface_handler *) this); } 
public:
  bool handle ( IPC::UUID& uuid ) { unsigned int op;
 if(uuid._interface == T::interface_id()) { _ipc >> op;  switch ( op ) { case
T::OP_open_session : { Shmem_handle shmem ; IPC_handle ipc_endpoint ; status_t 
result ; _ipc >> shmem; _ipc.reset(); 
/* actual function call */
 result = T::open_session(shmem,ipc_endpoint);
_ipc << ipc_endpoint; _ipc << result; 
return true; } case T::OP_close_session : { IPC_handle ipc_endpoint ; status_t 
result ; _ipc >> ipc_endpoint; _ipc.reset(); 
/* actual function call */
 result = T::close_session(ipc_endpoint); 
/* no egress params */ _ipc << result; 
return true; } default : { debug_stop("unknown opt. %d",op); } } } return false;
} 
public:
 uint32_t module_id() { return T::module_id(); } 
public:
 T* get_impl_class() { return (T*) this; } }; class
Filesystem_interface_handler : public  Module::Ext2fs::Filesystem_interface {
virtual status_t  open_session ( Shmem_handle shmem , IPC_handle & ipc_endpoint
) = 0 ; virtual status_t  close_session ( IPC_handle ipc_endpoint ) = 0 ; };
template < class T > class Filesystem_session_service : public 
IPC::Interface_handler , public  T { 
private:
 Ipc::Iostream& _ipc; 
public:
 Filesystem_session_service ( IPC::Service_loop * svc_loop ) :
IPC::Interface_handler ( T::interface_id() ) , _ipc ( svc_loop->ipc() ) {
svc_loop->register_handler((IPC::Interface_handler *) this); } 
public:
 Filesystem_session_service ( IPC::Service_loop& svc_loop ) :
IPC::Interface_handler ( T::interface_id() ) , _ipc ( svc_loop.ipc() ) {
svc_loop.register_handler((IPC::Interface_handler *) this); } 
public:
  bool handle ( IPC::UUID& uuid ) { unsigned int op;
 if(uuid._interface == T::interface_id()) { _ipc >> op;  switch ( op ) { case
T::OP_open_file : { String pathname ; unsigned long flags ; file_handle_t file ;
status_t  result ; _ipc >> pathname; _ipc >> flags; _ipc.reset(); 
/* actual function call */
 result = T::open_file(pathname,flags,file);
_ipc << file; _ipc << result; 
return true; } case T::OP_close_file : { file_handle_t file ; status_t  result ;
_ipc >> file; _ipc.reset(); 
/* actual function call */
 result = T::close_file(file); 
/* no egress params */ _ipc << result; 
return true; } case T::OP_read_file_from_offset : { file_handle_t file ;
unsigned long byte_count ; offset_t offset ; status_t  result ; _ipc >> file;
_ipc >> byte_count; _ipc >> offset; _ipc.reset(); 
/* actual function call */

result = T::read_file_from_offset(file,byte_count,offset); 
/* no egress params */ _ipc << result; 
return true; } case T::OP_read_file : { file_handle_t file ; unsigned long
byte_count ; status_t  result ; _ipc >> file; _ipc >> byte_count; _ipc.reset(); 
/* actual function call */
 result = T::read_file(file,byte_count); 
/* no egress params */ _ipc << result; 
return true; } case T::OP_read_file_info : { file_handle_t file ; file_info_t
finfo ; status_t  result ; _ipc >> file; _ipc.reset(); 
/* actual function call */
 result = T::read_file_info(file,finfo);
_ipc << finfo; _ipc << result; 
return true; } case T::OP_read_directory_entries : { String pathname ;
directory_query_flags_t flags ; size_t entries ; status_t  result ;
_ipc >> pathname; _ipc >> flags; _ipc.reset(); 
/* actual function call */

result = T::read_directory_entries(pathname,flags,entries); _ipc << entries;
_ipc << result; 
return true; } case T::OP_seek : { file_handle_t file ; offset_t offset ;
status_t  result ; _ipc >> file; _ipc >> offset; _ipc.reset(); 
/* actual function call */
 result = T::seek(file,offset); 
/* no egress params */ _ipc << result; 
return true; } default : { debug_stop("unknown opt. %d",op); } } } return false;
} 
public:
 uint32_t module_id() { return T::module_id(); } 
public:
 T* get_impl_class() { return (T*) this; } }; class
Filesystem_session_interface_handler : public 
Module::Ext2fs::Filesystem_session_interface { virtual status_t  open_file (
String pathname , unsigned long flags , file_handle_t & file ) = 0 ; virtual
status_t  close_file ( file_handle_t file ) = 0 ; virtual status_t 
read_file_from_offset ( file_handle_t file , unsigned long byte_count , offset_t
offset ) = 0 ; virtual status_t  read_file ( file_handle_t file , unsigned long
byte_count ) = 0 ; virtual status_t  read_file_info ( file_handle_t file ,
file_info_t & finfo ) = 0 ; virtual status_t  read_directory_entries ( String
pathname , directory_query_flags_t flags , size_t & entries ) = 0 ; virtual
status_t  seek ( file_handle_t file , offset_t offset ) = 0 ; }; } } 
#endif
