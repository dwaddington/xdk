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
  Authors:
  Copyright (C) 2013, Changhui Lin <changhui.lin@samsung.com>
*/

#ifndef __EXO_SHM_AREA_H__
#define __EXO_SHM_AREA_H__

#include <libexo.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <string>
#include <iostream>

#include <cassert>
#include <stdlib.h>
#include <stdio.h>

namespace Exokernel {

/** Encapsulates a shared memory area. */
class Shm_area {
private:
  char _shm_area_name[256];
  void*	_ptr;
  size_t _size;

public:

  /** Default constructor. */    
  Shm_area() : _ptr(NULL) { }

  /** Destructor. */    
  ~Shm_area() {
    shm_unlink(_shm_area_name);
  }

  /**
   *	Initializes the shared memory area, and attaches it to a specified virtual
   *	address based on a key.
   *	@param key a value used to generate the handle for the shared memory
   *             object.
   *	@param size the size of the created shared memory area.
   *	@param need_alloc whether the shared memory is newly created
   *	@param numa_node The numa node where the memory is allocated
   *	return the virtual address of the shared memory
   */
  void*	init(unsigned int key, size_t size, bool need_alloc, unsigned numa_node) {

    void* _ptr = NULL;
    if (size < MB(2))
      size = MB(2);

    if (need_alloc) {    
      int handle = -1;
      try {
        _ptr = Exokernel::Memory::huge_shmem_alloc(key, 
                                                   size, 
                                                   numa_node,
                                                   &handle, 
                                                   NULL);
      }
      catch(Exokernel::Exception e) {
        printf("huge_shmem_alloc error: %s\n",e.cause());
      }
      assert(_ptr);

      __builtin_memset(_ptr, 0, size);

      } 
    else {
      try {
        _ptr = Exokernel::Memory::huge_shmem_attach(key, size, NULL);
      }
      catch (Exokernel::Exception e) {
        printf("PERR: %s\n",e.cause());
      }
      assert(_ptr);
    }

    _size = size;

    return _ptr;
  }

  /** Returns the size of the shared memory area. */
  int	get_size() { return _size; }

  /** Returns a pointer to the start of the shared memory area. */
  void* get_ptr() { return _ptr; }

  /** Returns the name of the shared memory area. */
  const std::string get_name() { return _shm_area_name; }

  /** Prints on screen information about the shared memory area. */
  void dump(){
    std::cout << "*********************" << std::endl
      << "Shared memory object:" << std::endl
      << "name = " << get_name() << std::endl
      << "size = " << get_size() << std::endl
      << "start address = " << get_ptr()  << std::endl
      << "*********************" << std::endl;
  }

};

}

#endif
