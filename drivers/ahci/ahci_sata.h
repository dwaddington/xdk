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
  @author Daniel Waddington (d.waddington@samsung.com)
  @date: June 26, 2012
*/

#ifndef __AHCI_SATA_H__
#define __AHCI_SATA_H__

#include <sys/types.h>

/*----------------------------------------------------------------------------*/
/*-- SATA Buffer Lengths -----------------------------------------------------*/
/*----------------------------------------------------------------------------*/

/** Default sector size in bytes. */
#define SATA_DEFAULT_BLOCK_SIZE  512

/** Size for set feature command buffer in bytes. */
#define SATA_SET_FEATURE_BUFFER_LENGTH  512

/** Size for indentify (packet) device buffer in bytes. */
#define SATA_IDENTIFY_DEVICE_BUFFER_LENGTH  512

/*----------------------------------------------------------------------------*/
/*-- SATA Fis Frames ---------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

/** Sata FIS Type number. */
#define SATA_CMD_FIS_TYPE  0x27 // Register FIS H2D

/** Sata FIS Type command indicator. */
#define SATA_CMD_FIS_COMMAND_INDICATOR  0x80

/** Standard Command frame */
typedef struct {
  union {
    struct {
      /** FIS type - always SATA_CMD_FIS_TYPE. */
      unsigned fis_type : 8;
      /** Indicate that FIS is a Command - always SATA_CMD_FIS_COMMAND_INDICATOR. */
      unsigned c : 8;
      /** Command ID */
      unsigned command : 8;
      /** Features - subcommand for set features - set tranfer mode - 0x03. */
      unsigned features : 8;
      /** 0:23 bits of LBA. */
      unsigned lba_lower : 24;
      /** Device. */
      unsigned device : 8;
      /** 24:47 bits of LBA. */
      unsigned lba_upper : 24;
      /** Features - subcommand for set features - set tranfer mode - 0x03. */
      unsigned features_upper : 8;
      /** Sector count - transfer mode for set transfer mode operation. */
      unsigned count : 16;
      /** Isochronous Command Completion (SATA-III) */
      unsigned icc : 8;
      /** Control. */
      unsigned control : 8;
      /** Auxilary. */
      unsigned auxilary : 16;
      /** Reserved. */
      unsigned reserved2 : 16;
    } __attribute__((packed));

    struct {
      byte cfis[0x40];
      byte acmd[0x10];
      byte reserved[0x30];
      byte prdt[0];
    };
  };
} sata_std_command_frame_t;

/** Command frame for NCQ data operation. */
typedef struct {
  union {
    struct {
      /** FIS type - always 0x27. (fis type)  */
      uint8_t fis_type; 
      /** Indicate that FIS is a Command - always SATA_CMD_FIS_COMMAND_INDICATOR. (CRR-Reserved) */
      uint8_t c;
      /** Command - FPDMA Read - 0x60, FPDMA Write - 0x61. (Command) */
      uint8_t command; 
      /** bits 7:0 of sector count. (Features) */
      uint8_t sector_count_low; 
      /** bits 7:0 of lba. (Sector Number) */
      uint8_t lba0; 
      /** bits 15:8 of lba. (Cyl Low) */
      uint8_t lba1; 
      /** bits 23:16 of lba. (Cyl High) */
      uint8_t lba2;
      /* (Dev/Head) */
      uint8_t fua; 
      /** bits 31:24 of lba. (Sec Num Exp) */
      uint8_t lba3; 
      /** bits 39:32 of lba. (Cyl Low Exp) */
      uint8_t lba4; 
      /** bits 47:40 of lba. (Cyl High Exp) */
      uint8_t lba5; 
      /** bits 15:8 of sector count. (Features Exp) */
      uint8_t sector_count_high; 
      /** Tag number of NCQ operation. (Sec Count) - note this is an index into the scatter/gather list */
      uint8_t tag;
      /** PRIO (bits 6 and 7)(Sec Count Exp) */
      unsigned reserved1 : 6;
      unsigned prio : 2;
      /** ICC */
      uint8_t icc;
      /** Control. (Control) */
      uint8_t control;
      /** Reserved.  */
      uint8_t reserved3;
      /** Reserved. */
      uint8_t reserved4;
      /** Reserved. */
      uint8_t reserved5;
      /** Reserved. */
      uint8_t reserved6;
    } __attribute__((packed));
    struct {
      byte cfis[0x40];
      byte acmd[0x10];
      byte reserved[0x30];
      byte prdt[0];
    } __attribute__((packed));
  };
} sata_ncq_command_frame_t;

/*----------------------------------------------------------------------------*/
/*-- SATA Identify device ----------------------------------------------------*/
/*----------------------------------------------------------------------------*/

/** Data returned from identify device and identify packet device command. */
typedef struct {
	uint16_t gen_conf;
	uint16_t cylinders;
	uint16_t reserved2;
	uint16_t heads;
	uint16_t _vs4;
	uint16_t _vs5;
	uint16_t sectors;
	uint16_t _vs7;
	uint16_t _vs8;
	uint16_t _vs9;
	
	uint16_t serial_number[10];
	uint16_t _vs20;
	uint16_t _vs21;
	uint16_t vs_bytes;
	uint16_t firmware_rev[4];
	uint16_t model_name[20];
	
	uint16_t max_rw_multiple;
	uint16_t reserved48;
	/* Different meaning for packet device. */
	uint16_t caps;
	uint16_t reserved50;
	uint16_t pio_timing;
	uint16_t dma_timing;
	
	uint16_t validity;
	uint16_t cur_cyl;
	uint16_t cur_heads;
	uint16_t cur_sectors;
	uint16_t cur_capacity0;
	uint16_t cur_capacity1;
	uint16_t mss;
	uint16_t total_lba28_0;
	uint16_t total_lba28_1;
	uint16_t sw_dma;
	uint16_t mw_dma;
	uint16_t pio_modes;
	uint16_t min_mw_dma_cycle;
	uint16_t rec_mw_dma_cycle;
	uint16_t min_raw_pio_cycle;
	uint16_t min_iordy_pio_cycle;
	
	uint16_t reserved69;
	uint16_t reserved70;
	uint16_t reserved71;
	uint16_t reserved72;
	uint16_t reserved73;
	uint16_t reserved74;
	
	uint16_t queue_depth;
	/** SATA capatibilities - different meaning for packet device. */
	uint16_t sata_cap;
	/** SATA additional capatibilities - different meaning for packet device. */
	uint16_t sata_cap2;
	uint16_t serial_ata_features[1 + 79 - 78];
	uint16_t version_maj;
	uint16_t version_min;
	uint16_t cmd_set0;
	uint16_t cmd_set1;
	uint16_t csf_sup_ext;
	uint16_t csf_enabled0;
	uint16_t csf_enabled1;
	uint16_t csf_default;
	uint16_t udma;
	
	uint16_t reserved89[1 + 99 - 89];
	
	/* Total number of blocks in LBA-48 addressing. */
	uint16_t total_lba48_0;
	uint16_t total_lba48_1;
	uint16_t total_lba48_2;
	uint16_t total_lba48_3;
	
	uint16_t reserved104[1 + 105 - 104];
	uint16_t physical_logic_sector_size;
	/* Note: more fields are defined in ATA/ATAPI-7. */
	uint16_t reserved107[1 + 127 - 107];
	uint16_t reserved128[1 + 159 - 128];
	uint16_t reserved160[1 + 255 - 160];
} sata_identify_data_t;

/** Capability bits for register device. */
enum sata_rd_caps {
	sata_rd_cap_iordy = 0x0800,
	sata_rd_cap_iordy_cbd = 0x0400,
	sata_rd_cap_lba = 0x0200,
	sata_rd_cap_dma = 0x0100
};

/** Bits of @c identify_data_t.cmd_set1. */
enum sata_cs1 {
	/** 48-bit address feature set. */
	sata_cs1_addr48 = 0x0400
};

/** SATA capatibilities for not packet device - Serial ATA revision 3_1. */
enum sata_np_caps {
	/** Supports READ LOG DMA EXT. */
	sata_np_cap_log_ext = 0x8000,
	/** Supports Device Automatic Partial to Slumber transitions. */
	sata_np_cap_dev_slm = 0x4000,
	/** Supports Host Automatic Partial to Slumber transitions. */
	sata_np_cap_host_slm = 0x2000,
	/** Supports NCQ priority information. */
	sata_np_cap_ncq_prio = 0x1000,
	/** Supports Unload while NCQ command outstanding. */
	sata_np_cap_unload_ncq = 0x0800,
	/** Supports Phy event counters. */
	sata_np_cap_phy_ctx = 0x0400,
	/** Supports recepits of host-initiated interface power management. */
	sata_np_cap_host_pmngmnt = 0x0200,
	
	/** Supports NCQ. */
	sata_np_cap_ncq = 0x0100,
	
	/** Supports SATA 3. */
	sata_np_cap_sata_3 = 0x0008,
	/** Supports SATA 2. */
	sata_np_cap_sata_2 = 0x0004,
	/** Supports SATA 1. */
	sata_np_cap_sata_1 = 0x0002
};

/** SATA capatibilities for packet device - Serial ATA revision 3_1. */
enum sata_pt_caps {
	/** Supports READ LOG DMA EXT. */
	sata_pt_cap_log_ext = 0x8000,
	/** Supports Device Automatic Partial to Slumber transitions. */
	sata_pt_cap_dev_slm = 0x4000,
	/** Supports Host Automatic Partial to Slumber transitions. */
	sata_pt_cap_host_slm = 0x2000,
	/** Supports Phy event counters. */
	sata_pt_cap_phy_ctx = 0x0400,
	/** Supports recepits of host-initiated interface power management. */
	sata_pt_cap_host_pmngmnt = 0x0200,
	
	/** Supports SATA 3. */
	sata_pt_cap_sat_3 = 0x0008,
	/** Supports SATA 2. */
	sata_pt_cap_sat_2 = 0x0004,
	/** Supports SATA 1. */
	sata_pt_cap_sat_1 = 0x0002
};

#endif
