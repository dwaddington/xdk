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

#ifndef __NVME_DEVICE_H__
#define __NVME_DEVICE_H__

#include <libexo.h>
#include "nvme_common.h"
#include "nvme_registers_wrappers.h"
#include "nvme_queue.h"
#include "cq_thread.h"
#include "config.h"

#define CONFIG_MAX_IO_QUEUES 32 /* increase this to support more queues */

/** 
 * Devices that this driver works with
 * 
 */
static Exokernel::device_vendor_pair_t dev_tbl[] = {{0x8086,0x5845}, // Intel SSD (QEMU)
                                                    {0x144d,0xa820}, // Samsung XS1715
                                                    {0x8086,0x0953}, // Intel P750
                                                    {0,0}};

class NVME_device : public Exokernel::Pci_express_device
{
private:
  enum { verbose = 1 };

  Pci_mapped_memory_region * _mmio;
  NVME_registers *           _regs;

  NVME_admin_queue *         _admin_queues;
  std::vector<unsigned>      _msi_vectors;

  NVME_IO_queue *            _io_queues[CONFIG_MAX_IO_QUEUES];
  unsigned                   _num_io_queues;

  Config                     _config;
  
public:
  struct {
    uint16_t _vid;             // vendor ID
    uint16_t _ssvid;           // PCI subsystem vendor
    byte     _firmware_rev[8]; // firmware revision
    uint16_t _oacs;            // optional admin command support
    uint32_t _nn;              // number of namespaces
  } _ident;

  struct ns_info {
    uint64_t _nsze;     // total size of the namespace in logical blocks
    uint64_t _ncap;     // maximum number of blocks that can be allocated at one time
    uint32_t _flbasize; // formatted LBA size
  };

  std::vector<ns_info *> _ns_ident;
  
public:

  /** 
   * Constructor
   * 
   */
  NVME_device(const char * config_filename, unsigned device_instance) 
    : Exokernel::Pci_express_device(device_instance, dev_tbl), 
      _mmio(NULL), 
      _regs(NULL),
      _admin_queues(NULL),
      _config(config_filename)
  { 
    nvme_init_device();
  }

  ~NVME_device();

  /** 
   * Initial the device. Should be called once only.  Sets up register
   * mappings, creates IO queues, etc.
   * 
   */
  void nvme_init_device();

  Config& config() { return _config; }


  /** 
   * Reset the device using the device configuration control register
   * 
   */
  void hw_reset();

  /** 
   * Prepare for graceful shut down of the device.
   * 
   */
  void destroy();

  /** 
   * Accessors
   * 
   */
  INLINE NVME_registers * regs() const { return _regs; }
  INLINE NVME_IO_queue * io_queue(unsigned qid) { 
    assert(qid > 0);
    return _io_queues[qid-1]; 
  }

  NVME_admin_queue * admin_queues() const { return _admin_queues; }

#if 0  
  unsigned register_callback(unsigned queue_id,
                             notify_callback_t callback,
                             void * callback_param) {

    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      assert(0);
      return Exokernel::E_INVAL;
    }
    assert(_io_queues[queue_id - 1]);
    return _io_queues[queue_id - 1]->register_callback(callback,
                                                       callback_param);
  }
#endif

  /** 
   * Main entry to perform asynchronous read
   * 
   * @param queue_id Queue identifier counting from 0
   * @param prp1 Physical address of memory for DMA
   * @param offset Logical block address
   * @param num_blocks Number of blocks to read
   * @param sequential Hint - this is part of a sequential read
   * @param access_freq Hint - this is a high freqency data item
   * @param access_lat Hint - latency requirements
   * @param nsid - Namespace identifier
   * 
   * @return S_OK on success
   */
  status_t block_async_read(unsigned queue_id,
                            addr_t prp1,
                            off_t offset,
                            size_t num_blocks,
                            bool sequential=false, 
                            unsigned access_freq=0, 
                            unsigned access_lat=0,
                            unsigned nsid=1) 
  {
    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      assert(0);
      return Exokernel::E_INVAL;
    }

    return _io_queues[queue_id - 1]->issue_async_read(prp1,
                                                      offset,
                                                      num_blocks,
                                                      sequential,
                                                      access_freq,
                                                      access_lat,
                                                      nsid);
  }




  /** 
   * Main entry to perform asynchronous write
   * 
   * @param queue_id Queue identifier counting from 0
   * @param prp1 Physical address of memory for DMA
   * @param offset Logical block address
   * @param num_blocks Number of blocks to write
   * @param sequential Hint - this is part of a sequential write
   * @param access_freq Hint - this is a high freqency data item
   * @param access_lat Hint - latency requirements
   * @param nsid - Namespace identifier
   * 
   * @return 
   */
  status_t block_async_write(unsigned queue_id,
                             addr_t prp1,
                             off_t offset,
                             size_t num_blocks,
                             bool sequential=false, 
                             unsigned access_freq=0, 
                             unsigned access_lat=0,
                             unsigned nsid=1) 
  {
    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      assert(0);
      return Exokernel::E_INVAL;
    }
    assert(_io_queues[queue_id - 1]);
    return _io_queues[queue_id - 1]->issue_async_write(prp1,
                                                       offset,
                                                       num_blocks,
                                                       sequential,
                                                       access_freq,
                                                       access_lat,
                                                       nsid);
  }

  status_t async_io_batch(unsigned queue_id,
                          io_request_t* io_desc,
                          uint64_t length
                          )
  {
    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      PERR("queue id (%u) > num IO queues! (%u)", queue_id, _num_io_queues);
      assert(0);
      return Exokernel::E_INVAL;
    }
    assert(_io_queues[queue_id - 1]);
    return _io_queues[queue_id - 1]->issue_async_io_batch(io_desc, length);
  }

  status_t flush(unsigned nsid, unsigned queue_id)
  {
    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      assert(0);
      return Exokernel::E_INVAL;
    }
    assert(_io_queues[queue_id - 1]);
    return _io_queues[queue_id - 1]->issue_flush(); //TODO: add nsid
  }


  status_t wait_io_completion(unsigned queue_id)
  {
    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      assert(0);
      return Exokernel::E_INVAL;
    }
    assert(_io_queues[queue_id - 1]);
    return _io_queues[queue_id - 1]->wait_io_completion();
  }


  uint16_t next_cmdid(unsigned queue_id) {
    if((queue_id > _num_io_queues)||(queue_id == 0)) {
      assert(0);
      return Exokernel::E_INVAL;
    }

    return _io_queues[queue_id - 1]->next_cmdid();
  }



};


#endif // __NVME_DEVICE_H__
