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
          @author Chen (chen.tian@samsung.com)
          @date: Aug 3, 2012
*/


#ifndef __E1000_CARD_H__
#define __E1000_CARD_H__

#include <printf.h>
#include <sleep.h>
#include <semaphore.h>
#include "env.h"
#include "e1000.h"
#include "pci.h"
#include <channel.h>

using namespace OmniOS;

#define TCTL_EN				(1 << 1)
#define TCTL_PSP			(1 << 3)
//#define CTRL_SLU			(1 << 6)


#define RCTL_EN				(1 << 1)
#define RCTL_SBP			(1 << 2)
#define RCTL_UPE			(1 << 3)
#define RCTL_MPE			(1 << 4)
#define RCTL_LPE			(1 << 5)
#define RDMTS_HALF			(0 << 8)
#define RDMTS_QUARTER		(1 << 8)
#define RDMTS_EIGHTH		(2 << 8)
#define RCTL_BAM			(1 << 15)
#define RCTL_BSIZE_256		(3 << 16)
#define RCTL_BSIZE_512		(2 << 16)
#define RCTL_BSIZE_1024		(1 << 16)
#define RCTL_BSIZE_2048		(0 << 16)
#define RCTL_BSIZE_4096		((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192		((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384	((1 << 16) | (1 << 25))
#define RCTL_SECRC			(1 << 26)


typedef char eth_pkt_t[1514];
typedef Channel_producer<eth_pkt_t, 20480> channel_t;

namespace E1000 {
  typedef struct PCI::Config_space_type0 PCI_config;
  class Address {
      volatile uint32_t _v;
      volatile uint32_t _p;
    public:
      Address(uint32_t v, uint32_t p):_v(v), _p(p){}
      Address() {}
      uint32_t v() {return _v;}
      uint32_t p() {return _p;}
      void v(uint32_t val) {_v = val;}
      void p(uint32_t val) {_p = val;}
  };

  class E1000_card {
    // RX and TX descriptor structures
      typedef struct _e1000_rx_desc {
          //volatile uint32_t	addressHigh;
          volatile uint64_t	address;
          volatile uint16_t	length;
          volatile uint16_t	checksum;
          volatile uint8_t	status;
          volatile uint8_t	errors;
          volatile uint16_t	special;
      } __attribute__((packed)) RX_DESC;

      typedef struct  _e1000_tx_desc {
          volatile uint64_t	address;
          volatile uint16_t	length;
          volatile uint8_t	cso;
          volatile uint8_t	cmd;
          volatile uint8_t	sta;
          volatile uint8_t	css;
          volatile uint16_t	special;
      }__attribute__((packed)) TX_DESC;

    typedef enum {
      STATUS_FD = (1 << 0),  /**< Link Full Duplex configuration Indication */
      STATUS_LU = (1 << 1),  /**< Link Up Indication */
      
      /** Link speed setting shift */
      STATUS_SPEED_SHIFT = 6,
      /** Link speed setting size */
      STATUS_SPEED_SIZE = 2,
      /** Link speed setting all bits set */
      STATUS_SPEED_ALL = ((1 << STATUS_SPEED_SIZE) - 1),
      /** Link speed setting 10 Mb/s value */
      STATUS_SPEED_10 = 0,
      /** Link speed setting 100 Mb/s value */
      STATUS_SPEED_100 = 1,
      /** Link speed setting 1000 Mb/s value variant A */
      STATUS_SPEED_1000A = 2,
      /** Link speed setting 1000 Mb/s value variant B */
      STATUS_SPEED_1000B = 3,
  } e1000_status_t;
  
  typedef enum {
    CTRL_FD = (1 << 0),    /**< Full-Duplex */
    CTRL_LRST = (1 << 3),  /**< Link Reset */
    CTRL_ASDE = (1 << 5),  /**< Auto-Speed Detection Enable */
    CTRL_SLU = (1 << 6),   /**< Set Link Up */
    CTRL_ILOS = (1 << 7),  /**< Invert Loss-of-Signal */
    
    /** Speed selection shift */
    CTRL_SPEED_SHIFT = 8,
    /** Speed selection size */
    CTRL_SPEED_SIZE = 2,
    /** Speed selection all bit set value */
    CTRL_SPEED_ALL = ((1 << CTRL_SPEED_SIZE) - 1),
    /** Speed selection shift */
    CTRL_SPEED_MASK = CTRL_SPEED_ALL << CTRL_SPEED_SHIFT,
    /** Speed selection 10 Mb/s value */
    CTRL_SPEED_10 = 0,
    /** Speed selection 10 Mb/s value */
    CTRL_SPEED_100 = 1,
    /** Speed selection 10 Mb/s value */
    CTRL_SPEED_1000 = 2,
    
    CTRL_FRCSPD = (1 << 11),   /**< Force Speed */
    CTRL_FRCDPLX = (1 << 12),  /**< Force Duplex */
    CTRL_RST = (1 << 26),      /**< Device Reset */
    CTRL_VME = (1 << 30),      /**< VLAN Mode Enable */
    CTRL_PHY_RST = (1 << 31),   /**< PHY Reset */
  } e1000_ctrl_t;


  public:
      enum {
          NUM_TX_BUF_PAGES   = 4, 
          NUM_TX_DESCRIPTORS = NUM_TX_BUF_PAGES*L4_PAGESIZE/sizeof(TX_DESC), //1024
          NUM_RX_BUF_PAGES   = 4, 
          NUM_RX_DESCRIPTORS = NUM_RX_BUF_PAGES*L4_PAGESIZE/sizeof(RX_DESC), //1024
          PKT_MAX_SIZE       = 2048
      };

      bool is_x8254;
      bool is_x8257;

      uint32_t pciID;
      uint16_t vendor;
      uint16_t device;
      uint32_t bar_mmio;
      uint32_t bar_port;
      bool full_duplex;
      uint32_t speed;

      Address mmio;
      uint32_t mmio_size;
      uint8_t mac[6];
      uint32_t ip[4];
      bool use_msi;
      bool ready;
      bool is_up;
      
	    volatile uint8_t		*tx_buf;
      volatile TX_DESC    *tx_desc[NUM_RX_DESCRIPTORS];
	    volatile uint16_t	  tx_buf_pos;
	    Address             rx_buf;
	    Address             rx_pkt_buf;
      volatile RX_DESC    *rx_desc[NUM_RX_DESCRIPTORS];
	    volatile uint16_t	  rx_buf_pos;
      
      
      uint16_t irq_num;
      uint8_t  irq_pin;

      uint64_t snd_counter;
      uint64_t rcv_counter;
      channel_t* channel;

    public:
      E1000_card();
      bool init();
      bool mac_init();
      bool irq_init(bool use_mptable);
      void tx_init();
      void rx_init();
      void reset_rx_desc(int index);
      void print_rx_desc(int index);

      void send(uint32_t pkt_phys, uint32_t len);
      void interrupt_handler();
      void activate();
      void wait_for_activate();
      bool config_msi(int msg);
      void poll_interrupt();
      void dump();
      bool pci_search_e1000();
      uint16_t read_eeprom(uint8_t reg);
      inline uint32_t mmio_read32(uint32_t reg);
      inline void mmio_write32(uint32_t reg, uint32_t val); 
      void register_channel(channel_t* c) {channel = c;}
      bool sent_to_me(uint8_t *pkt);
      bool is_arp(uint8_t *pkt);
  };
}

#endif 
