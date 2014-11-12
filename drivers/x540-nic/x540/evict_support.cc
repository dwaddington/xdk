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















/** 
 * @author Reza Dorrigiv <r.dorrigiv@samsung.com>
 * @author Juan A. Colmenares <juan.col@samsung.com>
 * @date September 10, 2013.
 */

#include "evict_support.h"

using namespace Exokernel;

bool __verbose = false; // true; // 

// Allows us to trigger eviction prematurely, just for debugging.
#define FORCE_EVICTION (0) // (1) // 
#define FORCED_EVICTION_THRESHOLD (1607155) // (1607170) // 

int num_frames_to_evict(unsigned total_avail, 
                        int threshold) {
  int num_avail_frames = total_avail;
  int diff = threshold - num_avail_frames; 
 
  if (__verbose) {
    if (diff >= 1000) {
      PLOG("threshold: %d and number of available frames: %d. diff = %d", 
           threshold, num_avail_frames, diff);
    }
  }

  return diff;  
}


bool evict_needed(pbuf_t* pbuf_list, 
                  int& num_frames_2_evict, 
                  unsigned total_avail, 
                  int threshold) {
  bool evict = false;

  // Figure out the type of request 
  mcd_binprot_header_t* app_hdr = (mcd_binprot_header_t*)(pbuf_list->pkt + NETWORK_HDR_SIZE);

  // For requests that might lead to eviction, find out whether eviction is required.
  if (operation_involves_eviction(app_hdr->opcode)) {
    num_frames_2_evict = num_frames_to_evict(total_avail, threshold);
    
    // No eviction is required
    if (num_frames_2_evict <= 0){
      num_frames_2_evict = 0;
    }
    else {
      //PLOG("number of frames to evict: %d", num_frames_2_evict);
      evict = true;
    }
  }  
  
  return evict;
}

int calculate_threshold(int channel_size, int channel_num, int request_rate, int eviction_period) {
#if (FORCE_EVICTION == 0)
  int num_active_frames = (2 * channel_size + 3) * channel_num;
  int frames_needed = request_rate * eviction_period / 1000;
  frames_needed -= num_active_frames;
  return frames_needed;
#else
  return FORCED_EVICTION_THRESHOLD;
#endif 
}

