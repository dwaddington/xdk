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

#ifndef __NVME_COMMAND_IO_H__
#define __NVME_COMMAND_IO_H__

#include <vector>
#include <common/types.h>
#include <common/dump_utils.h>

#include "nvme_command_structs.h"
#include "nvme_command_admin.h"
#include "nvme_device.h"
#include "nvme_types.h"

class NVME_device;


class Command_io_flush
{
protected:
  NVME_IO_queue * _ioq;
  unsigned _cid;

public:
  Command_io_flush(Submission_command_slot * sc,
                   unsigned command_id,
                   unsigned nsid) {
    
    assert(sc);
    sc->clear();

    struct nvme_rw_command * c = (struct nvme_rw_command *) sc->raw();
    
    c->opcode = nvme_cmd_flush;
    c->nsid = nsid;
    c->command_id = _cid = command_id;

    PLOG("!!! issuing flush command");
  }
};


class Command_io_rw
{
protected:
  NVME_IO_queue * _ioq;
  unsigned _cid;

public:
  Command_io_rw(Submission_command_slot * sc,
                unsigned command_id,
                unsigned nsid,
                addr_t prp1, 
                off_t offset, 
                size_t num_blocks,
                bool sequential, 
                unsigned access_freq, 
                unsigned access_lat,
                bool iswrite) {

    assert(nsid==1); // 1 namespace for moment

    unsigned slot_id;
    // assert(sc);
    // sc->clear();

    struct nvme_io_dsm dsm;
    dsm.access_freq = access_freq;
    dsm.access_lat = access_lat;
    dsm.seq_request = sequential;
    dsm.incompressible = 0;

    struct nvme_rw_command * c = (struct nvme_rw_command *) sc->raw();
    
    c->opcode = iswrite ? nvme_cmd_write : nvme_cmd_read;
    c->nsid = nsid;
    c->command_id = _cid = command_id; //ioq->next_command_id();
    c->prp1 = prp1;
    c->slba = offset;
    c->length = num_blocks;

    __builtin_memcpy(&c->dsmgmt,&dsm,1);

    PLOG("!!! issuing (%s) command prp1=0x%lx nsid=%d slba=%ld control=0x%x dsmgmt=0x%x!!!\n",
           iswrite ? "write" : "read",
           c->prp1,
           c->nsid,
           c->slba,
           c->control,
           c->dsmgmt
         );
  }

};


#endif // __NVME_COMMAND_IO_H__
