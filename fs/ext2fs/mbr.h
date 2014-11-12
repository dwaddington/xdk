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

#ifndef __OMNIOS_MBR_H__
#define __OMNIOS_MBR_H__

#include <printf.h>
#include "block_device.h"
#include "byteorder.h"

class Mbr
{
private:
  enum {
    N_PRIMARY       = 4,
    BR_SIGNATURE    = 0xAA55
  };

  struct Partition_entry {
    uint8_t _boot;
    uint8_t _starting_head;
    unsigned _starting_sector : 6;
    unsigned _starting_cylinder : 10;
    uint8_t _system_id;
    uint8_t _ending_head;
    unsigned _ending_sector : 6;
    unsigned _ending_cylinder : 10;
    uint32_t _lba_first_block;
    uint32_t _partition_length;
    
  } __attribute__((packed));

  struct {
    uint8_t _bootcode[440];
    uint32_t _media_id;
    uint16_t _pad0;
    Partition_entry _pte[N_PRIMARY];
    uint16_t _signature;    
  } __attribute__((packed)) _mbr;

public:
  Mbr(block_device_session_t * block_device) {
    assert(sizeof(_mbr)==512);
    memset(&_mbr,0,sizeof(_mbr));
    assert(block_device);
    /* read in first sector of device */
    block_device->read(0,512,(void*)&_mbr);
    dump_mbr_info();
    assert(_mbr._signature == BR_SIGNATURE);
  }

  /** 
   * Read the logical block address of a partition 
   * 
   * @param type Type of partition (0x83 = Native linux)
   * @param instance 0 is first instance and so on
   * 
   * @return 
   */
  block_off_t get_partition_lba(unsigned type, unsigned instance) {
    for(unsigned p=0;p<N_PRIMARY;p++) {
      if(_mbr._pte[p]._system_id == type) {
        if(instance == 0) return uint32_t_le2host(_mbr._pte[p]._lba_first_block);
        else instance--;
      }
    }
    assert(0);
    return 0;
  }

  void dump_mbr_info() 
  {
    info("MBR::signature (0x%x)\n",_mbr._signature);
    for(unsigned p=0;p<N_PRIMARY;p++) {
      info("\tPartition: boot(%x) systemid(%x) LBA (0x%x)\n",
           _mbr._pte[p]._boot,
           _mbr._pte[p]._system_id,
           _mbr._pte[p]._lba_first_block);
    } 
  }
};

#endif // __OMNIOS_MBR_H__
