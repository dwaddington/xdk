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
          @author Daniel Waddington
          @date: Aug 27, 2012
*/

#ifndef __OMNIOS_INTERFACES_H__
#define __OMNIOS_INTERFACES_H__

#include <types.h>
#include <utils.h>

#include "../ahci/ahci-itf-cli.h"
#include "measure.h"
#include <file/types.h>

typedef Module::Ahci::Block_device_proxy ahci_device_t;


/** 
 * Session interface for block devices
 * 
 */
class block_device_session_t
{
public:
  virtual status_t read(aoff64_t offset, unsigned byte_count, void * buffer) = 0;
  virtual status_t dummy_read(aoff64_t offset, unsigned byte_count, void * buffer) = 0;
  virtual status_t write(aoff64_t offset , unsigned byte_count, void * buffer) = 0;
};

#include <basicstring.h>

template <class T, class U, unsigned S>
class block_device_t : public block_device_session_t
{
private:
  T   _block_device;
  U * _block_device_proxy;

  void * _buffer;
  IPC_handle _ipc_handle;
  Spin_lock _lock;

public:
  // ctor
  block_device_t(const char * service_name) : _block_device(service_name) {

    /* allocate shared memory */
    char myid[32];
    myid[0]='\0';
    strcat(myid,"channel_");
    strcat(myid,itoa(env()->self_id()));

    status_t s = env()->shmem_alloc(S,
                                    &_buffer, 
                                    myid);
    //                                    false /* non-cached */);

    assert(s==S_OK);

    /* open session */
    Shmem_handle shmem_handle(myid);
    
    s = _block_device.open(0,               /* port */
                           shmem_handle,    /* shared memory identifier */
                           _ipc_handle);    /* return handle for block session IPC */
    
    /* open up block device session */
    _block_device_proxy = new Module::Ahci::Block_device_session_proxy("ahci_block_0"); 
    assert(_block_device_proxy != NULL);
  }

public:

  status_t read(aoff64_t offset, unsigned byte_count, void * outbuffer) {

    assert(_block_device_proxy);
    assert(outbuffer);

    aoff64_t block_start_offset = offset & (~0x1ff); /* assumes block size of 512 */
    unsigned delta = (offset - block_start_offset);
    unsigned modified_byte_count = byte_count + delta;

    // info("actual byte count = %u\n",byte_count);
    // info("actual start address = 0x%llx\n",offset);
    // info("block start address = 0x%llx\n",block_start_offset);
    // info("delta = 0x%x\n",delta);
    // info("modified_byte_count = %u\n",modified_byte_count);
    {
      Lock_guard guard(_lock);
      
      //      memset(_buffer,0xf,S);
      _block_device_proxy->read(block_start_offset,modified_byte_count);
      //      hexdump(_buffer,1024);
      /* copy memory into client space */
      __builtin_memcpy(outbuffer,((byte*)_buffer)+delta,byte_count);

    }

    return S_OK;
  }  

  status_t dummy_read(aoff64_t offset, unsigned byte_count, void * outbuffer) {

    assert(_block_device_proxy);
    assert(outbuffer);

    aoff64_t block_start_offset = offset & (~0x1ff);
    unsigned delta = offset - block_start_offset;
    unsigned modified_byte_count = byte_count + delta;
    char tmp[512];
    {
      Lock_guard guard(_lock);

      //      panic("**byte_count = %u\n",byte_count);      
#ifdef DUMMY
      PRINT_TIME("call dummy_read on proxy",
      _block_device_proxy->dummy_read(block_start_offset,modified_byte_count);
                 );
#else
      _block_device_proxy->read(block_start_offset,modified_byte_count);
#endif
      //      panic("after **byte_count = %u\n",byte_count);      
      //      PRINT_TIME("memory copy",
      /* copy memory into client space */
      //                 __builtin_memcpy(outbuffer,((byte*)_buffer)+delta,byte_count);
      __builtin_memcpy(tmp,((byte*)_buffer)+delta,512);
      //                 );
      
    }

    return S_OK;
  }  

  status_t write(aoff64_t offset , unsigned byte_count, void * inbuffer) {
    assert("not ready"==0);
    assert(_block_device_proxy);
    assert(inbuffer);
    Lock_guard guard(_lock);
    memcpy(_buffer,inbuffer,byte_count);
    _block_device_proxy->write(offset,byte_count);
    return S_OK;
  } 

  //  block_device_session_t  * session() { return (block_device_session_t *)_block_device_proxy; }
};

typedef block_device_session_t * block_service_t;

#endif // __OMNIOS_INTERFACES_H__
