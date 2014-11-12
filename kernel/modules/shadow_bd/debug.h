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






#define PDBG(f, a...)	printk( KERN_DEBUG "[SHADOW BD]: %s:" f "\n",  __func__ , ## a);
#define PINF(f, a...)	printk( KERN_INFO "[SHADOW BD]: %s:" f "\n",  __func__ , ## a);
#define PWRN(f, a...)	printk( KERN_INFO "[SHADOW BD]: %s:" f "\n",  __func__ , ## a);
#define PLOG(f, a...)	printk("[SHADOW BD]: " f "\n",  ## a);
#define PERR(f, a...)	printk( KERN_ERR "[SHADOW BD]: ERROR %s:" f "\n",  __func__ , ## a);

#define ASSERT(x) if(!(x)) { printk("ASSERT: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); panic("Assertion failed!"); }
