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
  @date: Aug 2, 2012
*/

#ifndef __AHCI_FIS__
#define __AHCI_FIS__

typedef enum {
  FIS_TYPE_REG_H2D  = 0x27, // Register FIS - host to device
  FIS_TYPE_REG_D2H  = 0x34, // Register FIS - device to host
  FIS_TYPE_DMA_ACT  = 0x39, // DMA activate FIS - device to host
  FIS_TYPE_DMA_SETUP  = 0x41, // DMA setup FIS - bidirectional
  FIS_TYPE_DATA   = 0x46, // Data FIS - bidirectional
  FIS_TYPE_BIST   = 0x58, // BIST activate FIS - bidirectional
  FIS_TYPE_PIO_SETUP  = 0x5F, // PIO setup FIS - device to host
  FIS_TYPE_DEV_BITS = 0xA1, // Set device bits FIS - device to host
} FIS_TYPE;


typedef struct tagFIS_DMA_SETUP
{
  uint8_t fis_type;
  uint8_t resv:5;
  uint8_t d:1;
  uint8_t i:1;
  uint8_t r:1;
  uint8_t resv2[2];
  uint32_t dma_buff_l;
  uint32_t dma_buff_h;
  uint32_t resv3;
  uint32_t dma_buff_offset;
  uint32_t dma_transfer_count;
  uint32_t resv4;
} fis_dma_setup_t;

typedef struct tagFIS_DEV_BITS
{
  uint8_t fis_type; // FIS_TYPE_REG_H2D
  uint32_t unspecified;
} fis_dev_bits_t;

typedef struct tagFIS_REG_H2D
{
  // DWORD 0
  uint8_t fis_type; // FIS_TYPE_REG_H2D
 
  uint8_t pmport:5; // Port multiplier
  uint8_t rsv0:2;   // Reserved
  
  uint8_t c:1;    // 1: Command, 0: Control
 
  uint8_t command;  // Command register
  uint8_t featurel; // Feature register, 7:0
 
  // DWORD 1
  uint8_t lba0;   // LBA low register, 7:0
  uint8_t lba1;   // LBA mid register, 15:8
  uint8_t lba2;   // LBA high register, 23:16
  uint8_t device;   // Device register
 
  // DWORD 2
  uint8_t lba3;   // LBA register, 31:24
  uint8_t lba4;   // LBA register, 39:32
  uint8_t lba5;   // LBA register, 47:40
  uint8_t featureh; // Feature register, 15:8
 
  // DWORD 3
  uint8_t countl;   // Count register, 7:0
  uint8_t counth;   // Count register, 15:8
  uint8_t icc;    // Isochronous command completion
  uint8_t control;  // Control register
 
  // DWORD 4
  uint8_t rsv1[4];  // Reserved
} fis_reg_h2d_t;

typedef struct tagFIS_REG_D2H
{
  // DWORD 0
  uint8_t fis_type;    // FIS_TYPE_REG_D2H
 
  unsigned  pmport:4;    // Port multiplier
  unsigned  rsv0:2;      // Reserved
  unsigned  i:1;         // Interrupt bit
  unsigned  rsv1:1;      // Reserved
 
  uint8_t status;      // Status register
  uint8_t error;       // Error register
 
  // DWORD 1
  uint8_t lba0;        // LBA low register, 7:0
  uint8_t lba1;        // LBA mid register, 15:8
  uint8_t lba2;        // LBA high register, 23:16
  uint8_t device;      // Device register
 
  // DWORD 2
  uint8_t lba3;        // LBA register, 31:24
  uint8_t lba4;        // LBA register, 39:32
  uint8_t lba5;        // LBA register, 47:40
  uint8_t rsv2;        // Reserved
 
  // DWORD 3
  uint8_t countl;      // Count register, 7:0
  uint8_t counth;      // Count register, 15:8
  uint8_t rsv3[2];     // Reserved
 
  // DWORD 4
  uint8_t rsv4[4];     // Reserved
} fis_reg_d2h_t;

typedef struct tagFIS_DATA
{
  // DWORD 0
  uint8_t fis_type; // FIS_TYPE_DATA
 
  uint8_t pmport:4; // Port multiplier
  uint8_t rsv0:4;   // Reserved
 
  uint8_t rsv1[2];  // Reserved
 
  // DWORD 1 ~ N
  uint32_t data[1]; // Payload
} fis_data_t;

typedef struct tagFIS_PIO_SETUP
{
  // DWORD 0
  uint8_t fis_type; // FIS_TYPE_PIO_SETUP
 
  uint8_t pmport:4; // Port multiplier
  uint8_t rsv0:1;   // Reserved
  uint8_t d:1;    // Data transfer direction, 1 - device to host
  uint8_t i:1;    // Interrupt bit
  uint8_t rsv1:1;
 
  uint8_t status;   // Status register
  uint8_t error;    // Error register
 
  // DWORD 1
  uint8_t lba0;   // LBA low register, 7:0
  uint8_t lba1;   // LBA mid register, 15:8
  uint8_t lba2;   // LBA high register, 23:16
  uint8_t device;   // Device register
 
  // DWORD 2
  uint8_t lba3;   // LBA register, 31:24
  uint8_t lba4;   // LBA register, 39:32
  uint8_t lba5;   // LBA register, 47:40
  uint8_t rsv2;   // Reserved
 
  // DWORD 3
  uint8_t countl;   // Count register, 7:0
  uint8_t counth;   // Count register, 15:8
  uint8_t rsv3;   // Reserved
  uint8_t e_status; // New value of status register
 
  // DWORD 4
  uint16_t tc;    // Transfer count
  uint8_t rsv4[2];  // Reserved

} fis_pio_setup_t;


struct received_fis_t {
  union {
    uint8_t fis_type;
  };
  union {
    fis_dma_setup_t dfis;
    byte padding[0x20];
  };
  union {
    fis_pio_setup_t psfis;
    byte padding2[0x20];
  };
  union {
    fis_reg_d2h_t rfis;
    byte padding3[0x18];
  };
  union {
    fis_dev_bits_t sdbfis;
    byte padding4[0x2];
  };
  
} __attribute__((packed));

#endif
