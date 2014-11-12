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

#ifndef __EXT2FS_IPC_H__
#define __EXT2FS_IPC_H__

#include <thread.h>
#include <basicstring.h>

#include "ext2fs-itf-srv.h"
#include "ext2fs.h"

#define BOOTSTRAP_IPC_NAME "ext2fs"
#define SESSION_IPC_NAME "ext2fs_session_"

namespace Ext2fs {

  class Ext2fs_core;

  /**------------------------------------------------------------------------------
   * Interface implementation classes
   * 
   **------------------------------------------------------------------------------*/


  /** 
   * Filesystem_session_impl implements the session interface for the filesystem
   * 
   * 
   */
  class File;
  class Filesystem_session_impl : public Module::Ext2fs::Filesystem_session_interface_handler
  {
  private:
    Ext2fs_core *                 _core;
    void *                        _client_shmem;
    size_t                        _client_shmem_size;
    file_handle_t _curr_handle_index;

    struct File_handle : public List_element<File_handle> {
      Ext2fs::File * _file_obj;
      file_handle_t _handle;
    };

    List<File_handle> _active_handles;

    enum {
      MAX_ACTIVE_HANDLES = 255
    };

  public:
    Filesystem_session_impl() : _curr_handle_index(1) {
    }

    void hook_core(Ext2fs_core * c) { assert(c); _core = c; }

    /** 
     * Used to set parameters in the session implementation class (propogated from the bootstrap class)
     * 
     * @param client_shmem 
     * @param client_shmem_size 
     */
    void configure(Ext2fs_core * core, void * client_shmem, size_t client_shmem_size) {
      assert(core);
      assert(client_shmem);
      assert(client_shmem_size > 0);      
      _core = core;
      _client_shmem = client_shmem;
      _client_shmem_size = client_shmem_size;      
    }

  public: // interface methods
    status_t open_file(String pathname, unsigned long flags, file_handle_t & file );
    status_t close_file(file_handle_t file);
    status_t read_file_from_offset(file_handle_t file, unsigned long byte_count, offset_t offset);
    status_t read_file(file_handle_t file, unsigned long byte_count);
    status_t read_file_info(file_handle_t, file_info_t&);
    status_t read_directory_entries (String pathname, directory_query_flags_t flags, size_t & entries);
    status_t seek(file_handle_t file, offset_t offset);
  };

  /** 
   * Filesystem_session_ipc_service implements the active thread for IPC on the session interface
   * 
   */
  class Filesystem_session_ipc_service : public OmniOS::Thread
  {
  private:
    OmniOS::BString _label;

    struct Thread_params {
      Ext2fs_core * _core;
      void * _shmem;
      size_t _shmem_size;      
      Thread_params(Ext2fs_core * c, void * s, size_t ss) : _core(c),_shmem(s),_shmem_size(ss) {}
    };

  public:
    Filesystem_session_ipc_service(Ext2fs_core * core, unsigned session_id, void * shmem, size_t shmem_size) {

      /* build IPC endpoint label */
      {
        char label[32]; /* label for new IPC session */
        strcpy(label,SESSION_IPC_NAME);
        strcat(label,itoa(session_id));
        _label = label;
      }
      EXT2FS_INFO("new Filesystem_session_ipc_service on handle [%s]\n",_label.c_str());
      start((void*) new Thread_params(core, shmem, shmem_size));
    }

    void entry_point(void * params) {
      assert(params);
      Thread_params * p = (Thread_params *) params;
      
      IPC::Service_loop ipc_service_loop(_label);
      Module::Ext2fs::Filesystem_session_service<Filesystem_session_impl> _impl(ipc_service_loop);
      
      /* pass through initialization parameters */
      assert(p->_core);
      _impl.get_impl_class()->configure(p->_core,p->_shmem,p->_shmem_size);
      delete p;
                                        
      /* call IPC servicing loop */
      ipc_service_loop.run();
    }

    const char * label() { return _label.c_str(); }

  };


  /** 
   * Filesystem_impl implements the bootstrap interface for the filesystem
   * 
   * 
   */
  class Filesystem_impl : public Module::Ext2fs::Filesystem_interface_handler
  {
  private:
    enum { MAX_SESSIONS=32 };
    Filesystem_session_ipc_service * _sessions[MAX_SESSIONS];
    Ext2fs_core * _core;
    unsigned _num_sessions;

  public:

    Filesystem_impl() : _core(NULL),_num_sessions(0) {
      __builtin_memset(_sessions,0,sizeof(_sessions));
    }

    void hook_driver(Ext2fs_core * core) { _core = core; }

  public: // interface methods


    /** 
     * Open a session: instantiate a new IPC endpoint for the Filesystem_session interface
     * 
     * @param shmem Shared memory to be used for the session
     * @param [out] ipc_endpoint New IPC endpoint handle
     * 
     * @return S_OK on success.
     */
    status_t open_session(Shmem_handle shmem , IPC_handle & ipc_endpoint) {
      EXT2FS_INFO("open_session: shmem=%s\n", shmem.c_str());

      /* get hold of shared memory */
      void * buffer_virt = NULL;
      size_t buffer_size = 0;
      status_t s = env()->shmem_map(shmem.c_str(), &buffer_virt, &buffer_size);
      if(s!=S_OK) { assert(0); return s; }

      /* instantiate new IPC session */
      assert(_sessions[_num_sessions] == 0);
      assert(_core);
      _sessions[_num_sessions] = new Filesystem_session_ipc_service(_core,_num_sessions, buffer_virt, buffer_size);

      /* out parameter is ipc label */
      ipc_endpoint.set(_sessions[_num_sessions]->label());
      _num_sessions++;

      return S_OK;
    }


    status_t close_session(IPC_handle /*ipc_endpoint*/) {
      panic("close_session not yet implemented");
      return S_OK;
    }
  };

  class Filesystem_ipc_service : public OmniOS::Thread
  {
  private:
    Ext2fs_core * _core;
  public:
    Filesystem_ipc_service(Ext2fs_core * core) : _core(core) {
      start(NULL); /* start IPC thread */
    }

    void entry_point(void *) {
      IPC::Service_loop ipc_service_loop(BOOTSTRAP_IPC_NAME);
      Module::Ext2fs::Filesystem_service<Filesystem_impl> _impl(ipc_service_loop);
      
      _impl.get_impl_class()->hook_driver(_core); /* hook in the actual driver code to the skeleton */
      
      ipc_service_loop.run(); /* call IPC servicing loop */
    }
  };


}

#endif // __EXT2FS_IPC_H__

