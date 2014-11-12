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











#ifndef __EVICT_SUPPORT_H__
#define __EVICT_SUPPORT_H__

/** 
 * @author Reza Dorrigiv <r.dorrigiv@samsung.com>
 * @author Juan A. Colmenares <juan.col@samsung.com>
 * @date September 10, 2013.
 */

#include <libexo.h>
#include <mcd_binprot.h>
#include <network_stack/protocol.h>
//#include <x540/x540_device.h>

using namespace Exokernel;

/**
 * Node of a list of lists of frames (AKA 'pbuf's).
 * This is part of eviction support in the transmit thread.   
 */
struct Frames_list_node : public List_element<Frames_list_node> {

  /** Pointer to (the head element of) a list of frames. */
  pbuf* frame_list; 

  /** Default Constructor. */
  Frames_list_node() : frame_list(NULL) {} 

  /** Constructor. */
  Frames_list_node(pbuf* list_of_frames) : frame_list(list_of_frames) {} 

  /** Resets the pointer to the list of frames. */
  void reset() { frame_list = NULL; }

} __attribute__((aligned(64)));


/** 
 * Calculates the number of frames that should be evicted
 * @parame [in] the total available blocks in an allocator for a socket (NIC)
 * @parame [in] threshhold the minimum number of frames that should be available at all times
 * @retval number of frames that should be evicted, if negative no eviction is required
 */
int num_frames_to_evict(unsigned total_avail, 
                        int threshhold);

/** 
 * Tells whether or not the given operation involves the evition logic. 
 * @param opcode operation code.
 * @return true if the operation involves the evition logic; otherwise, false.
 */
inline static
bool operation_involves_eviction(int opcode) {
  // TODO: When improving KVCache, check if other opcodes can trigger eviction.
  return (opcode == SET      || 
          opcode == ADD      || 
          opcode == REPLACE); 
}

/** 
 * Decides whetehr an eviction is required and if yes, how many frames are recommended for
 * eviction.
 * @param pbuf_list 
 * @param [out] num_frames_2_evict number of frames to be evicted       
 * @parame [in] the total available blocks in an allocator for a socket (NIC) 
 * @parame [in] threshhold the minimum number of frames that should be available at all times
 * @retval True is an eviction is required
 */
bool evict_needed(pbuf_t* pbuf_list, 
                  int& num_frames_2_evict,
                  unsigned total_avail, 
                  int threshold);
 
/**
 * Calculates the minimum number of frames needed to support the incoming requests.
 * @param [in] channel_size the size of each channel
 * @param [in] channel the number of channels
 * @param [in] request_rate the rate of requests (in requests per second)
 * @param [in] eviction_period eviction period in milliseconds.       
 * @retval the number of frames.
 */
int calculate_threshold(int channel_size, int channel_num, int request_rate, int eviction_period); 

#endif
