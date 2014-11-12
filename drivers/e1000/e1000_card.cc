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
          @author Chen (chen.tian@samsung.com)
          @date: Aug 3, 2012
*/

#include "e1000_card.h"
#include "mptable-itf-cli.h"

using namespace OmniOS;
using namespace E1000;


E1000_card::E1000_card(){}

bool E1000_card::init() {
  if (!pci_search_e1000())
    return false;

  if (!mac_init())
    return false;

#if 0
  if (!irq_init(true)) 
    return false;

//control
  uint32_t ctrl = mmio_read32(E1000_CTRL);
	ctrl |= CTRL_FRCSPD;
	ctrl |= CTRL_FRCDPLX;
  ctrl |= CTRL_FD;
	ctrl &= ~(CTRL_SPEED_MASK);
  ctrl |= CTRL_SPEED_1000 << CTRL_SPEED_SHIFT;
	mmio_write32(E1000_CTRL, ctrl);
#endif

//status
  uint32_t status = mmio_read32(E1000_STATUS);

	if (status & STATUS_FD)
    full_duplex = true;
	else
    full_duplex = false;
	
	uint32_t speed_bits =
	    (status >> STATUS_SPEED_SHIFT) & STATUS_SPEED_ALL;
	
	if (speed_bits == STATUS_SPEED_10)
		speed = 10;
	else if (speed_bits == STATUS_SPEED_100)
		speed = 100;
	else if ((speed_bits == STATUS_SPEED_1000A) ||
	    (speed_bits == STATUS_SPEED_1000B))
		speed = 1000;

//rx&tx
  tx_init();
  rx_init();
  snd_counter=0;
  rcv_counter=0;
  ip[0] = 0;
  ready = false;
  dump();
  assert(use_msi);
  is_up = false;
  return true;
}

void E1000_card::activate() {
// link up
	mmio_write32(E1000_CTRL, (mmio_read32(E1000_CTRL) & ~CTRL_SLU));		
  usleep(10);
	mmio_write32(E1000_CTRL, (mmio_read32(E1000_CTRL) | CTRL_SLU));		
  is_up = true;
 
//enable interrupts
  mmio_write32(E1000_IMS, 0x1F6DC);
  mmio_read32(E1000_ICR);

// set the transmit control register (padshortpackets)
	mmio_write32(E1000_TCTL, (TCTL_EN | TCTL_PSP) );

 // mmio_write32(E1000_RCTL, (RCTL_SBP | RCTL_UPE | RCTL_MPE | RDMTS_HALF | RCTL_SECRC | 
//									RCTL_LPE | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_EN));
  mmio_write32(E1000_RCTL, (RCTL_MPE | RDMTS_HALF | RCTL_BSIZE_2048 | RCTL_SECRC |RCTL_EN));
  ready = true;
}

void E1000_card::wait_for_activate() {
  while (!ready) 
    usleep(10000); 
}

bool E1000_card::irq_init(bool use_mptable) {
  if (use_mptable) 
    irq_num = 10;
  else {
    /*  declare proxy from remote service */
    Module::MP_table::Query_proxy mptable("mptable_service"); 
    /* use the MP specification table to get the actual GSI IRQ */
    short irq = mptable.get_irq(irq_pin - 1, pciID);
    if (irq < 0) return false;
    irq_num = (uint16_t )irq;
  }
  return true;
}


void E1000_card::tx_init() {
  uint32_t tmp_v, tmp_p;
	tmp_v = (uint32_t)env()->alloc_pages(NUM_TX_BUF_PAGES, EAGER_MAPPING, (addr_t *)&tmp_p, false); 
  
  Address buf(tmp_v, tmp_p);
  
  assert(buf.p()%16==0);
	tx_buf  = (uint8_t *)buf.p(); 

	for(int i = 0; i < NUM_TX_DESCRIPTORS; i++ ) {
		tx_desc[i] = (TX_DESC*)(buf.v()+ (i * sizeof(TX_DESC)));
		tx_desc[i]->address = 0;
		tx_desc[i]->cmd = 0;
	}
	
	// notify the card of the ring buffer of transmit descriptors 
	mmio_write32(E1000_TDBAH(0), 0); 
	mmio_write32(E1000_TDBAL(0), (uint32_t)tx_buf);
  
	// buffer length
	mmio_write32(E1000_TDLEN(0), (uint32_t)(NUM_TX_DESCRIPTORS * sizeof(TX_DESC)));
	
	// head and tail pointers
	mmio_write32(E1000_TDH(0), 0);
	mmio_write32(E1000_TDT(0), 0);
	tx_buf_pos= 0;
	
	return;
}

void E1000_card::rx_init() {
  uint32_t tmp_v, tmp_p;
	tmp_v = (uint32_t)env()->alloc_pages(NUM_RX_BUF_PAGES, EAGER_MAPPING, (addr_t *)&tmp_p, false); 
  memset((void*)tmp_v, 0, NUM_RX_BUF_PAGES*L4_PAGESIZE);
  Address buf(tmp_v, tmp_p);
  assert(buf.p()%16==0);
	rx_buf.v(tmp_v);
	rx_buf.p(tmp_p);
  
  uint32_t pkt_v, pkt_p;
  uint32_t pkt_num_pages = (NUM_RX_DESCRIPTORS * PKT_MAX_SIZE)/L4_PAGESIZE; 
	pkt_v = (uint32_t)env()->alloc_pages(pkt_num_pages, EAGER_MAPPING, (addr_t *)&pkt_p); 
  memset((void*)pkt_v, 0, (NUM_RX_DESCRIPTORS * PKT_MAX_SIZE));
  rx_pkt_buf.v(pkt_v); 
  rx_pkt_buf.p(pkt_p); 
  assert(pkt_p);
  assert(rx_pkt_buf.p()%16==0);

  for(int i = 0; i < NUM_RX_DESCRIPTORS; i++ ) {
	  rx_desc[i] = (RX_DESC*)(rx_buf.v()+ (i * sizeof(RX_DESC)));
    reset_rx_desc(i);
	}

	// notify the card of the ring buffer of transmit descriptors 
	mmio_write32(E1000_RDBAH(0), 0); 
	mmio_write32(E1000_RDBAL(0), (uint32_t)rx_buf.p());
  
  
	// buffer length
	assert(sizeof(RX_DESC) == 16);
	mmio_write32(E1000_RDLEN(0), (uint32_t)(NUM_RX_DESCRIPTORS * sizeof(RX_DESC)));
	mmio_write32(E1000_RDH(0), 0);
	mmio_write32(E1000_RDT(0), NUM_RX_DESCRIPTORS); 
	rx_buf_pos= 0;

}

void E1000_card::print_rx_desc(int index) {
    uint32_t pkt_v = (rx_pkt_buf.v()+(index * PKT_MAX_SIZE));
    printf("rx_desc %d is at v=%p p=%p: pkt addr[v=0x%x, p=0x%x],"
        " len %d, checksum 0x%x, status 0x%x, errors 0x%x, special 0x%x\n",
    index, rx_desc[index],    
    (RX_DESC*)(rx_buf.p()+ (index * sizeof(RX_DESC))),
    pkt_v,
		(uint32_t)rx_desc[index]->address, 
    rx_desc[index]->length ,
    (uint32_t)rx_desc[index]->checksum,
    (uint32_t)rx_desc[index]->status  ,
    (uint32_t)rx_desc[index]->errors  ,
    (uint32_t)rx_desc[index]->special);
}

void E1000_card::reset_rx_desc(int index) {
#if 0
    uint32_t pkt_v = (rx_pkt_buf.v()+(index * PKT_MAX_SIZE));
    memset((void*)pkt_v, 0,  PKT_MAX_SIZE);
    ((uint8_t*)pkt_v)[0] = 'A';
#endif
    uint32_t pkt_p = (rx_pkt_buf.p()+(index * PKT_MAX_SIZE));
		rx_desc[index]->address   = (uint64_t)pkt_p;
    rx_desc[index]->length    = 0;
    rx_desc[index]->checksum  = 0;
    rx_desc[index]->status    = 0;
    rx_desc[index]->errors    = 0;
    rx_desc[index]->special   = 0;
}

void E1000_card::dump() {
   info("\n\t[e1000]:\n \tFound An %s %s Ethernet Card [PCI ID %x]:\n"
         "\tBar_mmio: %x bar_port: %x \n\tMMIO: p %x, v %x size %x \n"
         "\tirq_pin %d, irq No. %d %s \n"
         "\t%s DUPLEX, %d M bits/s. \n\tMAC Address: ",
          (vendor==0x8086)?"Intel":"Non-Intel",
          (device==0x100e)?"82540EM-A":"8254xx",
          pciID, bar_mmio, bar_port, mmio.p(), mmio.v(), mmio_size, 
          irq_pin, irq_num, use_msi?"use MSI":"",
          full_duplex?"FULL":"HALF", speed
          );

    for(int i=0; i<6; i++) {
      info("%x", mac[i]); 
      if(i!=5) info(":");
    }
    info("\n");
}

bool E1000_card::mac_init() {
  int pos=0;
  for (int i=0; i<4; i++) {
    mac[pos++] = (uint8_t)(mmio_read32(E1000_RAL(0)) >> (8*i));
  }

  for (int i=0; i<2; i++) {
    mac[pos++] = (uint8_t)(mmio_read32(E1000_RAH(0)) >> (8*i));
  }
  return true;
}

uint16_t E1000_card::read_eeprom(uint8_t reg) {
	uint16_t data= 0;
	uint32_t tmp = 0;
  if (is_x8254) {
    mmio_write32(E1000_EERD, (1) | ((uint32_t)(reg) << 8) );
    while( !((tmp = mmio_read32(E1000_EERD)) & (1 << 4)) )
      usleep(1);
    data = (uint16_t)((tmp >> 16) & 0xFFFF);
  }
  else if (is_x8257) {
	  mmio_write32(E1000_EERD, (1) | ((uint32_t)(reg) << 2) );
    while( !((tmp = mmio_read32(E1000_EERD)) & (1 << 4)) )
      usleep(1);
    data = (uint16_t)((tmp >> 16) & 0xFFFF);
  }
  else 
    panic("card is not supported");
	return data;
}

inline uint32_t E1000_card::mmio_read32(uint32_t reg) {
  return *((volatile uint32_t *)(mmio.v()+reg)); 
}

inline void E1000_card::mmio_write32(uint32_t reg, uint32_t val) {
  *((volatile uint32_t *)(mmio.v()+reg)) = val;
  return;
}

bool E1000_card::pci_search_e1000() {
  PCI::PCI_space_device_entry * entries;
  PCI::Device_type type = PCI::Device_type_unknown;
  unsigned device_count = 0;

  int rc = env()->query_pci_config_space(PCI::PCI_ALL_DEVICES,
                                     PCI::PCI_ALL_VENDORS,
                                     PCI_CLASS_NETWORK_ETHERNET,	
                                     PCI::PCI_QUERY_FLAGS_TYPE0,
                                     &entries, 
                                     &device_count);
  assert(rc==S_OK);
  if (!device_count) 
    return false;

  PCI_config dev;
  is_x8254 = false;
  is_x8257 = false;

  unsigned i;
  for(i=0;i<device_count;i++) {
    check_ok(env()->get_pci_config(entries[i].pci_id, (void*)&dev,
                         sizeof(PCI::Config_space_type0), type));

    if ((dev.vendor!=0x8086) || ((dev.device!=0x100e) && (dev.device!=0x105e))) 
      continue;
    if (dev.device==0x100e) {
      is_x8254 = true;
    }
    else if (dev.device==0x105e) {
      is_x8257 = true;
    }
    else {
      panic("card not supported");
    }

    pciID = entries[i].pci_id;
    vendor= dev.vendor;
    device= dev.device;
    irq_pin = dev.irq_pin;

    bar_port=0;
    bar_mmio=0;
    for(unsigned b=0;b<6;b++) {
        addr_t addr = dev.bar[b];
        if (!addr) continue;
        if (addr & 1UL) {
            if(!bar_port)
	            bar_port= addr;
        }
        else {
            if(!bar_mmio)
            bar_mmio  = addr;
        }
    }
    break;
  }

  
  if (i==device_count) 
      return false;
  
  //mmio addr
  uint32_t mmio_paddr = bar_mmio & ~0xF;
  mmio.p(mmio_paddr);

  //mmio size
  uint32_t ss_key = ~0UL, ss_value;
  env()->pci_write(pciID, 0x10, &ss_key ,env()->Cfg_long);
  env()->pci_read( pciID, 0x10, &ss_value, env()->Cfg_long);
  //write back old values
  env()->pci_write ( pciID, 0x10, &bar_mmio,env()->Cfg_long);

  ss_value &= ~0xF;
  //the number of zeroes is the log2 size
  mmio_size  = ss_value & ~( ss_value - 1 );

  uint32_t mmio_vaddr;
  check_ok (env()->map_iomem((addr_t)mmio.p(), mmio_size, (void **)&mmio_vaddr, true));
  mmio.v(mmio_vaddr);

  //check if we need to use msi
  if (is_x8254) {
    uint16_t ctrl = read_eeprom(0x0F);
    if ((ctrl & (1<<7))) 
      use_msi = false;
    else
      use_msi = true;
  }
  else {
      use_msi = true;
  }

  free((void*)entries);
  return true; 
}

bool E1000_card::config_msi(int msg) {
    bool find_msi = false;
    uint8_t cap_ptr;

    // iterate caps in pci to make sure msi is usable
    env()->pci_read(pciID, 0x34, &cap_ptr,Env::Cfg_byte);
    assert((cap_ptr & 0x3)==0);
    while (cap_ptr) {
      uint8_t id;
      env()->pci_read(pciID, cap_ptr, &id,Env::Cfg_byte);
      if (id==0x05) {
        find_msi = true;
        break;
      }
      env()->pci_read(pciID, cap_ptr+1, &cap_ptr, Env::Cfg_byte);
      assert((cap_ptr & 0x3)==0);
    }

    if (!find_msi) {
        panic("msi block is not present in pci configration space!");
        return false;
    }

    uint16_t msi_ctrl;
    env()->pci_read(pciID, cap_ptr+2, &msi_ctrl, Env::Cfg_byte);
    assert(msi_ctrl & (1<<7)); //support 64-bit

    uint32_t msi_msg_addr_low = 0xfee00000;
    uint32_t msi_msg_addr_high = 0;
    uint16_t msi_msg_data;

    env()->pci_write(pciID, cap_ptr+0x4, &msi_msg_addr_low,  Env::Cfg_long);
    env()->pci_write(pciID, cap_ptr+0x8, &msi_msg_addr_high, Env::Cfg_long);

    env()->pci_read(pciID, cap_ptr+0xc ,&msi_msg_data,Env::Cfg_short);
    msi_msg_data = msi_msg_data | msg | (1<<14); //edge, assert, fixed
    env()->pci_write(pciID, cap_ptr+0xc ,&msi_msg_data,Env::Cfg_short);

    msi_ctrl |= 1;
    env()->pci_write(pciID, cap_ptr+0x2, &msi_ctrl,Env::Cfg_short);

    env()->pci_read(pciID, 0xF4,&msi_msg_addr_low,Env::Cfg_long);
    env()->pci_read(pciID, 0xF8,&msi_msg_addr_high,Env::Cfg_long);
    env()->pci_read(pciID, 0xFC,&msi_msg_data,Env::Cfg_short);
    env()->pci_read(pciID, 0xF2,&msi_ctrl,Env::Cfg_short);
#if 0
    panic("MSI: ctrl %p, addr %p, addr_high %p, data %p\n",
          (void *)msi_ctrl,
          (void *)msi_msg_addr_low,
          (void *)msi_msg_addr_high,
          (void *)msi_msg_data
       );
#endif
    return true;
}

void E1000_card::send(uint32_t pkt_phys, uint32_t length )
{
	tx_desc[tx_buf_pos]->address = (uint64_t)pkt_phys;
	tx_desc[tx_buf_pos]->length = length;
	tx_desc[tx_buf_pos]->cmd = ((1 << 3) | (3));
  
	// update the tail so the hardware knows it's ready
	int old_pos= tx_buf_pos;
	tx_buf_pos = (tx_buf_pos + 1) % NUM_TX_DESCRIPTORS;
	mmio_write32(E1000_TDT(0), tx_buf_pos);
  
	while( !(tx_desc[old_pos]->sta & 0xF) ) {
		usleep(10000);
	}

  snd_counter++;
  usleep(10000);

#if 0
	printf("A packet is sent addr %p, length %p. Transmit status = %p, h=%x, t=%x, ICR %x\n", 
      (void*)pkt_phys, (void*)length, 
    (void *) (tx_desc[old_pos]->sta & 0xF),
    mmio_read32(E1000_TDH(0)), mmio_read32(E1000_TDT(0)), mmio_read32(E1000_ICR) 
  );
#endif
}

void E1000_card::poll_interrupt() {
    volatile uint32_t icr;
    info("start polling to see if I can get some interrupts....\n");
    while (1) {
       icr = mmio_read32(E1000_ICR);
       if (icr & (1<<7)) {
            panic("pkt received");
        }
        printf("E1000_PRC64=%x\n", mmio_read32(E1000_PRC64));
	      printf("E1000_PRC127=%x\n", mmio_read32(E1000_PRC127));
	      printf("E1000_PRC255=%x\n", mmio_read32(E1000_PRC255));
	      printf("E1000_PRC511=%x\n", mmio_read32(E1000_PRC511));
	      printf("E1000_PRC1023=%x\n", mmio_read32(E1000_PRC1023));
	      printf("E1000_PRC1522=%x\n", mmio_read32(E1000_PRC1522));
    }
    info("quit polling....\n");
}

bool E1000_card::sent_to_me(uint8_t *pkt) {
    for (int i=0; i<6; i++) {
      if (mac[i]!=pkt[i])
        return false;
    }
    return true;
}

bool  E1000_card::is_arp(uint8_t * pkt) {
  bool ret =true; 
  unsigned pos;
  for (pos=0; pos<6; pos++) {
      if (pkt[pos] != 0xFF)
        return false;
  } 
  
  if (pkt[12]!=0x08 || pkt[13]!=0x06) 
        return false;

  if (ip[0]) {
    for (pos=38; pos<42; pos++) {
      if (pkt[pos] != ip[pos-38])
          return false;
    } 
  }
  return ret;
}

void E1000_card::interrupt_handler() {
       uint32_t icr = mmio_read32(E1000_ICR);
       if (icr & (1<<7)) {
            uint32_t pktlen =0;
            volatile uint32_t status;
            status = rx_desc[rx_buf_pos]->status;
            while (status & 0x01) {
              if (!(status && 0x10)) {
                panic("The packet is not complete, need to implement");
              }
              if (is_x8254 && rx_desc[rx_buf_pos]->errors) {//x8257 checks IP checksum 
                panic("found an error for x8254 when receiving a packet");
              }

              pktlen = rx_desc[rx_buf_pos]->length;
              uint8_t* pkt = (uint8_t *)(rx_pkt_buf.v()+ rx_buf_pos*PKT_MAX_SIZE);
#if 0
            printf("Notify app: a packet at %p, len=%d is received. [RX_BUF: POS=%d RDH=%d, RDT=%d]\n", 
                pkt, pktlen, rx_buf_pos, mmio_read32(E1000_RDH(0)), mmio_read32(E1000_RDT(0))); 
#endif

              if (is_arp(pkt)) {
                channel->produce(pkt,1, pktlen);
              }
              else {
                  if (sent_to_me(pkt)) {
                    channel->produce(pkt,1, pktlen);
                  }
              }
              reset_rx_desc(rx_buf_pos);
              mmio_write32(E1000_RDT(0), (rx_buf_pos)% NUM_RX_DESCRIPTORS);
              rx_buf_pos = (rx_buf_pos + 1) % NUM_RX_DESCRIPTORS;
              status = rx_desc[rx_buf_pos]->status;
            }
       }
       else if (icr& (1<<2)) {
            info("[e1000]: Network cable is %s...\n", is_up?"Disconnected":"Connected");
            is_up = !is_up;
       }
       else {
            info("[e1000]: Ignore interrupt icr= %p\n", (void *)icr);
       }
}
