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
  Copyright (C) 2013, Juan A. Colmenares <juan.col@samsung.com>
  Copyright (C) 2015, Daniel G. Waddington <d.waddington@samsung.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <fstream>

#include <common/cycles.h>
#include <common/assert.h>

#include <sys/utsname.h>

#if defined(__i386__) || defined(__x86_64__)

double get_tsc_frequency_in_mhz() {
  FILE *fp;
  char output[128];
  double tsc_freq_in_MHz = 0;

  struct utsname ver;
  if (uname(&ver) < 0) {
    panic("uname(...) failed!");
  }

  // PLOG("Kernel version: %s", ver.release); 
  
  int major, minor;
  sscanf(ver.release, "%d.%d%*s", &major, &minor);
  //PLOG("Kernel version: %d.%d", major, minor); 

	//check if we can get frequency from dmesg
	//if not, read it from /proc/cpuinfo
	int find_from_dmesg;
	fp = popen("`which dmesg` | `which grep` \"Detected.*MHz\" | `which wc` -l", "r");
	assert(fp != NULL);
	if (fgets(output, sizeof(output), fp) != NULL) {
		find_from_dmesg = atoi(output);
	}
	pclose(fp);

  if (find_from_dmesg && major >= 3) {
    // Open the command for reading the cpu frequency in MHz from dmesg command.
    if (minor >= 8) {
      fp = popen("`which dmesg` | `which grep` \"Detected.*MHz\" | `which awk` '{print $5}'", "r");
    }
    else {
      fp = popen("`which dmesg` | `which grep` \"Detected.*MHz\" | `which awk` '{print $4}'", "r");
    }

    assert(fp != NULL);

    if (fgets(output, sizeof(output), fp) != NULL) {
      tsc_freq_in_MHz = atof(output);
      assert(tsc_freq_in_MHz > 0);
    }
  }
  else {
    // Open the command for reading the cpu frequency in GHz from /proc/cpuinfo
    // file.
    //fp = popen("`which awk` '{ if ($2==\"name\")print $9 }' /proc/cpuinfo | `which uniq` | `which cut` -d \\G -f 1", "r");
    fp = popen("`which cat` /proc/cpuinfo | `which grep` \"model name\" | `which uniq` | `which cut` -d \\@ -f 2", "r");
    assert(fp!=NULL);

    char * result = fgets(output, sizeof(output), fp);
    if(result == NULL) {
      PWRN("cannot parse /proc/cpuinfo");
    }
    else {
      tsc_freq_in_MHz = atof(output);
      assert(tsc_freq_in_MHz > 0);

      // As the value is in GHz, convert it to MHz.
      tsc_freq_in_MHz = tsc_freq_in_MHz * 1000;
    }
  }

  pclose(fp);
  return tsc_freq_in_MHz;
}

#elif defined(__arm__)
double get_tsc_frequency_in_mhz() {
  
  std::ifstream ifs("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
  std::string line;
  std::getline(ifs, line);

  double result = strtod(line.c_str(), NULL);
  return result / (1000.0);
}
#endif 
