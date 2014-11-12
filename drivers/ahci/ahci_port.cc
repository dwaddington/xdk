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
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#include <libexo.h>

#include "ahci_port.h"
#include "ahci_device.h"
#include "ahci_fis.h"
#include "ahci_sata.h"
#include "ahci_hw.h"
#include "ahci_types.h"

/* 
   The BSY bit in the Status register conveys only the device’s readiness to 
   receive another command, and does not convey the
   completion status of queued commands.
*/
#define  WAIT_FOR_NOT_BUSY while(_port_reg->pxtfd & (1 << 7)) { usleep(1); }


// ctor
Ahci_device_port::Ahci_device_port(unsigned port, AHCI_uddk_device * device, void * pctrlreg, unsigned irq) : 
  _port_num(port),
  _port_reg((ahci_port_t *)pctrlreg),
  _device(device),
  _irq(irq),
  _initialized(false),
  _cmd_frame_allocator(device, 4096, 10),
  _misc_allocator(device, 4096, 8)
{
  assert(device);
  assert(pctrlreg);
}


Ahci_device_port::~Ahci_device_port() {
  if(_ptr_irq_thread)
    delete _ptr_irq_thread;
}


/** 
 * Start the port h/w
 * 
 */
void Ahci_device_port::hw_start()
{
  AHCI_INFO("starting AHCI port hw.\n");
  
  ahci_port_cmd_t pxcmd;
        
  pxcmd.u32 = _port_reg->pxcmd;
  
  /* Frame receiver disabled. */
  pxcmd.fre = 0;

  /* disable command processing */
  pxcmd.st = 0;        
  _port_reg->pxcmd = pxcmd.u32;

  /* Clear interrupt status. */
  _port_reg->pxis = 0xffffffff;
        
  /* Clear error status. */
  _port_reg->pxserr = 0xffffffff;
        
  /* Enable all interrupts. */
  _port_reg->pxie = 0xf; //fffffff;

  /* Frame receiver enabled. */
  pxcmd.fre = 1;

  /* Enable process the command list. */
  pxcmd.st = 1;
        
  _port_reg->pxcmd = pxcmd.u32;

  __sync_synchronize();
}



/** 
 * Stop processing on the port
 * 
 */
void Ahci_device_port::stop()
{
  ahci_port_cmd_t pxcmd;       

  /* disable interrupts */
  _port_reg->pxie = 0;

  pxcmd.u32 = _port_reg->pxcmd;

  /* stop command processing */
  pxcmd.st = 0;
  pxcmd.fr = 0;
  _port_reg->pxcmd = pxcmd.u32;

  AHCI_INFO("[AHCI-PORT]: Stopping AHCI port\n");

  
  while(!_port_reg->pxcmd & AHCI_PORT_CMD_FR);

  /* clear FRE bit */
  _port_reg->pxcmd &= (~(1<<4));
}


/** -----------------------------------------------------------------------------
 * Port initialzation; triggered by port enumeration stage at the device-level
 *  -----------------------------------------------------------------------------
 */
void Ahci_device_port::init()
{
  ahci_port_ssts_t pxssts;
  pxssts.u32 = _port_reg->pxssts;
  rmb();
  PLOG("PXSSTS = %x",pxssts.u32);
  /* only initialize present devices */
  if(pxssts.det == 0x3) { /* present and active */

    if(_port_reg->pxsig!=0x101) {  /* only initialize ATA devices not ATAPI (e.g., CDROM) */
      AHCI_INFO("Port [%u] detected as non-ATA device.\n",_port_num);
      return;
    }
    
    uint32_t version = _device->ctrl()->vs;
    AHCI_INFO("Initializing active SATA port %u\n",_port_num);  
    AHCI_INFO("  version: %x.%x\n", (version >> 16), version && 0xffff);

    /* set up memory */
    setup_memory();

    /* start port */
    hw_start();

    /* check we are in H:idle */
    {
      ahci_port_ssts_t pxssts;
      pxssts.u32 = _port_reg->pxssts;
      /* only initialize active device */
      assert(pxssts.det==0x3);
    }

    /* set up asynchronous IRQ handler thread */
    setup_irq_thread();
    /* run IDENTIFY DEVICE command */
    identify_device();

    _port_reg->pxis = ~0;

    _initialized = true;

    AHCI_INFO("Port [%u] is active.\n",_port_num);
    //    dump_port_info();
  }
  else {
    AHCI_INFO("Port [%u] not active.\n",_port_num);
  }
}


/** 
 * Construct an IDENTIFY_DEVICE command
 * 
 * @param cmdhdr Pointer to command header (slot)
 * @param phys Physical address of the DMA area
 * 
 * @return Constructed command frame
 */
sata_std_command_frame_t * 
Ahci_device_port::
identify_device_cmd(ahci_cmdhdr_t * cmdhdr, addr_t prdt_phys)
{
  assert(_command_hdr);
  assert(_port_reg);

  sata_std_command_frame_t *cmd = 0;
  addr_t cmd_phys = 0;

  cmd = (sata_std_command_frame_t *) _cmd_frame_allocator.alloc(&cmd_phys);

  assert(cmd);
  assert(cmd_phys);
        
  cmd->fis_type = SATA_CMD_FIS_TYPE; // 0x27
  cmd->c = SATA_CMD_FIS_COMMAND_INDICATOR;
  cmd->command = 0xEC; // IDENTIFY DEVICES (PIO-IN) command
  cmd->features = 0;
  cmd->lba_lower = 0;
  cmd->device = 0;
  cmd->lba_upper = 0;
  cmd->features_upper = 0;
  cmd->count = 0;
  cmd->icc = 0;
  cmd->control = 0;
  cmd->reserved2 = 0;
        
  /* get hold of PRDT section */
  volatile ahci_cmd_prdt_t *prdt = (volatile ahci_cmd_prdt_t *) &cmd->prdt;
        
  prdt->data_address_low = prdt_phys;
  prdt->data_address_upper = 0; 
  prdt->reserved1 = 0;
  prdt->dbc = SATA_IDENTIFY_DEVICE_BUFFER_LENGTH - 1;
  prdt->reserved2 = 0;
  prdt->ioc = 0;/* last one should interrupt on completion */
        
  /* set up command header */
  {    
    cmdhdr->flags = AHCI_CMDHDR_FLAGS_CLEAR_BUSY_UPON_OK | AHCI_CMDHDR_FLAGS_5DWCMD | 0x80;
    cmdhdr->prdtl = 1;
    cmdhdr->bytesprocessed = 0;
    /* connect to command table */
    cmdhdr->cmdtable = LO(cmd_phys);
    cmdhdr->cmdtableu = HI(cmd_phys);
  }
        
  return cmd;
}

void Ahci_device_port::poll_for_port_interrupt(unsigned bit)
{
  while((get_pxis() & (1 << bit))  == 0) cpu_relax();
}


/* taken from Linux; id is u16 type */
#define ata_id_has_fpdma_aa(id) \
        ( (((id)[76] != 0x0000) && ((id)[76] != 0xffff)) && \
          ((id)[78] & (1 << 2)) )

status_t Ahci_device_port::identify_device()
{
  AHCI_INFO("identify device entry\n");

  addr_t phys = 0;
  sata_identify_data_t *idata;

  /* allocate from the misc allocator */
  AHCI_INFO("allocating DMA memory.\n");
  idata = (sata_identify_data_t *) _misc_allocator.alloc(&phys);

  assert(idata);
  assert(phys);

  __builtin_memset(idata,0x0,512); //SATA_IDENTIFY_DEVICE_BUFFER_LENGTH);

  WAIT_FOR_NOT_BUSY;

  /* set up IDENTIFY_DEVICE command */
  const unsigned slot = 0;
  sata_std_command_frame_t * cmd = identify_device_cmd(get_command_slot(slot),phys);

  /* notify IRQ thread to look out for this; this is non-NCQ 
     so there won't be any PxSACT bit clearance.  This is why
     we don't set the PxSACT bit.
   */
  //  _device->set_slot_std_pending(_port_num,slot);

  AHCI_INFO("issuing identify command..\n");

  /* Issue command. Do not set PxSACT bit */  
  _port_reg->pxci = (1<<slot);
  wmb();

  /* IDENTIFY DEVICE is a PIO Setup FIS.
   */

  /* wait for IRQ to fire */
  {
    /* poll interrupt status */
    AHCI_INFO("polling interrupt status");

    if((device()->device_id() == 0x2829 || device()->device_id() == 0x2922) &&
       (device()->vendor() == 0x8086)) {
      // Virtualbox / QEMU
      poll_for_port_interrupt(0); // wait for Device to Host Register FIS Interrupt (DHRS)
    }
    else {
      poll_for_port_interrupt(1); // wait for PIO Setup FIS Interrupt (PSS)
    }

    AHCI_INFO("PXIS=0x%x\n",get_pxis());
    AHCI_INFO("PXSACT=0x%x\n",get_pxsact());

    /* clear port interrupts */
    set_pxis(~0);
    
    /* disable interrupts */
    _device->ctrl()->ghc &= ~AHCI_GHC_GHC_IE;

    /* clear controller level interrupt bits */
    _device->ctrl()->is = ~0;
    wmb();
  }
   
  WAIT_FOR_NOT_BUSY;

  assert(_port_reg);
  assert(_port_reg->pxserr!=0x400000);

  /* store results */
  __builtin_memcpy(&_device_identity,idata,sizeof(sata_identify_data_t));

  /* sanity check capabilities */
  {
    AHCI_INFO("Queue depth: %u\n",_device_identity.queue_depth);
    AHCI_INFO("NCQ support: %s\n",(_device_identity.sata_cap & (1<<8)) ? "yes" : "no");
    //    assert(_device_identity.sata_cap & (1<<8));
    AHCI_INFO("Non-zero offsets: %s\n",(_device_identity.serial_ata_features[0] & 0x1) ? "yes" : "no");    

    /* check for fpdma support */
    if(!ata_id_has_fpdma_aa((uint16_t *)&_device_identity)) {
      AHCI_INFO("Device does not support FPDMA!\n");
    }
    else {
      AHCI_INFO("FPDMA support: yes\n");
    }
  }

  _cmd_frame_allocator.free(cmd);
  _misc_allocator.free(idata);

  ahci_port_is_t pxis = _port_reg->pxis;
  if(pxis & AHCI_PORT_IS_TFES) { // task file error
    panic("Unexpected task-file error!!");
  }
  else {
    char model_name[40];
    __builtin_memset(model_name,0,40);
    get_model_name((char*)_device_identity.model_name, model_name,40);
    AHCI_INFO("Device Model: (%s)\n",model_name);

    uint64_t total_blocks = _device_identity.total_lba48_0 |
      ((uint64_t)_device_identity.total_lba48_1) << 16 |
      ((uint64_t)_device_identity.total_lba48_2) << 32 |
      ((uint64_t)_device_identity.total_lba48_3) << 48;

    _device_capacity_blocks = total_blocks;
    _device_capacity = total_blocks * SATA_DEFAULT_BLOCK_SIZE;
    AHCI_INFO("Device Capacity (%lu) bytes\n", 
           _device_capacity);
  }
  return Exokernel::S_OK;
}



/** 
 * Setup memory areas for command list and FIS receive (Figure 4.)
 * 
 */
ahci_cmdhdr_t * Ahci_device_port::get_command_slot(unsigned index) {
  assert(_command_hdr);
  return _command_hdr + index;
}

/** 
 * Get the physical address of command slot N
 * 
 * @param index Index value
 * 
 * @return Physical address of the command header
 */
addr_t Ahci_device_port::get_command_slot_p(unsigned index) {
  assert(index < 32);
  return _command_hdr_p + (index * sizeof(ahci_cmdhdr_t));
}


void Ahci_device_port::setup_memory()
{
  addr_t phys = 0;

  /* allocate a 4k page of DMA memory */
  byte * virt = (byte*) _device->alloc_dma_pages(1,(addr_t*)&phys);

  assert(phys);
  assert(virt);
  __builtin_memset(virt,0,PAGE_SIZE);

  PLOG("allocated memory at v=%lx p=%lx",(addr_t)virt,phys);

  /* allocate and init received FIS structure (256 bytes) assuming no FBS */
  _received_fis_p = phys;
  _received_fis_v = (received_fis_t *) virt; 
  _port_reg->pxfb = LO(phys); 
  _port_reg->pxfbu = HI(phys); 
  phys+=1024;
  virt+=1024; /* 1K align command list (RFIS only needs 256 bytes) */

  /* command list structure (i.e., list of command headers */
  _port_reg->pxclb = LO(phys); 
  _port_reg->pxclbu = HI(phys); 
  _command_hdr = (ahci_cmdhdr_t *) virt;
  _command_hdr_p = phys;

  AHCI_INFO("Address for command headers %p (phys=%p)\n",
            (void*)virt, (void*) phys);
  phys+=1024;
  virt+=1024; /* 32 entries of 32 bytes each */
  
  /* note: command tables are set up on demand using a dynamic allocator */
  wmb();
  AHCI_INFO("memory setup complete OK.\n");
}

void Ahci_device_port::setup_irq_thread() {
  assert(_irq > 0);
  _ptr_irq_thread = new IRQ_thread(this, _irq, -1);
  assert(_ptr_irq_thread);
}


void Ahci_device_port::dump_port_info() {    
  PINF("PORT[%u]---------------------------------------\n",_port_num);
  PINF("Active = %s\n", _port_reg->pxsact ? "yes" : "no");
  if(_port_reg->pxsact) {
    PINF("FBS enabled = %s\n", _port_reg->pxfbs & 0x1 ? "yes" : "no"); /* section 3.3.16 */
  }
}



void Ahci_device_port::get_model_name(char * src, char * dst, unsigned len)
{
  unsigned i=0;
  while(src[i] && i < len) {
    
    if(src[i+1]) {
      dst[i] = src[i+1];
    }
    dst[i+1] = src[i];

    i+=2;
  }

  dst[i] = '\0';
}

uint32_t Ahci_device_port::get_device_is() const { 
  //  return _device->ctrl()->is; 
  return 0;
}


/** 
 * Perform a synchronous first-party DMA read
 * 
 * @param slot Slot number
 * @param block Sector number
 * @param count Number of sector
 * @param prdt_p Destination memory PRDT
 * 
 * @return 
 */

status_t Ahci_device_port::sync_fpdma_read(unsigned slot, 
                                           uint64_t block, 
                                           unsigned count, 
                                           addr_t prdt_p)
{
  return sync_fpdma(slot,block,count,prdt_p,false);
}

/** 
 * Perform a synchronous first-party DMA write
 * 
 * @param slot Slot number
 * @param block Sector number
 * @param count Number of sector
 * @param prdt_p Destination memory PRDT
 * 
 * @return 
 */
status_t Ahci_device_port::sync_fpdma_write(unsigned slot, 
                                            uint64_t block, 
                                            unsigned count, 
                                            addr_t prdt_p)
{
  return sync_fpdma(slot,block,count,prdt_p,true);
}


/** 
 * Generalized function for first-party DMA
 * 
 * @param slot Command slot to use
 * @param blocknum Starting sector
 * @param count Number of sectors to r/w
 * @param prdt_p DMA area
 * @param write 1=write 0=read
 * 
 * @return S_OK on success
 */
status_t Ahci_device_port::sync_fpdma(unsigned slot,
                                      uint64_t blocknum, 
                                      unsigned count, 
                                      addr_t prdt_p, 
                                      bool write)
{

  if(!_initialized) 
    panic("[AHCI]: call to sync_fpdma before initialization is complete.\n");


  /* get the command header for this slot */
  ahci_cmdhdr_t * cmdhdr = get_command_slot(slot);
  assert(cmdhdr);

#ifdef AHCI_VERBOSE
  if(write) 
    AHCI_INFO("fpdma_write port(%u) block:[%lu] count:[%u blocks] using slot (%u)\n",
              _port_num,blocknum,count,slot);
  else
    AHCI_INFO("fpdma_read port(%u) block:[%lu] count:[%u blocks] using slot (%u)\n",
              _port_num,blocknum,count,slot);
#endif 
  
  volatile sata_ncq_command_frame_t * cmd;
  if(write) {
    /* set up command */
    cmd = setup_fpdma_write_cmd(slot,cmdhdr,blocknum,count,prdt_p);
  }
  else {
    /* set up command */
    cmd = setup_fpdma_read_cmd(slot,cmdhdr,blocknum,count,prdt_p);
  }

  WAIT_FOR_NOT_BUSY;

	/*
	 * The barrier is required to ensure that writes to cmd_block reach
	 * the memory before the write to PORT_CMD_ACTIVATE.
	 */
  wmb();

  /* issue */
  {
    _port_reg->pxsact = (1<<slot);
    _port_reg->pxci = (1<<slot);
  }

  wmb();

  WAIT_FOR_NOT_BUSY;
 
  poll_for_port_interrupt(3); // wait for Set Device Bits Interrupt (SDBS)
  
  // AHCI_INFO("PXIS=0x%x\n",get_pxis());
  // AHCI_INFO("PXSACT=0x%x\n",get_pxsact());

  set_pxis(0x8); // clear interrupt

  _cmd_frame_allocator.free((sata_std_command_frame_t *)cmd);  

  return Exokernel::S_OK;
}


/** Read one sector from the SATA device using FPDMA.
 *
 * @param phys     Physical address of buffer for sector data.
 * @param blocknum Block number to read.
 *
 */
/** 
 * Perform a first-party DMA read
 * 
 * @param cmdhdr 
 * @param block Starting sector (each sector is 512 bytes)
 * @param count Number of sectors
 * @param prdt_phys Physical buffer address
 * 
 * @return Pointer to command frame (caller release)
 */
volatile sata_ncq_command_frame_t * 
Ahci_device_port::setup_fpdma_read_cmd(unsigned slot, 
                                       ahci_cmdhdr_t * cmdhdr, 
                                       uint64_t blocknum, 
                                       unsigned count, 
                                       addr_t prdt_phys)
{

  /* allocate command frame */
  volatile sata_ncq_command_frame_t *cmd = 0;
  addr_t cmd_phys = 0;

  cmd = (sata_ncq_command_frame_t *) _cmd_frame_allocator.alloc(&cmd_phys);

  assert(cmd);
  assert(cmd_phys);

  //  assert(sizeof(sata_ncq_command_frame_t) <= sizeof(sata_std_command_frame_t));
  __builtin_memset((void*)cmd,0,sizeof(sata_ncq_command_frame_t));

  /* set up Read first-party DMA queued command */
  cmd->fis_type = SATA_CMD_FIS_TYPE; // 0x27 Register Host-to-Device FIS
  cmd->c = SATA_CMD_FIS_COMMAND_INDICATOR;
  cmd->command = 0x60;
  cmd->control = 0;
        
  cmd->reserved1 = 0;
  cmd->icc = 0;
  cmd->reserved3 = 0;
  cmd->reserved4 = 0;
  cmd->reserved5 = 0;
  cmd->reserved6 = 0;
        
  //  cmd->tag = 1; // tag field (identifies amongst outstanding commands
  assert(count != 0);
  cmd->sector_count_low = count & 0xff;
  cmd->sector_count_high = (count >> 8) & 0xff;
        
  cmd->lba0 = blocknum & 0xff;
  cmd->lba1 = (blocknum >> 8) & 0xff;
  cmd->lba2 = (blocknum >> 16) & 0xff;
  cmd->lba3 = (blocknum >> 24) & 0xff;
  cmd->lba4 = (blocknum >> 32) & 0xff;
  cmd->lba5 = (blocknum >> 40) & 0xff;

  cmd->tag = slot << 3;

  /* set up PRDT */
  volatile ahci_cmd_prdt_t *prdt = (volatile ahci_cmd_prdt_t *) &cmd->prdt;

  prdt->data_address_low = LO(prdt_phys);
  prdt->data_address_upper = HI(prdt_phys);
  prdt->reserved1 = 0;
  prdt->dbc = (SATA_DEFAULT_BLOCK_SIZE * count) - 1;
  prdt->reserved2 = 0;
  prdt->ioc = 0;


  /* set up command header */
  {    
    cmdhdr->flags = 5; // command fis h2d is 5 dwords
    cmdhdr->prdtl = 1;
    cmdhdr->bytesprocessed = 0;
    cmdhdr->flags |= AHCI_CMDHDR_FLAGS_CLEAR_BUSY_UPON_OK | AHCI_CMDHDR_FLAGS_5DWCMD;
    cmdhdr->cmdtable = LO(cmd_phys);
    cmdhdr->cmdtableu = HI(cmd_phys);
  }

  return cmd;
}


volatile sata_ncq_command_frame_t * Ahci_device_port::setup_fpdma_write_cmd(unsigned slot, 
                                                                            ahci_cmdhdr_t * cmdhdr, 
                                                                            uint64_t blocknum, 
                                                                            unsigned count,
                                                                            addr_t prdt_phys)
{
  /* allocate command frame */
  volatile sata_ncq_command_frame_t *cmd = 0;
  addr_t cmd_phys = 0;

  cmd = (sata_ncq_command_frame_t *) _cmd_frame_allocator.alloc(&cmd_phys);

  assert(cmd);
  assert(cmd_phys);

  assert(sizeof(sata_ncq_command_frame_t) <= sizeof(sata_std_command_frame_t));
  __builtin_memset((void*)cmd,0,sizeof(sata_ncq_command_frame_t));

  cmd->fis_type = SATA_CMD_FIS_TYPE;
  cmd->c = SATA_CMD_FIS_COMMAND_INDICATOR;
  cmd->command = 0x61;
  cmd->control = 0;
  
  cmd->reserved1 = 0;
  cmd->icc = 0;
  cmd->reserved3 = 0;
  cmd->reserved4 = 0;
  cmd->reserved5 = 0;
  cmd->reserved6 = 0;
        
  cmd->sector_count_low = count & 0xff;
  cmd->sector_count_high = (count >> 8) & 0xff;
        
  cmd->lba0 = blocknum & 0xff;
  cmd->lba1 = (blocknum >> 8) & 0xff;
  cmd->lba2 = (blocknum >> 16) & 0xff;
  cmd->lba3 = (blocknum >> 24) & 0xff;
  cmd->lba4 = (blocknum >> 32) & 0xff;
  cmd->lba5 = (blocknum >> 40) & 0xff;
        
  cmd->tag = slot << 3;

  /* set up PRDT */
  volatile ahci_cmd_prdt_t *prdt = (volatile ahci_cmd_prdt_t *) &cmd->prdt;

  prdt->data_address_low = LO(prdt_phys);
  prdt->data_address_upper = HI(prdt_phys);
  prdt->reserved1 = 0;
  prdt->dbc = (SATA_DEFAULT_BLOCK_SIZE * count) - 1;
  prdt->reserved2 = 0;
  prdt->ioc = 0; /* interrupt on completion */

  /* set up command header */
  {    
    cmdhdr->flags = 5; // command fis h2d is 5 dwords
    cmdhdr->prdtl = 1;
    cmdhdr->bytesprocessed = 0;

    cmdhdr->flags =     
      AHCI_CMDHDR_FLAGS_CLEAR_BUSY_UPON_OK |
      AHCI_CMDHDR_FLAGS_WRITE |
      AHCI_CMDHDR_FLAGS_5DWCMD;

    cmdhdr->cmdtable = LO(cmd_phys);
    cmdhdr->cmdtableu = HI(cmd_phys);
  }
        
  return cmd;
}


void * IRQ_thread::entry(void *) {
  assert(_port);
  unsigned count = 0;
  while(!thread_should_exit()) {
    _port->device()->wait_for_msi_irq(_irq);

    PDBG("IRQ thread fired.");
  }
}


