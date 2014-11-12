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






 #ifndef __EXT2FS_CLIENT_SIDE_PROXY_H__
 #define __EXT2FS_CLIENT_SIDE_PROXY_H__
 #include "ext2fs-itf-common.h"
#include <namespace.h>

 using namespace IDL;
 namespace Module { namespace Ext2fs { class Filesystem_proxy : public 
Filesystem_interface { 
private:
 cap_index_t _server_cap;
Name_space_table _ns;
 
public:
 Filesystem_proxy ( const char * service_name ) {
int retry_count=0;  
    while(_ns.lookup(service_name,&_server_cap)!=S_OK) { 
      if(IDL::retry_connection(retry_count)==false) 
        panic("Proxy could not connect to [%s]\n",service_name);   
    } 
} 
public:
  status_t  open_session ( Shmem_handle shmem , IPC_handle &
ipc_endpoint ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_open_session; _ipc << shmem;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
 _ipc >> ipc_endpoint; status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  close_session ( IPC_handle ipc_endpoint ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_close_session; _ipc << ipc_endpoint;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
  status_t   result;
_ipc >> result;
 return result; } }; class Filesystem_session_proxy : public 
Filesystem_session_interface { 
private:
 cap_index_t _server_cap;
Name_space_table _ns;
 
public:
 Filesystem_session_proxy ( const char * service_name ) {
int retry_count=0;  
    while(_ns.lookup(service_name,&_server_cap)!=S_OK) { 
      if(IDL::retry_connection(retry_count)==false) 
        panic("Proxy could not connect to [%s]\n",service_name);   
    } 
} 
public:
  status_t  open_file ( String pathname , unsigned long flags ,
file_handle_t & file ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_open_file; _ipc << pathname << flags;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
 _ipc >> file; status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  close_file ( file_handle_t file ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_close_file; _ipc << file;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
  status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  read_file_from_offset ( file_handle_t file , unsigned long
byte_count , offset_t offset ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_read_file_from_offset;
_ipc << file << byte_count << offset;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
  status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  read_file ( file_handle_t file , unsigned long byte_count )
{ 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_read_file; _ipc << file << byte_count;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
  status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  read_file_info ( file_handle_t file , file_info_t & finfo )
{ 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_read_file_info; _ipc << file;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
 _ipc >> finfo; status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  read_directory_entries ( String pathname ,
directory_query_flags_t flags , size_t & entries ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_read_directory_entries;
_ipc << pathname << flags;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
 _ipc >> entries; status_t   result;
_ipc >> result;
 return result; } 
public:
  status_t  seek ( file_handle_t file , offset_t offset ) { 
/* ingress parameters */
 Ipc::Iostream _ipc(l4_utcb());
_ipc << ModuleId << InterfaceId << OP_seek; _ipc << file << offset;
l4_msgtag_t res = _ipc.call(_server_cap); assert(!l4_ipc_error(res,l4_utcb())); 
/* egress parameters */
  status_t   result;
_ipc >> result;
 return result; } }; } } 
#endif
