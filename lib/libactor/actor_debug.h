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
  Copyright (C) 2015, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __ACTOR_DEBUG_H__
#define __ACTOR_DEBUG_H__

/* enable/disable debugging for actor framework */
//#define DEBUG_ACTORS

#define ERROR(f, a...)   fprintf( stderr, "%s[ACTOR]: ERROR %s:" f "\n%s",  BRIGHT_RED, __func__ , ## a, ESC_END);

#ifdef DEBUG_ACTORS

#include <stdio.h>
#include <assert.h>

#include <common/logging.h>
#include <common/assert.h>

#define LOG(f, a...)   fprintf( stderr, "%s[ACTOR]:" f "%s\n", ESC_LOG, ## a, ESC_END)
//#define ASSERT(cond, f, a...) if(! cond ) { fprintf( stderr, "%s[ACTOR]: ASSERT FAIL %s:" f "\n%s",  ESC_ERR,__func__ , ## a, ESC_END); assert(cond); }
#else

#include <stdio.h>
#include <assert.h>

#include <common/logging.h>
#include <common/assert.h>


#define LOG(f, a...)

//#define ASSERT(cond, f, a...)

#endif // DEBUG_ACTORS

#endif // __ACTOR_DEBUG_H__
