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
  @author Jilong Kuang (jilong.kuang@samsung.com)
*/

#ifndef __NIC_FILTER_H__
#define __NIC_FILTER_H__

#include "x540_device.h"

using namespace Exokernel;

namespace Exokernel {

  class Nic_filter {
  private:
    Intel_x540_uddk_device * _card;
    int         _max_flow_filter;
    int         counter;

  public:
    Nic_filter(Intel_x540_uddk_device * card) {
      _card = card;
      _max_flow_filter = 128;
      /* disable all 128 flow filters */
      for (int i = 0; i < _max_flow_filter; i++) {
        disable_flow_filter(i);
      }
      counter = 0;
    }

    void disable_flow_filter(int i) {
      _card->_mmio->mmio_write32(IXGBE_SAQF(i),0);
      _card->_mmio->mmio_write32(IXGBE_DAQF(i),0);
      _card->_mmio->mmio_write32(IXGBE_SDPQF(i),0);
      _card->_mmio->mmio_write32(IXGBE_FTQF(i),0);
      _card->_mmio->mmio_write32(IXGBE_L34T_IMIR(i),0);
    }

    /** 
     * Setup a 5-tuple flow filter
     * 
     * @param i Flow filter index
     * @param queue Matching packet goes to this queue number
     * @param sip Source IP address (0: dont care)
     * @param dip Destination IP address (0: dont care)
     * @param sp  Source port (0: dont care)
     * @param dp  Destination port (0: dont care)
     * @param prot Protocol (0: dont care, 1: UDP, 2: TCP)
     * 
     */
    void setup_flow_filter(int i, uint32_t queue, uint32_t sip, uint32_t dip, uint32_t sp, uint32_t dp, uint32_t prot) {
      assert(queue>=0);
      assert(queue<NUM_RX_QUEUES);
      assert(i>=0);
      assert(i<_max_flow_filter);
      disable_flow_filter(i);

      /* setup an example flow filter */
      uint32_t ftqf = 0;

      uint32_t SIPMASK,DIPMASK,SPMASK,DPMASK,PMASK;
      if (sip==0) SIPMASK=1; else SIPMASK=0;
      if (dip==0) DIPMASK=1; else DIPMASK=0;
      if (sp==0) SPMASK=1; else SPMASK=0;
      if (dp==0) DPMASK=1; else DPMASK=0;
      if (prot==0) PMASK=1; else PMASK=0;

      uint32_t POOLMASK=1;
      uint32_t EN=1;      //Enable this filter

      if (prot==1) ftqf |= 0x1d;         //UDP filter with the highest priority
        else if (prot==2) ftqf |= 0x1c;  //TCP filter with the highest priority
          else ftqf |= 0x1f;             //Protocol is masked

      ftqf |= (SIPMASK<<25);
      ftqf |= (DIPMASK<<26);
      ftqf |= (SPMASK<<27);
      ftqf |= (DPMASK<<28);
      ftqf |= (PMASK<<29);
      ftqf |= (POOLMASK<<30);
      ftqf |= (EN<<31);

      uint32_t l34timir = 0;
      l34timir |= (queue<<21); // packets matching this filter goes to RX queue 
      l34timir |= (1<<12); // size bypass

      uint32_t spdp = 0;
      spdp |= dp<<16;
      spdp |= sp;
      _card->_mmio->mmio_write32(IXGBE_FTQF(i),ftqf);
      _card->_mmio->mmio_write32(IXGBE_SAQF(i),sip);
      _card->_mmio->mmio_write32(IXGBE_DAQF(i),dip);
      _card->_mmio->mmio_write32(IXGBE_SDPQF(i),spdp);
      _card->_mmio->mmio_write32(IXGBE_L34T_IMIR(i),l34timir);

    }

    void init_fdir_perfect(uint32_t fdirctrl, int flex_byte_offset, bool kvcache_server) {
      /*
       *  Turn perfect match filtering on
       *  Report hash in RSS field of Rx wb descriptor
       *  Set the maximum length per hash bucket to 10 filters
       *  Send interrupt when 64 (0x4 * 16) filters are left
       */
      fdirctrl |= IXGBE_FDIRCTRL_PERFECT_MATCH |
                  IXGBE_FDIRCTRL_REPORT_STATUS |
                  (flex_byte_offset << IXGBE_FDIRCTRL_FLEX_SHIFT) |
                  (FDIR_MAX_FILTER_LENGTH << IXGBE_FDIRCTRL_MAX_LENGTH_SHIFT) |
                  (FDIR_MIN_FILTER_THRESH << IXGBE_FDIRCTRL_FULL_THRESH_SHIFT);

      /* write hashes and fdirctrl register, poll for completion */

      int i;
      /* Prime the keys for hashing */
      _card->_mmio->mmio_write32(IXGBE_FDIRHKEY, 0x0);  // we only use the first bucket
      _card->_mmio->mmio_write32(IXGBE_FDIRSKEY, 0x0);

      _card->_mmio->mmio_write32(IXGBE_FDIRCTRL, fdirctrl);

      /* Mask all unused fields */
      _card->_mmio->mmio_write32(IXGBE_FDIRDIP4M,0xffffffff);
      _card->_mmio->mmio_write32(IXGBE_FDIRSIP4M,0xffffffff);
      _card->_mmio->mmio_write32(IXGBE_FDIRTCPM,0xffffffff);
      _card->_mmio->mmio_write32(IXGBE_FDIRIP6M,0xffffffff);
      if (kvcache_server) {
        _card->_mmio->mmio_write32(IXGBE_FDIRUDPM,0xffffffff);
        _card->_mmio->mmio_write32(IXGBE_FDIRM,0x2f); //Unmask FLEX bit to enable 2-byte filter
      }
      else {
        //Unmask the lower part of UDP destination port
        _card->_mmio->mmio_write32(IXGBE_FDIRUDPM,0x00ffffff); 
        _card->_mmio->mmio_write32(IXGBE_FDIRM,0x37); //Unmask L4P bit and mask FLEX bit
      }

      _card->_mmio->mmio_read32(IXGBE_STATUS);  // IXGBE_WRITE_FLUSH();

      for (i = 0; i < IXGBE_FDIR_INIT_DONE_POLL; i++) {
        if (_card->_mmio->mmio_read32(IXGBE_FDIRCTRL) & IXGBE_FDIRCTRL_INIT_DONE)
          break;
        usleep(1000);
      }
      if (i >= IXGBE_FDIR_INIT_DONE_POLL) {
        panic("Flow Director poll time exceeded!\n");
      }
    }

    void setup_fdir_filter(uint16_t key, int queue, bool kvcache_server) {
      /* setup the filters on bucket 0 */
      uint32_t fdirhash = 0;
      fdirhash |= (1 << IXGBE_FDIRHASH_BUCKET_VALID_SHIFT);
      uint32_t software_index = counter;
      fdirhash |= (software_index << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT);
      _card->_mmio->mmio_write32(IXGBE_FDIRHASH,fdirhash);

      _card->_mmio->mmio_write32(IXGBE_FDIRSIPv6(0),0);
      _card->_mmio->mmio_write32(IXGBE_FDIRSIPv6(1),0);
      _card->_mmio->mmio_write32(IXGBE_FDIRSIPv6(2),0);
      _card->_mmio->mmio_write32(IXGBE_FDIRIPSA,0);
      _card->_mmio->mmio_write32(IXGBE_FDIRIPDA,0);

      if (kvcache_server) {
        uint32_t fdirvlan = 0;
        fdirvlan |= (key << IXGBE_FDIRVLAN_FLEX_SHIFT);  
        _card->_mmio->mmio_write32(IXGBE_FDIRVLAN, fdirvlan);
        _card->_mmio->mmio_write32(IXGBE_FDIRPORT, 0);
      }
      else {
        _card->_mmio->mmio_write32(IXGBE_FDIRVLAN,0);
        uint32_t fdirport = 0;
        fdirport |= (key << IXGBE_FDIRPORT_DESTINATION_SHIFT);  
        _card->_mmio->mmio_write32(IXGBE_FDIRPORT, fdirport);
      }
        
      uint32_t fdircmd = 0;
      fdircmd |= IXGBE_FDIRCMD_CMD_ADD_FLOW;
      fdircmd |= IXGBE_FDIRCMD_FILTER_UPDATE;
      fdircmd |= IXGBE_FDIRCMD_L4TYPE_UDP;
      fdircmd |= IXGBE_FDIRCMD_LAST;
      fdircmd |= IXGBE_FDIRCMD_QUEUE_EN;

      fdircmd |= (queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT);
      _card->_mmio->mmio_write32(IXGBE_FDIRCMD,fdircmd);
      counter++;
    }
  };

}

#endif
