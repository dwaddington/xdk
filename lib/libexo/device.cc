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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>

#include <string>
#include <iostream>
#include <fstream>

#include "exo/device.h"

__thread Exokernel::Device::fd_cache_t Exokernel::Device::_cached_fd = {0,0};

/** 
 * Query the /sys/class/parasite/pkXXX entry.  
 * 
 * @param pk_fname 
 * 
 * @return True if both vendor and device id match
 */
static bool check_device_match(std::string pk_fname,
                               unsigned vendor,
                               unsigned device,
                               std::string& pci_node_name) {
  pk_fname += "/pci";

  PDBG("Opening device %s", pk_fname.c_str());

  FILE * pkf = fopen(pk_fname.c_str(), "r");

  if (pkf == NULL)
    throw Exokernel::Fatal(__FILE__, __LINE__, "fopen error");

  /* read /sys/class/parasite/pkXXX/pci */
  char pci_addr[64];
  unsigned cnt = fread(&pci_addr, 1, 64, pkf);
  assert(cnt < 64);
  pci_addr[cnt]='\0';
  if (cnt > 0)
    pci_addr[cnt-1]='\0';  // trim NL

  fclose(pkf);

  /* pci_node_name is /sys/bus/pci/devices/xxxx for the specific device */
  pci_node_name = pci_addr;

  /* check vendor value */
  {
    const std::string vendor_node = pci_node_name + "/vendor";

    try {
      std::ifstream ifs;
      ifs.open(vendor_node.c_str());
      unsigned val;
      ifs >> std::hex >> val;

      if (val != vendor)
        return false;
    }
    catch(...) {
      throw Exokernel::Fatal(__FILE__, __LINE__, "unexpected condition");
    }
  }

  /* check device id value */
  {
    const std::string device_node = pci_node_name + "/device";

    try {
      std::ifstream ifs;
      ifs.open(device_node.c_str());
      unsigned val;
      ifs >> std::hex >> val;

      if (val != device)
        return false;
    }
    catch(...) {
      throw Exokernel::Fatal(__FILE__, __LINE__, "unexpected condition");
    }
  }

  return true;
}


/** 
 * Locate the device and set member _sys_fs_root_name
 * 
 * @param instance Device instance counting from 0
 * @param vendor Vendor id
 * @param device Device id
 * 
 * @return True if a device is matched. Name is written to _sys_fs_root_name.
 */
bool
Exokernel::Device::
__locate_device(unsigned instance, unsigned vendor, unsigned device) {
  DIR *pdir;

  bool auto_load_attempted = false;

 retry:
  pdir = opendir("/sys/class/parasite/");
  if (pdir == NULL) {
    PLOG("Parasitic kernel not loaded.");
    return false;
  }

  if(auto_load_attempted)
    PINF("Re-scanning for device 0x%x:0x%x ...",vendor, device);
  else
    PINF("Scanning for device 0x%x:0x%x ...",vendor, device);

  try {
    struct dirent *entry;
    unsigned inst = 0;

    /* iterate through directory */
    while ((entry = readdir(pdir))) {
      if (strncmp(entry->d_name, "pk", 2) == 0) {
        /* check device vendor and deviceid match */
        std::string fname = "/sys/class/parasite/";
        std::string pci_node_name;
        fname += entry->d_name;
        if (check_device_match(fname, vendor, device, pci_node_name)) {
          if (inst == instance) {
            /* found our device */
            _sys_fs_root_name = fname;
            _sys_fs_pci_root_name = pci_node_name;
            _pk_device_name = entry->d_name;

            // TODO: need to wait for finalization of PK module set up
            //usleep(500);

            closedir(pdir);
            return true;
          } else {
            inst++;
          }
        }
      }
    }
    closedir(pdir);
  }
  catch(Exokernel::Exception& e) {
    throw e;
  }
  catch(Exokernel::Fatal& e) {
    e.abort();
  }

  /* device not found, attempt auto-load */
  if(!auto_load_attempted)
  {
    PINF("Attempting to auto-load device into PK module.");
    try {
      std::ofstream fs;
      
      //      fs.open("/sys/bus/pci/drivers/parasite/new_id");  
      fs.open("/sys/class/parasite/new_id");  /* non-root access */
      fs << std::hex << vendor << " " << std::hex << device << std::endl;
      fs.close();

      auto_load_attempted = true;
      goto retry;
    }
    catch(...) {
      throw Exokernel
        ::Exception("Unable to open parasite dir (/sys/class/parasite)");

      return false;
    }    
  }

  /* device not found */
  PDBG("Device (%x,%x) not found", vendor, device);
  _sys_fs_root_name.clear();
  return false;
}
