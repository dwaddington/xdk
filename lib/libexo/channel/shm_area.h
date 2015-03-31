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

#ifndef __SHM_AREA_H__
#define __SHM_AREA_H__

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <string>
#include <iostream>

#include <cassert>
#include <stdlib.h>
#include <stdio.h>


#define NAME_PREFIX "xdk"
#define SHM_BIND_ADDRESS_64BITS   0x0000050000000000
#define SHM_INTERVAL              0x1000000000


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
     *	return the virtual address of the shared memory
     */
    void*	init(unsigned int key, size_t size, bool need_alloc) {

      // TODO: Improve error management!

      sprintf(_shm_area_name, "/%s_%d", NAME_PREFIX, key);

      // Create shared memory object and set its size.
      int fd = shm_open(_shm_area_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
      if (fd == -1) {
        std::cerr << "Error: shm_open!" << std::endl;
        exit(1);
      }

      if (need_alloc) {
        if (ftruncate(fd, size) == -1) {
          std::cerr << "Error: ftruncate!" << std::endl;
          exit(2);
        }
      }

      // Map shared memory object.
      void* addr = (void*)(SHM_BIND_ADDRESS_64BITS + key * SHM_INTERVAL);

      //(MAP_FIXED)?
      _ptr = mmap(addr, size, 
                  PROT_READ | PROT_WRITE, 
                  MAP_SHARED | MAP_FIXED, 
                  fd, 0);
      //_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (_ptr == MAP_FAILED) {
        std::cerr << "Error: mmap!" << std::endl;
        exit(3);
      }

      if (need_alloc) {
        __builtin_memset(_ptr, 0, size);
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
