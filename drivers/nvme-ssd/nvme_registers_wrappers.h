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






#ifndef __NVME_REG_WRAPPERS__
#define __NVME_REG_WRAPPERS__

class NVME_registers
{

public:
  volatile struct NVME_register_map * _registers;
  void *                     _base;
  Exokernel::Pci_region *    _region;

public:

  NVME_registers(Exokernel::Pci_region * region) : _region(region) {
    assert(region);
    _base = region->mmap_ptr();
    _registers = (struct NVME_register_map *) _base;
  }

  uint16_t version() const {
    assert(_registers);
    return  (_registers->vs >> 16);
  }

  template <typename T>
  T * offset(off_t off) {
    return ((T *) (((byte *)_base)+off));
  }

  Exokernel::Pci_region * region() {
    return _region;
  }

  /** 
   * Maximum Queue Entries Supported
   * 
   * 
   * @return Number of queues.
   */
  uint16_t mqes() const { 
    return NVME_CAP_MQES(_registers->cap);
  } /* 0's based value */

  /** 
   * Controller Configuration
   */
  uint32_t cc() const { return _registers->cc; }

  /** 
   * Note: if the enable bit is zeroed, then all the register are reset to default.
   * 
   */
  void cc(uint32_t r) { _registers->cc = r | 0x1; }

#if DEPRECATED
  /** 
   * I/O Completion Queue Size
   * 
   * 
   * @return The value is in bytes and is specified as a power of two (2^n).
   */
  unsigned cc_iocqes() const     {  return (_registers->cc >> 20) & 0xff; }
  void     cc_iocqes(unsigned v) { _registers->cc |= (((((uint32_t)v) & 0xff) << 20) | 0x1); } 

  /** 
   * I/O Submission Queue Size
   * 
   * 
   * @return The value is in bytes and is specified as a power of two (2^n).
   */
  uint32_t cc_iosqes() const     {  return ((_registers->cc >> 16) & 0xff); }
  void     cc_iosqes(unsigned v) { _registers->cc |= (((((uint32_t)v) & 0xff) << 16) | 0x1); }

  /** 
   * Shutdown Notification
   * 
   */
  void cc_shutdown_normal() { _registers->cc |= (((uint32_t)0x1 << 14) | 0x1); }
  void cc_shutdown_abrupt() { _registers->cc |= (((uint32_t)0x2 << 14) | 0x1); }


  /** 
   * Memory page size
   * 
   */
  status_t cc_memory_page_size(uint16_t ps) { 
    unsigned lmsb = leftmost_set_bit(ps);
    if(lmsb < 12) return Exokernel::E_OUT_OF_BOUNDS;
    lmsb -= 12;
    _registers->cc |= (((lmsb & 0xf) << 7) | 0x1);
  }

  unsigned cc_memory_page_size() const {
    return 1U << (((_registers->cc >> 7) & 0xf)+12);
  }


  /** 
   * Arbitration Mechanism Selected (AMS): This field selects the arbitration
   * mechanism to be used. This value shall only be changed when EN is cleared
   * to   ‘0’.      Host software shall only set this field to supported arbitration
   * mechanisms indicated in CAP.AMS. If this field is set to an unsupported value,
   * the behavior is undefined.
   * 
   * @param v 
   */
  void cc_ams(unsigned v) {
    _registers->cc |= ((v & 7) << 11);
  }

  /** 
   * I/O Command Set Selected (CSS): This field specifies the I/O Command Set
   * that is selected for use for the I/O Submission Queues. Host software shall
   * only select a supported I/O Command Set, as indicated in CAP.CSS. This field
   * shall only be changed when the controller  is  disabled  (CC.EN  is  cleared  to  ‘0’).    
   * The I/O Command Set selected shall be used for all I/O Submission Queues.
   * 
   * @param v 
   */
  void cc_css(unsigned v) {
    _registers->cc |= ((v & 7) << 4);
  }
#endif

  /** 
      Enable (EN): When   set   to   ‘1’,   then   the controller shall process commands
      based  on  Submission  Queue  Tail  doorbell  writes.    When  cleared  to  ‘0’,  then  the  
      controller shall not process commands nor post completion queue entries to
      Completion  Queues.    When  this  field  transitions  from  ‘1’  to  ‘0’,  the controller is
      reset (referred to as a Controller Reset). The reset deletes all I/O Submission
      Queues and I/O Completion Queues, resets the Admin Submission Queue and
      Completion Queue, and brings the hardware to an idle state. The reset does
      not affect PCI Express registers nor the Admin Queue registers (AQA, ASQ, or
      ACQ). All other controller registers defined in this section and internal
      controller state (e.g., Feature values defined in section 5.12.1that are not
      persistent across power states) are reset to their default values. The controller
      shall ensure that there is no data loss for commands that have had
      corresponding completion queue entries posted to an I/O Completion Queue
      prior to the reset operation. Refer to section 7.3 for reset details.
      When   this   field   is   cleared   to   ‘0’,   the   CSTS.RDY   bit   is   cleared   to   ‘0’   by   the  
      controller once the controller is ready to be re-enabled. When this field is set to
      ‘1’,  the  controller  sets  CSTS.RDY  to  ‘1’  when  it  is  ready  to  process  commands.
      Setting  this  field  from  a  ‘0’  to  a  ‘1’  when  CSTS.RDY  is  a  ‘1,’  or  setting  this  field  
      from a '1' to a '0' when CSTS.RDY is a '0,' has undefined results. The Admin
      Queue registers (AQA, ASQ, and ACQ) shall only be modified when EN is
      cleared  to  ‘0’.
  * 
  */
  void cc_enable()  {   _registers->cc |= 0x1;      }
  void cc_disable() {   _registers->cc &= (~0x1U);  }


  /** 
   * Controller status
   * 
   * 
   * @return 
   */
  uint32_t csts() const { return _registers->csts; }

  /** 
   * Controller Status - Ready
   * 
   */
  unsigned csts_ready() const { return NVME_CSTS_RDY(_registers->csts); }

  /** 
   * Controller Status - Fatal Status
   * 
   */
  unsigned csts_cfs() const { return NVME_CSTS_CFS(_registers->csts); }

  /** 
   * Controller Status - Shutdown Status 
   * 
   */
  unsigned csts_shst() const { return NVME_CSTS_SHST(_registers->csts); }


  /** 
   * Admin Queue Attr - Admin Submission Queue Size (ASQS)
   * 
   * 
   * @return Size in queue items.
   */
  unsigned asqs() const { return _registers->aqa_attr & 0xfff; }
  void asqs(unsigned v) { _registers->aqa_attr |= (v & 0xfff); }

  /** 
   * Admin Queue Attr - Admin Completion Queue Size (ASQS)
   * 
   * 
   * @return Size in queue items.
   */
  unsigned acqs() const { return ((_registers->aqa_attr >> 16) & 0xfff); }
  void acqs(unsigned v) { _registers->aqa_attr |= ((v & 0xfff) << 16); }

  /** 
   * Admin Submission Queue Base
   * 
   * 
   * @return 64bit physical address
   */
  uint64_t asq_base() const { return _registers->asq_base; }
  void asq_base(uint64_t v) { _registers->asq_base = v;  } /* bits 0:11 are reserved */

  /** 
   * Admin Completion Queue Base
   * 
   * 
   * @return 64bit physical address
   */
  uint64_t acq_base() const { return _registers->acq_base; }
  void acq_base(uint64_t v) { _registers->acq_base = v;  } /* bits 0:11 are reserved */

  
  unsigned dstrd() const { return NVME_CAP_STRIDE(_registers->cap); }


  /** 
   * Worst case time that the host will wait in 500ms units
   * 
   * 
   * @return 
   */
  unsigned timeout() const { return NVME_CAP_TIMEOUT(_registers->cap); }

  /** 
   * Whole capability register
   * 
   * 
   * @return 
   */
  volatile uint64_t cap() const { return _registers->cap; }
  

public:
  /** 
   * Print out capability information and perform basic checks
   * 
   */
  void check() {
    NVME_INFO("CAP: version: %u\n", version());
    assert(version()==1);
    NVME_INFO("CAP: MQES max queue entries = %u\n", mqes());
    NVME_INFO("CAP: CQR contiguous memory queues required = %s\n", NVME_CAP_CQR(_registers->cap) ? "yes" : "no");
    /* Doorbell stride specified in (2 ^ (2 + DSTRD)) bytes */
    NVME_INFO("CAP: stride = %lu bytes\n", 1UL << (2 + NVME_CAP_STRIDE(_registers->cap)));
    assert((1UL << (2 + NVME_CAP_STRIDE(_registers->cap)))==4); /* set up for packed stride */
    /* Min/Max Page Size (2 ^ (12 + MPS)) */
    NVME_INFO("CAP: MPSMIN min memory page size = %lu\n", (1UL << (NVME_CAP_MPSMIN(_registers->cap)+12)));
    assert((1UL << (NVME_CAP_MPSMIN(_registers->cap)+12))==4096UL);
    NVME_INFO("CAP: MPSMAX max memory page size = %lu\n", (1UL << (NVME_CAP_MPSMAX(_registers->cap)+12)));
    NVME_INFO("CAP: AMS is %s\n",NVME_CAP_AMS(_registers->cap) == 0x2 ? "weighted RR" : "vendor specific");
    NVME_INFO("CAP: NSSRS supports sub-system reset is '%s'\n",NVME_CAP_NSSRS(_registers->cap) ? "yes" : "no");
    NVME_INFO("CAP: TO timeout %d ms\n", NVME_CAP_TIMEOUT(_registers->cap) * 500);

    if(_registers->cap & (1UL << 37)) /* CSS is NVMe command set */
      NVME_INFO("confirmed NVMe command set support\n");
    else {
      NVME_INFO("unable to confirm NVMe command set support\n");
      assert(0);
    }

  }

};

#endif // __NVME_REG_WRAPPERS__
