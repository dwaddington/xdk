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






#ifndef PK_SHADOW_BD_H
#define PK_SHADOW_BD_H

#define SBD_SECTOR_SIZE 512

extern int sbd_init(void);
extern void sbd_cleanup(void);
extern void sbd_write(sector_t sector_off, u8 *buffer, unsigned int sectors);
extern void sbd_read(sector_t sector_off, u8 *buffer, unsigned int sectors);
#endif
