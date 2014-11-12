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

#ifndef __PCI_H__
#define __PCI_H__

#define  PCI_VENDOR_ID           0x00    /* 16 bits */
#define  PCI_DEVICE_ID           0x02    /* 16 bits */
#define  PCI_COMMAND             0x04    /* 16 bits */

#define  PCI_COMMAND_IO          0x1     /* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY      0x2     /* Enable response in Memory space */
#define  PCI_COMMAND_MASTER      0x4     /* Enable bus mastering */
#define  PCI_COMMAND_SPECIAL     0x8     /* Enable response to special cycles */
#define  PCI_COMMAND_INVALIDATE  0x10    /* Use memory write and invalidate */
#define  PCI_COMMAND_VGA_PALETTE 0x20    /* Enable palette snooping */
#define  PCI_COMMAND_PARITY      0x40    /* Enable parity checking */
#define  PCI_COMMAND_WAIT        0x80    /* Enable address/data stepping */
#define  PCI_COMMAND_SERR        0x100   /* Enable SERR */
#define  PCI_COMMAND_FAST_BACK   0x200   /* Enable back-to-back writes */

#define  PCI_STATUS              0x06    /* 16 bits */
#define  PCI_REVISION            0x08    /* 8  bits */
#define  PCI_CLASSCODE_0         0x09    /* 8  bits */
#define  PCI_CLASSCODE_1         0x0a    /* 8  bits */
#define  PCI_CLASSCODE_2         0x0b    /* 8  bits */
#define  PCI_CACHE_LINE_SIZE     0x0c    /* 8  bits */
#define  PCI_LATENCY_TIMER       0x0d    /* 8  bits */
#define  PCI_HEADER_TYPE         0x0e    /* 8  bits */
#define  PCI_BIST                0x0f    /* 8  bits */
#define  PCI_BAR_0               0x10    /* 32 bits */
#define  PCI_BAR_1               0x14    /* 32 bits */
#define  PCI_BAR_2               0x18    /* 32 bits */
#define  PCI_BAR_3               0x1c    /* 32 bits */
#define  PCI_BAR_4               0x20    /* 32 bits */
#define  PCI_BAR_5               0x24    /* 32 bits */
#define  PCI_CARDBUS_CIS_PTR     0x28    /* 32 bits */
#define  PCI_SUBSYSTEM_VENDOR_ID 0x2c    /* 16 bits */
#define  PCI_SUBSYSTEM_ID        0x2e    /* 16 bits */
#define  PCI_EXPANSION_ROM       0x30    /* 32 bits */
#define  PCI_CAP_PTR             0x34    /* 8  bits */
#define  PCI_INTERRUPT_LINE      0x3c    /* 8 bits  */ 
#define  PCI_INTERRUPT_PIN       0x3d    /* 8 bits  */ 
#define  PCI_MIN_GNT             0x3e    /* 8 bits  */ 
#define  PCI_MAX_LAT             0x3f    /* 8 bits  */ 

#define PCI_PMCAP     0x40
#define PCI_MSICAP    (PCI_PMCAP+0x8)
#define PCI_MSIXCAP   (PCI_MSICAP+0xA)
#define PCI_PXCAP     (PCI_MSIXCAP+0xC)
#define PCI_AERCAP    (PCI_PXCAP+0x2A)


namespace PCI {
  typedef enum {
    Device_type_unknown=0xff,
    Device_type_normal=0,
    Device_type_bridge=1,
    Device_type_cardbus=2,
  } Device_type;
    
  /** 
   * PCI configuration space - type 0
   * 
   */
  struct Config_space_type0    
  {
    uint16_t vendor;
    uint16_t device;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t classcode[3];
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar[6]; 
    uint32_t cardbus_cis;
    uint16_t subsys_vendor;
    uint16_t subsys;
    uint32_t expansion_rom_addr;
    uint8_t cap_pointer;
    uint8_t reserved[7];
    uint8_t irq_line;
    uint8_t irq_pin;
    uint8_t min_gnt;
    uint8_t max_lat;
  } 
    __attribute__((packed,aligned(4)));

  struct Config_space_type0_memory_bar
  {
    unsigned region_type:1;
    unsigned locatable:2;
    unsigned prefetchable:1;
  }
    __attribute__((packed,aligned(4)));

  /** 
   * PCI configuration space - type 1
   * 
   */
  struct Config_space_type1
  {
    uint16_t vendor;
    uint16_t device;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t progif;
    uint8_t subclass;
    uint8_t classcode;
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar[2]; 
    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    uint8_t secondary_latency_timer;
    uint8_t io_base;
    uint8_t io_limit;
    uint16_t secondary_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint32_t prefetch_base_upper;
    uint32_t prefetch_limit_upper;
    uint16_t io_base_upper;
    uint16_t io_limit_upper;
    uint8_t capability_pointer;
    uint8_t reserved[3];
    uint32_t expansion_rom_base_addr;
    uint8_t irq_line;
    uint8_t irq_pin;
    uint16_t bridge_control;
  } 
    __attribute__((packed,aligned(4)));


  struct Config_space_type1_bist
  {
    unsigned completion_code : 4;
    unsigned reserved        : 2;
    unsigned start_bist      : 1;
    unsigned bist_capable    : 7;
  };

  struct Config_space_type1_header_type
  {
    unsigned type : 6;
    unsigned mf   : 1;
  };


  /** 
   * PCI configuration space - type 2
   * 
   */
  struct Config_space_type2
  {
    uint16_t vendor;
    uint16_t device;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t progif;
    uint8_t subclass;
    uint8_t classcode;
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t socket_base_addr;
    uint8_t offset_cap_list;
    uint8_t reserved;
    uint8_t subordinate_bus;
    uint8_t cardbus_latency_timer;
    uint32_t mem_base_0;
    uint32_t mem_limit_0;
    uint32_t mem_base_1;
    uint32_t mem_limit_1;
    uint32_t io_base_0;
    uint32_t io_limit_0;
    uint32_t io_base_1;
    uint32_t io_limit_1;
    uint8_t irq_line;
    uint8_t irq_pin;
    uint16_t bridge_control;
    uint16_t subsystem_device_id;
    uint16_t subsystem_vendor_id;
    uint32_t pccard_legacy_mode_base_addr;
  } 
  __attribute__((packed,aligned(4)));

  struct Config_space_type2_cmd
  {
    unsigned io_space : 1;
    unsigned mem_space : 1;
    unsigned bus_master : 1;
    unsigned special_cycles : 1;
    unsigned memory_wi_enable : 1;
    unsigned vga_palette_snoop : 1;
    unsigned parity_error_response : 1;
    unsigned reserved : 1;
    unsigned serr_enable : 1;
    unsigned fast_backtoback_enable : 1;
    unsigned irq_disable : 1;
    unsigned _reserved : 5;
  } 
  __attribute__((packed,aligned(4)));

  struct Config_space_type2_status
  {
    unsigned reserved : 3;
    unsigned interrupt_status : 1;
    unsigned cap_list : 1;
    unsigned cap_66mhz : 1;
    unsigned reserved2 : 1;
    unsigned fast_backtoback_capable : 1;
    unsigned master_data_parity_error : 1;
    unsigned devsel_timing : 2;
    unsigned signaled_target_abort : 1;
    unsigned received_target_abort : 1;
    unsigned received_master_abort : 1;
    unsigned signaled_system_error : 1;
    unsigned detected_parity_error : 1;
  } 
  __attribute__((packed,aligned(4)));

}
  
#endif // __PCI_H__
