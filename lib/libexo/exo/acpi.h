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
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __EXO_ACPI_H__
#define __EXO_ACPI_H__

#include <unistd.h>
#include "../private/__acpi_tables.h"

namespace Exokernel
{
  class ACPI 
  {
  private:
    enum {
      _verbose = false
    };

  public:
    ACPI() {
      /* check for ACPI support */
      if(access("/sys/firmware/acpi/tables", F_OK ) == -1) {
        if(_verbose)
          PERR("/sys/firmware/acpi/tables cannot be accessed.");
        throw Exokernel::Exception("cannot access /sys/firmware/acpi/tables");
      } 
      else {
        if(_verbose) 
          PLOG("/sys/firmware/acpi/tables check OK.");
      }
    }

    
    Exokernel::SRAT_table * open_table_SRAT() {

      SRAT_table * tbl = new SRAT_table;

      /* open /sys/firmware/acpi/tables/SRAT */
      int fd = open("/sys/firmware/acpi/tables/SRAT",O_RDONLY);
      if(fd == -1) {
        if(_verbose)
          PERR("/sys/firmware/acpi/tables/SRAT cannot be accessed.");
        throw Exokernel::Exception("cannot access /sys/firmware/acpi/tables/SRAT");
      }
      else {
        if(_verbose)
          PLOG("opened SRAT table OK.");
      }

      /* get file size */
      struct Acpi_table_layouts::DESCRIPTION_HEADER desc;
      {
        ssize_t s = read(fd, &desc, sizeof(Acpi_table_layouts::DESCRIPTION_HEADER));
        assert(s!=-1);
      }
      assert(strncmp(desc._signature,"SRAT",4)==0);
      lseek(fd,0,SEEK_SET);           
    
      /* allocate memory for the table */
      tbl->allocate_base(desc._length);
    
      /* read in the table */
      {
        size_t s = read(fd,tbl->base(), desc._length);
        assert(s == desc._length);
      }
      tbl->parse(tbl->base());
      return tbl;
    }

  };
}
#endif  // __EXO_ACPI_H__
