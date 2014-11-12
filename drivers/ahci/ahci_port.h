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

#ifndef __AHCI_PORT_H__
#define __AHCI_PORT_H__

#include <libexo.h>
#include <assert.h>

//#include "irq_thread.h"
// #include "ahci_types.h"
#include "ahci_hw.h"
#include "ahci_sata.h"
#include "ahci_fis.h"

// #include "device_memory.h"
// #include <file/types.h>



/**-------------------------------------------------------------------------------------------------
 * Class for each port on the AHCI device
 * 
 */
class IRQ_thread;
class AHCI_uddk_device;

class Ahci_device_port
{  

private:
  unsigned _port_num;

  volatile ahci_port_t *   _port_reg;
  ahci_cmdhdr_t *          _command_hdr;
  addr_t                   _command_hdr_p;
  volatile uint32_t *      _command_tbl;
  addr_t                   _command_tbl_p;

  AHCI_uddk_device *       _device;
  unsigned                 _irq;
  IRQ_thread *             _ptr_irq_thread;

  addr_t _port_mem_start_p;
  uint8_t * _port_mem_start_v;

  volatile received_fis_t * _received_fis_v;
  addr_t                    _received_fis_p;

  //   Bitmap_tracker_threadsafe<32> _command_slot_tracker;

  sata_identify_data_t _device_identity;
  uint64_t _device_capacity; /* capacity of the device in bytes */
  uint64_t _device_capacity_blocks;

  /* Command frame allocator */
  Exokernel::Slab_allocator  _cmd_frame_allocator;
  
  /* Misc blocks 512 bytes each */
  Exokernel::Slab_allocator _misc_allocator;
  
  bool _initialized;

  // public:
  uint32_t get_pxis() const { return _port_reg->pxis; }
  uint32_t get_pxtfd() const { return _port_reg->pxtfd; }
  uint32_t get_pxsact() const { return _port_reg->pxsact; }
  void     set_pxsact(uint32_t val) const { _port_reg->pxsact = val; }
  uint32_t get_pxserr() const { return _port_reg->pxserr; }
  void     set_pxis(uint32_t val) { _port_reg->pxis = val; }
  uint32_t get_device_is() const;
  unsigned port_num() const { return _port_num; }
  uint64_t device_capacity() { return _device_capacity; }

private:
  void setup_memory();
  void setup_irq_thread();

  status_t identify_device();
  sata_std_command_frame_t * identify_device_cmd(ahci_cmdhdr_t *, addr_t);

  //   char _model_name[256];
  void get_model_name(char * src, char * dst, unsigned len);
  ahci_cmdhdr_t * get_command_slot(unsigned index);
  addr_t get_command_slot_p(unsigned index);

  void poll_for_port_interrupt(unsigned bit);

public:
  // ctor
  Ahci_device_port(unsigned port, 
                   AHCI_uddk_device * device, 
                   void * pctrlreg, 
                   unsigned irq);

  ~Ahci_device_port();

  void init();
  void hw_start();
  void stop();
  void dump_port_info();
  
  AHCI_uddk_device * const device() { return _device; }
  volatile ahci_port_t * const port_reg() { return _port_reg; }

private:
  status_t sync_fpdma(unsigned slot, uint64_t block, unsigned count, addr_t prdt_p, bool write);
  volatile sata_ncq_command_frame_t * setup_fpdma_read_cmd(unsigned slot, ahci_cmdhdr_t * cmd_hdr, uint64_t blocknum, unsigned count, addr_t prdt_phys);
  volatile sata_ncq_command_frame_t * setup_fpdma_write_cmd(unsigned slot, ahci_cmdhdr_t * cmd_hdr, uint64_t blocknum, unsigned count, addr_t prdt_phys);

public:
  status_t sync_fpdma_read(unsigned slot, uint64_t block, unsigned count, addr_t prdt_p);
  status_t sync_fpdma_write(unsigned slot, uint64_t block, unsigned count, addr_t prdt_p);

  uint64_t capacity_in_blocks() const { return _device_capacity_blocks; }
};


class IRQ_thread : public Exokernel::Base_thread
{
private:
  unsigned             _irq;
  unsigned             _affinity;
  Ahci_device_port *   _port;

private:
  
  /** 
   * Main entry point for the thread
   * 
   * 
   * @return 
   */
  void * entry(void *);

public:
  /** 
   * Constructor : TODO add signal based termination
   * 
   * @param irq 
   * @param affinity 
   */
  IRQ_thread(Ahci_device_port * port,
             unsigned irq, 
             unsigned affinity) : _irq(irq),
                                  _port(port),
                                  Base_thread(NULL, affinity)
  {
  }

  ~IRQ_thread() {
    exit_thread();
  }


};


#endif // __AHCI_PORT_H__
