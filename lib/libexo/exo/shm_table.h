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
  Copyright (C) 2013, Jilong Kuang (jilong.kuang@samsung.com)
*/

#ifndef __SHM_TABLE_H__
#define __SHM_TABLE_H__

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <string>
#include <iostream>

#include <cassert>
#include <stdlib.h>
#include <stdio.h>
#include "spinlocks.h"
#include <string.h>

namespace Exokernel {

typedef uint32_t smt_type_t;
typedef uint32_t smt_sub_type_t;

enum {
  SHM_TABLE_ENTRY_VALUES_PER_ROW = 7
};

/** Entry (or row) in the shared memory table. */
typedef struct {
  smt_type_t type_id;
  smt_sub_type_t sub_type_id;
  uint64_t value[SHM_TABLE_ENTRY_VALUES_PER_ROW];
} shm_table_entry_t;


/** Control section of the shared memory table. */
typedef struct {
  Spin_lock lock;
  size_t size;
  unsigned next_row;
  unsigned id;
} shm_table_control_t;


#define SHM_TABLE_PREFIX "shm_mem_table"
#define SHM_TABLE_ENTRY (1024)
#define SHM_TABLE_SIZE  ((sizeof(shm_table_entry_t) * SHM_TABLE_ENTRY) + sizeof(shm_table_control_t))


/** 
 * Shared memory table for global lookup.
 * The shared memory table consists of multiple rows. Each row is 64 bytes long
 * and of type shm_table_entry_t.
 * @warning{Use the table's lock to protect reads and writes.} 
 */ 
class Shm_table {
private:
  char _shm_table_name[64];
  void * _ptr;
  Spin_lock * _lock;
  size_t * _size;
  unsigned * _next_row;
  unsigned * _id;  

public:

  /** 
   * Constructor.
   * @param need_alloc tells whether or not the shared memory are must be
   * created.
   * @param id identifier of the table.
   */
  Shm_table(bool need_alloc, unsigned id) {
    _ptr = NULL;
    _size = NULL;
    _lock = NULL;
    _next_row = NULL;
    _id = NULL;

    __init(need_alloc, id);
  }


  /** Destructor */
  virtual ~Shm_table() {
    shm_unlink(_shm_table_name);
  }

  void* get_table_addr() { 
    return (void *)((addr_t)_ptr + sizeof(shm_table_control_t)); 
  }

  /** Acquires the table's lock. */
  void lock() { _lock->lock(); }

  /** Releases the table's lock. */
  void unlock() { _lock->unlock(); }


  unsigned get_row_number() { 
    assert(*_next_row >= 0);
    assert(*_next_row < SHM_TABLE_ENTRY);
    return *_next_row; 
  }

  size_t get_table_size() { return *_size; }

  unsigned get_table_id() { return *_id; }

  void write_shm_table(unsigned row, shm_table_entry_t * st) {    
    assert(st);
    assert(row >=0);
    assert(row == *_next_row);
    shm_table_entry_t* dest = 
      (shm_table_entry_t*) ((addr_t)get_table_addr() + row * sizeof(shm_table_entry_t));
 
    __builtin_memcpy(dest, st, sizeof(shm_table_entry_t));

    // update the next available row number.
    *_next_row = *_next_row + 1;
  }

  /** Reads a given row of the table. */
  shm_table_entry_t* read_row(unsigned row) {
    assert(row >= 0);
    assert(row < *_next_row);
    shm_table_entry_t* dest = 
      (shm_table_entry_t *) ((addr_t)get_table_addr() + row * sizeof(shm_table_entry_t));
    return dest;
  }


  int find_row_number(smt_type_t tid, smt_sub_type_t sid) {
    shm_table_entry_t * stt = (shm_table_entry_t *)get_table_addr();
    for (unsigned i = 0; i < *_next_row; i++) { 
      if ((stt->type_id == tid) && (stt->sub_type_id == sid)) {
        return i;
      }
      else {
        stt++;
      }
    }
    printf("Error: no such entry in the shared table!\n");
    return -1;
  }

  void show_table_info() {
    printf("Shared Table %d Control Block:\n", *_id);
    printf("Shared region address %p\n",_ptr);
    printf("Shared region size %lu\n",*_size);
    printf("The next row %u\n", *_next_row);
  }

private: 

  /** 
   * Initializes the shared memory table. 
   */
  void __init(bool need_alloc, unsigned id) {
    memset(_shm_table_name, 0, 64);
    sprintf(_shm_table_name, "/%s_%d", SHM_TABLE_PREFIX, id);

    // Create shared memory object and set its size.
    int fd;
    if (need_alloc) {
      fd = shm_open(_shm_table_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
    } else {
      fd = shm_open(_shm_table_name, O_RDONLY, S_IRUSR | S_IROTH);
    }

    if (fd == -1) {
      std::cerr << "Error: shm_open!" << std::endl;
      fprintf(stderr, "[XDK Shm_table] Open failed:%s\n", strerror(errno));
      exit(1);
    }

    if (need_alloc) {
      if (ftruncate(fd, SHM_TABLE_SIZE) == -1) {
        std::cerr << "Error: ftruncate!" << std::endl;
        fprintf(stderr, "[XDK Shm_table] Truncate failed:%s\n", strerror(errno));
        exit(2);
      }
    
      _ptr = mmap(NULL, SHM_TABLE_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd, 0);
    } else {
      _ptr = mmap(NULL, SHM_TABLE_SIZE,
                  PROT_READ,
                  MAP_SHARED,
                  fd, 0);
    }
    assert(_ptr != NULL);

    if (_ptr == MAP_FAILED) {
      std::cerr << "Error: mmap!" << std::endl;
      fprintf(stderr, "[XDK Shm_table] Mmap failed:%s\n", strerror(errno));
      exit(3);
    }

    _next_row = (unsigned *)((addr_t)(_ptr) + sizeof(Spin_lock));
    _size = (size_t *)((addr_t)(_next_row) + sizeof(unsigned));
    _id = (unsigned *)((addr_t)(_size) + sizeof(size_t));

    if (need_alloc) {
      __builtin_memset(_ptr, 0, SHM_TABLE_SIZE);

      _lock = new(_ptr) Spin_lock();
      *_next_row = 0;
      *_size = SHM_TABLE_SIZE;
      *_id = id;
    }
    else {
      _lock = (Spin_lock *)(_ptr);
    }
  }

};

}
#endif
