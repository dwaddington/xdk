/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/






#include <linux/string.h>

#include "partition.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

#define SECTOR_SIZE 512
#define MBR_SIZE SECTOR_SIZE
#define MBR_DISK_SIGNATURE_OFFSET 440
#define MBR_DISK_SIGNATURE_SIZE 4
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_ENTRY_SIZE 16 // sizeof(PartEntry)
#define PARTITION_TABLE_SIZE 64 // sizeof(PartTable)
#define MBR_SIGNATURE_OFFSET 510
#define MBR_SIGNATURE_SIZE 2
#define MBR_SIGNATURE 0xAA55
#define BR_SIZE SECTOR_SIZE
#define BR_SIGNATURE_OFFSET 510
#define BR_SIGNATURE_SIZE 2
#define BR_SIGNATURE 0xAA55

typedef struct
{
	unsigned char boot_type; // 0x00 - Inactive; 0x80 - Active (Bootable)
	unsigned char start_head;
	unsigned char start_sec:6;
	unsigned char start_cyl_hi:2;
	unsigned char start_cyl;
	unsigned char part_type;
	unsigned char end_head;
	unsigned char end_sec:6;
	unsigned char end_cyl_hi:2;
	unsigned char end_cyl;
	unsigned long abs_start_sec;
	unsigned long sec_in_part;
} PartEntry;

typedef PartEntry PartTable[4];

static PartTable def_part_table =
{
	{
		boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x2,
		start_cyl: 0x00,
		part_type: 0x83,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x09,
		abs_start_sec: 0x00000001,
		sec_in_part: 0x0000013F
	},
	{
		boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x1,
		start_cyl: 0x0A, // extended partition start cylinder (BR location)
		part_type: 0x05,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x13,
		abs_start_sec: 0x00000140,
		sec_in_part: 0x00000140
	},
	{
		boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x1,
		start_cyl: 0x14,
		part_type: 0x83,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x1F,
		abs_start_sec: 0x00000280,
		sec_in_part: 0x00000180
	},
	{
	}
};
static unsigned int def_log_part_br_cyl[] = {0x0A, 0x0E, 0x12};
static const PartTable def_log_part_table[] =
{
	{
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x2,
			start_cyl: 0x0A,
			part_type: 0x83,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x0D,
			abs_start_sec: 0x00000001,
			sec_in_part: 0x0000007F
		},
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x1,
			start_cyl: 0x0E,
			part_type: 0x05,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x11,
			abs_start_sec: 0x00000080,
			sec_in_part: 0x00000080
		},
	},
	{
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x2,
			start_cyl: 0x0E,
			part_type: 0x83,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x11,
			abs_start_sec: 0x00000001,
			sec_in_part: 0x0000007F
		},
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x1,
			start_cyl: 0x12,
			part_type: 0x05,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x13,
			abs_start_sec: 0x00000100,
			sec_in_part: 0x00000040
		},
	},
	{
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x2,
			start_cyl: 0x12,
			part_type: 0x83,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x13,
			abs_start_sec: 0x00000001,
			sec_in_part: 0x0000003F
		},
	}
};

static void copy_mbr(u8 *disk)
{
	memset(disk, 0x0, MBR_SIZE);
	*(unsigned long *)(disk + MBR_DISK_SIGNATURE_OFFSET) = 0x36E5756D;
	memcpy(disk + PARTITION_TABLE_OFFSET, &def_part_table, PARTITION_TABLE_SIZE);
	*(unsigned short *)(disk + MBR_SIGNATURE_OFFSET) = MBR_SIGNATURE;
}
static void copy_br(u8 *disk, int start_cylinder, const PartTable *part_table)
{
	disk += (start_cylinder * 32 /* sectors / cyl */ * SECTOR_SIZE);
	memset(disk, 0x0, BR_SIZE);
	memcpy(disk + PARTITION_TABLE_OFFSET, part_table,
		PARTITION_TABLE_SIZE);
	*(unsigned short *)(disk + BR_SIGNATURE_OFFSET) = BR_SIGNATURE;
}
void copy_mbr_n_br(u8 *disk)
{
	int i;

	copy_mbr(disk);
	for (i = 0; i < ARRAY_SIZE(def_log_part_table); i++)
	{
		copy_br(disk, def_log_part_br_cyl[i], &def_log_part_table[i]);
	}
}
