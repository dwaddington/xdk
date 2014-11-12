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






#ifndef __NVME_REGISTERS_H__
#define __NVME_REGISTERS_H__

#include <stdint.h>

#define NVME_REG_CAP 0x0
#define NVME_REG_VS  0x8
#define NVME_REG_INTMS 0xC
#define NVME_REG_INTMC 0x10
#define NVME_REG_CC 0x14
#define NVME_REG_CSTS 0x1C

struct NVME_register_map {
  volatile uint64_t cap; /* Controller Capabilities */
  volatile uint32_t vs; /* Version */
  volatile uint32_t intms; /* Interrupt Mask Set */
  volatile uint32_t intmc; /* Interrupt Mask Clear */
  volatile uint32_t cc; /* Controller Configuration */
  volatile uint32_t reserved1; /* Reserved */
  volatile uint32_t csts; /* Controller Status */
  volatile uint32_t reserved2; /* Reserved */
  volatile uint32_t aqa_attr; /* Admin Queue Attributes */
  volatile uint64_t asq_base; /* Admin SQ Base Address */
  volatile uint64_t acq_base;	/* Admin CQ Base Address */  
} __attribute__((packed));

#define NVME_CAP_CQR(cap)	    ((cap) & 0x10000)
#define NVME_CAP_MQES(cap)	  ((cap) & 0xffff)
#define NVME_CAP_TIMEOUT(cap)	(((cap) >> 24) & 0xff)
#define NVME_CAP_STRIDE(cap)	(((cap) >> 32) & 0xf)
#define NVME_CAP_MPSMIN(cap)	(((cap) >> 48) & 0xf)
#define NVME_CAP_MPSMAX(cap)	(((cap) >> 51) & 0xf)
#define NVME_CAP_CSS(cap)	    (((cap) >> 36) & 0x1)
#define NVME_CAP_AMS(cap)	    (unsigned)(((cap) >> 16) & 0x3)

#define NVME_CSTS_RDY(csts)  (csts & 0x1)
#define NVME_CSTS_CFS(csts)  ((csts >> 1) & 0x1)
#define NVME_CSTS_SHST(csts) ((csts >> 2) & 0x3)

#define NVME_OFFSET_DOORBELL_SUBMISSION_TAIL(cap,q) (0x1000 + ((2 * q) * (4 << NVME_CAP_STRIDE(cap))))
#define NVME_OFFSET_DOORBELL_COMPLETION_HEAD(cap,q) (0x1000 + (((2 * q) + 1) * (4 << NVME_CAP_STRIDE(cap))))

#define NVME_OFFSET_SUBMISSION_DB(cap,q,stride) (0x1000 + ((2 * q) * (4 << stride)))
#define NVME_OFFSET_COMPLETION_DB(cap,q,stride) (0x1000 + (((2 * q) + 1) * (4 << stride)))



enum {
	NVME_CC_ENABLE		= 1 << 0,
	NVME_CC_CSS_NVM		= 0 << 4,
	NVME_CC_MPS_SHIFT	= 7,
	NVME_CC_ARB_RR		= 0 << 11,
	NVME_CC_ARB_WRRU	= 1 << 11,
	NVME_CC_ARB_VS		= 7 << 11,
	NVME_CC_SHN_NONE	= 0 << 14,
	NVME_CC_SHN_NORMAL	= 1 << 14,
	NVME_CC_SHN_ABRUPT	= 2 << 14,
	NVME_CC_IOSQES		= 6 << 16,
	NVME_CC_IOCQES		= 4 << 20,
	NVME_CSTS_RDY		= 1 << 0,
	NVME_CSTS_CFS		= 1 << 1,
	NVME_CSTS_SHST_NORMAL	= 0 << 2,
	NVME_CSTS_SHST_OCCUR	= 1 << 2,
	NVME_CSTS_SHST_CMPLT	= 2 << 2,
};


#endif

