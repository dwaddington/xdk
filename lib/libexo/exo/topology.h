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

#ifndef __EXOKERNEL_TOPOLOGY_H__
#define __EXOKERNEL_TOPOLOGY_H__

#include <numa.h>
#include "types.h"

/* 
   Wrappers and APIs to consolidate CPU, memory and device topology discovery
*/

namespace Exokernel
{
  namespace CPU {
    /** 
     * Return the number of configured (on-line and off-line CPUs)
     * 
     * 
     * @return Configured CPU count 
     */
    unsigned num_configured();


    /** 
     * Determine the number of logical cores (HTs) on line
     * 
     * 
     * @return Number of online CPUs
     */
    unsigned num_cpus_online();


    /** 
     * Get the Local APIC id for a given logical core
     * 
     * @param core Logical core id
     * 
     * @return APIC id or -1 on error
     */
    signed get_apic_id(core_id_t core);


    /** 
     * Get the Hyper-Threading partner for a given logical core
     * 
     * @param core Logical core of which to determine partner
     * 
     * @return Partner's logical core id
     */
    signed get_ht_partner(core_id_t core);
  
  }
}

#endif // __EXOKERNEL_TOPOLOGY_H__
