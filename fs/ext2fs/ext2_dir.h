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

#ifndef __EXT2_DIRECTORY_H__
#define __EXT2_DIRECTORY_H__

#include <types.h>
#include <list.h>
#include "block_device.h"
#include "byteorder.h"

namespace Ext2fs
{
  enum { EXT2_NAME_LEN = 255 };

  struct Directory_entry
  {
  private:

    struct {
    /* strict packed format - do not add additional member variables */
    uint32_t inode; 
    uint16_t rec_length; /* defines offset to the next record */
    uint8_t  name_length; 
    uint8_t  file_type;
    char     name[EXT2_NAME_LEN];
    } __attribute__((aligned(4),packed));
  
  public:
    /* endian safe accessors */
    uint32_t get_inode() { return uint32_t_le2host(inode); }
    uint16_t get_record_length() { return uint16_t_le2host(rec_length); }
    uint8_t get_name_length() { return name_length; }

    BString get_name() { 
      char tmpnam[EXT2_NAME_LEN];
      tmpnam[0]='\0';
      strncpy(tmpnam, name, get_name_length());
      tmpnam[get_name_length()]='\0'; 
      return BString(tmpnam);
    }
    uint8_t get_file_type() { return file_type; }

    Directory_entry * next() { 
      byte * p = (byte *) &inode;
      p += get_record_length();
      return ((Directory_entry *)p);
    }

  public:

    /** 
     * Dump a directory Inode for debugging 
     * 
     */
    void dump() 
    {
      info("\tType:%x Inode:0x%x Record_length:%u bytes Name:[%s] Namelen:%u\n",
           get_file_type(),
           get_inode(),
           get_record_length(),
           get_name().c_str(),
           get_name_length());
    }
  };
}

#endif
