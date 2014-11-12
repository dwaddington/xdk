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
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "exo/topology.h"
#include "exo/errors.h"

/** 
 * Convert string of the form "1,2,3" into a vector of signed
 * 
 * @param source 
 * @param delimiter 
 * 
 * @return 
 */
SUPPRESS_NOT_USED_WARN
static std::vector<signed> split_num_list(const std::string &source, 
                                          const char *delimiter = ",")
{
  std::vector<signed> tokens;

  size_t prev = 0;
  size_t next = 0;

  while ((next = source.find_first_of(delimiter, prev)) != std::string::npos) {
    if (next - prev != 0) {
      tokens.push_back(atoi(source.substr(prev, next - prev).c_str()));
    }
    prev = next + 1;
  }
  
  if (prev < source.size())
    tokens.push_back(atoi(source.substr(prev).c_str()));
  
  return tokens;
}

unsigned Exokernel::CPU::num_cpus_online() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}


/** 
 * Get the Local APIC id for a given logical core
 * 
 * @param core Logical core id
 * 
 * @return APIC id or -1 on error
 */
signed Exokernel::CPU::get_apic_id(core_id_t core)
{
  int curr_cpu = 0;
  try {

    /* extract info from /proc/cpuinfo */
    std::ifstream ifs("/proc/cpuinfo");
    
    std::string line;
    
    /* we could use regex but I don't want to depend on C++0x */
    while(std::getline(ifs,line)) {

      if(line.compare(0,9,"processor")==0) {        
        curr_cpu = atoi(line.substr(12).c_str());
      }
      else if(line.compare(0,6,"apicid")==0) {
        if((unsigned) curr_cpu == core)
          return atoi(line.substr(10).c_str());
      }
    }
  }
  catch(...) {    
      throw Exokernel::Fatal(__FILE__, __LINE__, "unexpected condition opening /proc/cpuinfo");
  }

  throw Exokernel::Fatal(__FILE__, __LINE__, "unexpected condition located apic id in /proc/cpuinfo");
  return -1;
}

/** 
 * Get the Hyper-Threading partner for a given logical core
 * 
 * @param core Logical core of which to determine partner
 * 
 * @return Partner's logical core id
 */
signed Exokernel::CPU::get_ht_partner(core_id_t core)
{
  /* note: we assume only one HT partner per core */
  try {
    /* extract from /sys/devices/system/cpu/cpuX/topology/thread_siblings */
    std::stringstream fname; 
    fname << "/sys/devices/system/cpu/cpu" << core << "/topology/thread_siblings";

    std::ifstream ifs(fname.str().c_str());
    
    unsigned int cpu_mask[8];
    char c;
    for(unsigned i=0;i<8;i++) {
      ifs >> std::hex >> cpu_mask[7-i];
      ifs >> c;
    }

    int offset = 0;
    for(unsigned j=0;j<8;j++) {
      if (cpu_mask[j] == 0) {
        offset+=32;
        continue;
      }
      else {
        /* clear our own bit */
        cpu_mask[j] &= ~(1 << (core-offset));
        return __builtin_ffs(cpu_mask[j])-1+offset;
      }
    }
    assert(0);
    return -1;
  }
  catch(...) {
    throw Exokernel::Fatal(__FILE__, __LINE__, "unexpected condition parsing /sys/devices/system/..");
  }
    
  return -2;
}
