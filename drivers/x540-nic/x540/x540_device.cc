/*******************************************************************************

Copyright (c) 2001-2012, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/


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
  @author Jilong Kuang (jilong.kuang@samsung.com)
*/

#include "x540_device.h"
#include "x540_types.h"
#include "x540_threads.h"
#include "x540_filter.h"
#include "prefetch.h"
#include <unistd.h>
#include <netinet/in.h>
#include <emmintrin.h>
/** 
 * Verify device supports MSI-X (sanity check)
 * 
 * 
 * @return True if device supports MSI-X
 */
bool Intel_x540_uddk_device::msix_support() {
  bool find_msi_x = false;
  uint8_t cap_ptr;

  /* iterate caps in PCI capability space to make sure msi-x is usable */
  cap_ptr = pci_config()->read8(PCI_CAP_PTR);
  assert((cap_ptr & 0x3)==0);

  while (cap_ptr) {
    uint8_t id;
    id = pci_config()->read8(cap_ptr);
    if (id==0x11) { // MSI-X capability ID 0x11
      find_msi_x = true;
      /* store msix_cap_ptr */
      _msix_cap_ptr = cap_ptr;
      break;
    }
    cap_ptr = pci_config()->read8(cap_ptr+1); // next pointer 
    assert((cap_ptr & 0x3) == 0);
  }
  return find_msi_x;
}

void Intel_x540_uddk_device::init_device() {
  while(_imem->get_comp_state(0) < MEM_READY_STATE) { sleep(1); }

  init_nic_param();

  /* dump PCI config space and PCI link speed */
  pci_config()->dump_pci_device_info();

  if (!nic_configure()) {
    panic("Failed to configure X540 card!\n");
  }

  setup_msi_interrupt();
  enable_msi();
  
  setup_rx_threads();
}

void Intel_x540_uddk_device::setup_rx_threads() {
  Irq_thread* irq_threads[NUM_RX_THREADS_PER_NIC];
  PINF("Creating RX threads ...");

  for(unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
    unsigned global_id = i + _index * NUM_RX_THREADS_PER_NIC;
    unsigned core_id = rx_core[i];
    unsigned local_id = i;
    irq_threads[i]= new Irq_thread(this, local_id, global_id, core_id, _params);
    PLOG("RX Thread[%d] is running on core %d", global_id , core_id);
    assert(irq_threads[i]);
    irq_threads[i]->activate();
  }
}

/** 
  * Allocate MSI interrupt for the device
  * 
  */
void Intel_x540_uddk_device::setup_msi_interrupt() {
  allocate_msi_vectors(msix_vector_num, _msi_vectors);

  unsigned i;
  for (i = 0; i < msix_vector_num; i++) {
    _irq[i] = _msi_vectors[i];
    route_interrupt(_msi_vectors[i],rx_core[i]);
    printf("[%u] msi %u goes to cpu %u\n",i, _msi_vectors[i],rx_core[i]);
  }
}

void Intel_x540_uddk_device::enable_msi() {
  enum { PM_CAP_OFF = 0x34, /* power management cap */
         MSI_CAP = 0x5,
         MSI_ENABLED = 0x1 };

  /* Read the power management cap
   */
  uint8_t cap = _mmio->mmio_read8(PM_CAP_OFF);

  /* Iterate through cap pointers. The next capability is bits 15:08 of PMCAP see section 2.2.1 
   */
  for (uint16_t val = 0; cap; cap = val >> 8) {

    val = _mmio->mmio_read8(cap);

    if ((val & 0xff) != MSI_CAP)
      continue;

    uint8_t msi = _mmio->mmio_read8(cap + 2);

    if (!(msi & MSI_ENABLED)) { /* bit 0 of MSIMC */
      PLOG("Enabling MSI!!!");
      _mmio->mmio_write8(cap+2,msi | MSI_ENABLED);
    }
  }
  PLOG("MSI flags OK.");
}

void Intel_x540_uddk_device::add_ip(char* myip) {
  int len = strlen(myip);
  char s[len+1];
  __builtin_memcpy(s,myip,len);
  s[len]='\0';
  parse_ip_format(s,ip);
  printf("IP: %d.%d.%d.%d is added to NIC[%u]\n",ip[0],ip[1],ip[2],ip[3],_index);
  uint32_t temp=0;
  temp=(ip[0]<<24)+(ip[1]<<16)+(ip[2]<<8)+ip[3];
  ip_addr.addr=ntohl(temp);
}

void Intel_x540_uddk_device::parse_ip_format(char *s, uint8_t * ip) {
  char* tmp = s;
  int i=0;
  while (*tmp) {
    if (*tmp == '.') {
       *tmp='\0';
        ip[i++] = atoi(s);
        tmp++;
        s = tmp;
    }
    else {
      tmp++;
    }
  }
  if (i!=3)  {
    panic("IP address is wrong. [e.g. 105.144.29.xxx]");
  }
  ip[i]=atoi(s);
}

void Intel_x540_uddk_device::activate(unsigned tid) {
  ready[tid] = true;
  sleep(1);

  /* Configure the per-queue packet buffer */
  uint32_t srrctl = IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF; 
  srrctl = srrctl | 0x2; //2K receive buffer size for packet buffer 
  _mmio->mmio_write32(IXGBE_SRRCTL(2*tid), srrctl);
  _mmio->mmio_write32(IXGBE_SRRCTL(2*tid+1), srrctl);

  // Enable receive descriptor ring.
  _mmio->mmio_write32(IXGBE_RDT(2*tid), NUM_RX_DESCRIPTORS_PER_QUEUE - 1);
  _mmio->mmio_write32(IXGBE_RDT(2*tid+1), NUM_RX_DESCRIPTORS_PER_QUEUE - 1);
}

void Intel_x540_uddk_device::wait_for_activate() {
  for (unsigned i=0; i < NUM_RX_THREADS_PER_NIC; i++) {
    while (!ready[i]) {
      usleep(10000);
    }
  }

  _mmio->mmio_read32(IXGBE_EICR);
  _mmio->mmio_write32(IXGBE_EIMS,0xffff);
  all_setup_done=true;
}

void Intel_x540_uddk_device::init_nic_param() {
  unsigned pos = 0;
  unsigned i,j;
  for (i = 0; i < 4; i++) {
    mac[pos++] = (uint8_t)(_mmio->mmio_read32(IXGBE_RAL(0)) >> (8*i));
  }

  for (i = 0; i < 2; i++) {
    mac[pos++] = (uint8_t)(_mmio->mmio_read32(IXGBE_RAH(0)) >> (8*i));
  }

  for (i = 0; i < ETHARP_HWADDR_LEN; i++)
    ethaddr.addr[i]=mac[i];

  printf("[MAC] %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  add_ip((char *)server_ip.c_str());

  flow_signature[0] = FLOW_SIGNATURE_0;
  flow_signature[1] = FLOW_SIGNATURE_1;
  flow_signature[2] = FLOW_SIGNATURE_2;
  flow_signature[3] = FLOW_SIGNATURE_3;
  flow_signature[4] = FLOW_SIGNATURE_4;

  num_q_vectors      = NUM_RX_THREADS_PER_NIC;
  rx_pb_size         = 384;
  max_tx_queues      = 128;
  max_rx_queues      = 128;
  fdir_pballoc       =   1; // Flow Director Filters Memory 64KB

  /* derive cpus for RX threads from RX_THREAD_CPU_MASK */
  //obtain NUMA cpu mask 
  struct bitmask * cpumask;
  cpumask = numa_allocate_cpumask();
  numa_node_to_cpus(_index, cpumask);
  
  for (unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
      rx_core[i] = 0;
  }

  Cpu_bitset thr_aff_per_node(rx_threads_cpu_mask);

  pos = 0;
  unsigned n = 0;
  for (unsigned i = 0; i < cpus_per_nic; i++) {
    if (thr_aff_per_node.test(pos)) {
      rx_core[n] = get_cpu_id(cpumask,i+1);
      n++;
    }
    pos++;
  }
  
#if 1
  printf("RX Thread assigned core: ");
  for (unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
    printf("%d ",rx_core[i]);
  }
  printf("\n");
#endif

}

bool Intel_x540_uddk_device::nic_configure(bool kvcache_server) {
  unsigned i;
  if (!reset_hw())
    return false;

  if (!start_hw())
    return false;

  core_configure(kvcache_server);

  up_complete();

  for (i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
    ready[i] = false;
    __builtin_memset(&int_counter[i], 0, sizeof(int_counter[i]));
  }

  for (i = 0; i < NUM_RX_QUEUES; i++) {
    __builtin_memset(&recv_counter[i], 0, sizeof(recv_counter[i]));
  }

  int_total_counter=0;
  all_setup_done = false;
  rx_start_ok = false;
  is_up = false;

  return true;
}

bool Intel_x540_uddk_device::reset_hw() {
  uint32_t ctrl, i;

  /* Call adapter stop to disable tx/rx and clear interrupts */
  stop_adapter();

  /* flush pending Tx transactions */
  clear_tx_pending();

  /* mac_reset_top starts */
  ctrl = IXGBE_CTRL_RST;
  ctrl |= _mmio->mmio_read32(IXGBE_CTRL);
  _mmio->mmio_write32(IXGBE_CTRL, ctrl);
  IXGBE_WRITE_FLUSH();

  /* Poll for reset bit to self-clear indicating reset is complete */
  for (i = 0; i < 10; i++) {
    usleep(1000);
    ctrl = _mmio->mmio_read32(IXGBE_CTRL);
    if (!(ctrl & IXGBE_CTRL_RST_MASK))
      break;
  }

  if (ctrl & IXGBE_CTRL_RST_MASK) {
    printf("Reset polling failed to complete.\n");
    return false;
  }
  sleep(1);

  /* Set the Rx packet buffer size. */
  _mmio->mmio_write32(IXGBE_RXPBSIZE(0), 384 << IXGBE_RXPBSIZE_SHIFT);

  return true;
}

void Intel_x540_uddk_device::stop_adapter() {
  unsigned i;
  uint32_t reg_val;
  /* Disable the receive unit */
  _mmio->mmio_write32(IXGBE_RXCTRL,0);

  /* Clear interrupt mask to stop interrupts from being generated */
  _mmio->mmio_write32(IXGBE_EIMC, IXGBE_IRQ_CLEAR_MASK);
  _mmio->mmio_write32(IXGBE_EIMC_EX(0), IXGBE_IRQ_CLEAR_MASK);
  _mmio->mmio_write32(IXGBE_EIMC_EX(1), IXGBE_IRQ_CLEAR_MASK);

  /* Clear any pending interrupts, flush previous writes */
  _mmio->mmio_read32(IXGBE_EICR);

  /* Disable the transmit unit.  Each queue must be disabled. */
  for (i = 0; i < max_tx_queues; i++)
    _mmio->mmio_write32(IXGBE_TXDCTL(i), IXGBE_TXDCTL_SWFLSH);

  /* Disable the receive unit by stopping each queue */
  for (i = 0; i < max_rx_queues; i++) {
    reg_val = _mmio->mmio_read32(IXGBE_RXDCTL(i));
    reg_val &= ~IXGBE_RXDCTL_ENABLE;
    reg_val |= IXGBE_RXDCTL_SWFLSH;
    _mmio->mmio_write32(IXGBE_RXDCTL(i), reg_val);
  }

  /* flush all queues disables */
  IXGBE_WRITE_FLUSH();
  usleep(2000);
}

void Intel_x540_uddk_device::clear_tx_pending() {
  uint32_t gcr_ext, hlreg0;

  /*
   * Set loopback enable to prevent any transmits from being sent
   * should the link come up.  This assumes that the RXCTRL.RXEN bit
   * has already been cleared.
   */
  hlreg0 = _mmio->mmio_read32(IXGBE_HLREG0);

  _mmio->mmio_write32(IXGBE_HLREG0, hlreg0 | IXGBE_HLREG0_LPBK);

  /* initiate cleaning flow for buffers in the PCIe transaction layer */
  gcr_ext = _mmio->mmio_read32(IXGBE_GCR_EXT);
  _mmio->mmio_write32(IXGBE_GCR_EXT, gcr_ext | IXGBE_GCR_EXT_BUFFERS_CLEAR);

  /* Flush all writes and allow 20usec for all transactions to clear */
  IXGBE_WRITE_FLUSH();
  usleep(1000);

  /* restore previous register values */
  _mmio->mmio_write32(IXGBE_GCR_EXT, gcr_ext);
  _mmio->mmio_write32(IXGBE_HLREG0, hlreg0);
}

bool Intel_x540_uddk_device::start_hw() {
  clear_hw_counters();

  /* Set No Snoop Disable */
  uint32_t ctrl_ext = _mmio->mmio_read32(IXGBE_CTRL_EXT);
  ctrl_ext |= IXGBE_CTRL_EXT_NS_DIS;
  _mmio->mmio_write32(IXGBE_CTRL_EXT, ctrl_ext);
  IXGBE_WRITE_FLUSH();

  return true;
}

void Intel_x540_uddk_device::clear_hw_counters() {
  uint16_t i;

  _mmio->mmio_read32(IXGBE_CRCERRS);
  _mmio->mmio_read32(IXGBE_ILLERRC);
  _mmio->mmio_read32(IXGBE_ERRBC);
  _mmio->mmio_read32(IXGBE_MSPDC);

  for (i = 0; i < 8; i++)
    _mmio->mmio_read32(IXGBE_MPC(i));

  _mmio->mmio_read32(IXGBE_MLFC);
  _mmio->mmio_read32(IXGBE_MRFC);
  _mmio->mmio_read32(IXGBE_RLEC);
  _mmio->mmio_read32(IXGBE_LXONTXC);
  _mmio->mmio_read32(IXGBE_LXOFFTXC);
  _mmio->mmio_read32(IXGBE_LXONRXCNT);
  _mmio->mmio_read32(IXGBE_LXOFFRXCNT);

  for (i = 0; i < 8; i++) {
    _mmio->mmio_read32(IXGBE_PXONTXC(i));
    _mmio->mmio_read32(IXGBE_PXOFFTXC(i));
    _mmio->mmio_read32(IXGBE_PXONRXCNT(i));
    _mmio->mmio_read32(IXGBE_PXOFFRXCNT(i));
    _mmio->mmio_read32(IXGBE_PXON2OFFCNT(i));
  }

  _mmio->mmio_read32(IXGBE_PRC64);
  _mmio->mmio_read32(IXGBE_PRC127);
  _mmio->mmio_read32(IXGBE_PRC255);
  _mmio->mmio_read32(IXGBE_PRC511);
  _mmio->mmio_read32(IXGBE_PRC1023);
  _mmio->mmio_read32(IXGBE_PRC1522);
  _mmio->mmio_read32(IXGBE_GPRC);
  _mmio->mmio_read32(IXGBE_BPRC);
  _mmio->mmio_read32(IXGBE_MPRC);
  _mmio->mmio_read32(IXGBE_GPTC);
  _mmio->mmio_read32(IXGBE_GORCL);
  _mmio->mmio_read32(IXGBE_GORCH);
  _mmio->mmio_read32(IXGBE_GOTCL);
  _mmio->mmio_read32(IXGBE_GOTCH);
  _mmio->mmio_read32(IXGBE_RUC);
  _mmio->mmio_read32(IXGBE_RFC);
  _mmio->mmio_read32(IXGBE_ROC);
  _mmio->mmio_read32(IXGBE_RJC);
  _mmio->mmio_read32(IXGBE_MNGPRC);
  _mmio->mmio_read32(IXGBE_MNGPDC);
  _mmio->mmio_read32(IXGBE_MNGPTC);
  _mmio->mmio_read32(IXGBE_TORL);
  _mmio->mmio_read32(IXGBE_TORH);
  _mmio->mmio_read32(IXGBE_TPR);
  _mmio->mmio_read32(IXGBE_TPT);
  _mmio->mmio_read32(IXGBE_PTC64);
  _mmio->mmio_read32(IXGBE_PTC127);
  _mmio->mmio_read32(IXGBE_PTC255);
  _mmio->mmio_read32(IXGBE_PTC511);
  _mmio->mmio_read32(IXGBE_PTC1023);
  _mmio->mmio_read32(IXGBE_PTC1522);
  _mmio->mmio_read32(IXGBE_MPTC);
  _mmio->mmio_read32(IXGBE_BPTC);

  for (i = 0; i < 16; i++) {
    _mmio->mmio_read32(IXGBE_QPRC(i));
    _mmio->mmio_read32(IXGBE_QPTC(i));
    _mmio->mmio_read32(IXGBE_QBRC_L(i));
    _mmio->mmio_read32(IXGBE_QBRC_H(i));
    _mmio->mmio_read32(IXGBE_QBTC_L(i));
    _mmio->mmio_read32(IXGBE_QBTC_H(i));
    _mmio->mmio_read32(IXGBE_QPRDC(i));
  }
}

void Intel_x540_uddk_device::filter_setup(bool kvcache_server){
  Nic_filter nic_filter(this);
  /* setup a 5-tuple flow filter (sip, dip, sp, dp, proto)=(105.144.29.103, 105.144.29.109, 0x5678, 0x6789, UDP)*/
  // WARNING: big-endian format in sip, dip, sp and dp
#if 0
  nic_filter.setup_flow_filter(0,FLOW_FILTER_QUEUE_1,CLIENT_IP_32,SERVER_IP_32,htons(CLIENT_PORT),htons(SERVER_PORT),1);
#endif

  /* setup a flow-director filter */
  nic_filter.init_fdir_perfect(fdir_pballoc, flex_byte_pos, kvcache_server);

  for (unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i ++) {
    unsigned queue = 2*i;
    nic_filter.setup_fdir_filter(flow_signature[i], queue, kvcache_server);
  }
}

void Intel_x540_uddk_device::core_configure(bool kvcache_server) {
  configure_pb();

  set_rx_mode();

  disable_sec_rx_path();

  filter_setup(kvcache_server);

  enable_sec_rx_path();

  configure_tx();
  PLOG("[NIC %d] Configured TX. OK", _index);

  _inic->set_comp_state(NIC_TX_DONE_STATE, _index);

  while (_istack->get_comp_state(_index) < STACK_READY_STATE) { sleep(1); }

  configure_rx();
  PLOG("[NIC %d] Configured RX. OK", _index);
  
  _inic->set_comp_state(NIC_RX_DONE_STATE, _index);
}

void Intel_x540_uddk_device::configure_msix() {
  /*
   * Populate the IVAR table and set the ITR values to the
   * corresponding register.
   */
  unsigned v_idx;
  u32 mask;
  for (v_idx = 0; v_idx < num_q_vectors; v_idx++) {
    for (unsigned i=2*v_idx;i<2*v_idx+2;i++) {
      set_ivar(0, i, v_idx);
      set_ivar(1, i, v_idx);
    }
  }
  set_ivar(-1, 1, v_idx);
  _mmio->mmio_write32(IXGBE_EITR(v_idx), 1950);

  mask=0xffff; // Auto-clear of EICR enabled
  _mmio->mmio_write32(IXGBE_EIAC, mask);

}

void Intel_x540_uddk_device::set_ivar(s8 direction, u8 queue, u8 msix_vector) {
  u32 ivar, index;
  if (direction == -1) {
  /* other causes */
    msix_vector |= IXGBE_IVAR_ALLOC_VAL;
    index = ((queue & 1) * 8);
    ivar = _mmio->mmio_read32(IXGBE_IVAR_MISC);
    ivar &= ~(0xFF << index);
    ivar |= (msix_vector << index);
    _mmio->mmio_write32(IXGBE_IVAR_MISC, ivar);
  }
  else {
    /* tx or rx causes */
    msix_vector |= IXGBE_IVAR_ALLOC_VAL;
    index = ((16 * (queue & 1)) + (8 * direction));
    ivar = _mmio->mmio_read32(IXGBE_IVAR(queue >> 1));
    ivar &= ~(0xFF << index);
    ivar |= (msix_vector << index);
    _mmio->mmio_write32(IXGBE_IVAR(queue >> 1), ivar);
  }
}


void Intel_x540_uddk_device::configure_pb() {
  unsigned hdrm;
  hdrm = 32 << fdir_pballoc;
  setup_rxpba(1,hdrm,PBA_STRATEGY_EQUAL);

}

void Intel_x540_uddk_device::setup_rxpba(unsigned num_pb, uint32_t headroom, int strategy) {
  uint32_t pbsize = rx_pb_size;
  unsigned i = 0;
  uint32_t rxpktsize, txpktsize, txpbthresh;

  /* Reserve headroom */
  pbsize -= headroom;

  /* Divide remaining packet buffer space amongst the number of packet
   * buffers requested using supplied strategy.
   */
  switch (strategy) {
    /* Fall through to configure remaining packet buffers */
    case PBA_STRATEGY_EQUAL:
      rxpktsize = (pbsize / (num_pb - i)) << IXGBE_RXPBSIZE_SHIFT;
      for (; i < num_pb; i++)
        _mmio->mmio_write32(IXGBE_RXPBSIZE(i), rxpktsize);
      break;
    default:
      break;
  }

  /* Only support an equally distributed Tx packet buffer strategy. */
  txpktsize = IXGBE_TXPBSIZE_MAX / num_pb;
  txpbthresh = (txpktsize / 1024) - IXGBE_TXPKT_SIZE_MAX;
  for (i = 0; i < num_pb; i++) {
    _mmio->mmio_write32(IXGBE_TXPBSIZE(i), txpktsize);
    _mmio->mmio_write32(IXGBE_TXPBTHRESH(i), txpbthresh);
  }

  /* Clear unused TCs, if any, to zero buffer size*/
  for (; i < IXGBE_MAX_PB; i++) {
    _mmio->mmio_write32(IXGBE_RXPBSIZE(i), 0);
    _mmio->mmio_write32(IXGBE_TXPBSIZE(i), 0);
    _mmio->mmio_write32(IXGBE_TXPBTHRESH(i), 0);
  }
}


void Intel_x540_uddk_device::set_rx_mode() {
  uint32_t fctrl;
  fctrl = _mmio->mmio_read32(IXGBE_FCTRL);
  /* set all bits that we expect to always be set */
  fctrl |= IXGBE_FCTRL_BAM;
  fctrl |= IXGBE_FCTRL_DPF; /* discard pause frames when FC enabled */
  fctrl |= IXGBE_FCTRL_PMCF;
  /* clear the bits we are changing the status of */
  fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
  //fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
  
  _mmio->mmio_write32(IXGBE_FCTRL, fctrl);

}

void Intel_x540_uddk_device::disable_sec_rx_path() {
  unsigned i;
  int secrxreg;
  secrxreg = _mmio->mmio_read32(IXGBE_SECRXCTRL);
  secrxreg |= IXGBE_SECRXCTRL_RX_DIS;
  _mmio->mmio_write32(IXGBE_SECRXCTRL, secrxreg);
  for (i = 0; i < IXGBE_MAX_SECRX_POLL; i++) {
    secrxreg = _mmio->mmio_read32(IXGBE_SECRXSTAT);
    if (secrxreg & IXGBE_SECRXSTAT_SECRX_RDY)
      break;
    else
      usleep(1000);
  }
  if (i >= IXGBE_MAX_SECRX_POLL)
    printf("Rx unit being enabled before security "
                         "path fully disabled.  Continuing with init.\n");
}

void Intel_x540_uddk_device::enable_sec_rx_path() {
  int secrxreg;
  secrxreg = _mmio->mmio_read32(IXGBE_SECRXCTRL);
  secrxreg &= ~IXGBE_SECRXCTRL_RX_DIS;
  _mmio->mmio_write32(IXGBE_SECRXCTRL, secrxreg);
  IXGBE_WRITE_FLUSH();

}


void Intel_x540_uddk_device::setup_mrqc() {
  static const uint32_t seed[10] = { 0xE291D73D, 0x1805EC6C, 0x2A94B30D,
                          0xA54F2BEC, 0xEA49AF7C, 0xE214AD3D, 0xB855AABE,
                          0x6A3E67EA, 0x14364D17, 0x3BED200D};
  uint32_t mrqc = 0;
  uint32_t rxcsum;
  unsigned i;

  /* Fill out hash function seeds */
  for (i = 0; i < 10; i++)
    _mmio->mmio_write32(IXGBE_RSSRK(i), seed[i]); 

  /* Disable indicating checksum in descriptor, enables RSS hash */
  rxcsum = _mmio->mmio_read32(IXGBE_RXCSUM);
  rxcsum |= IXGBE_RXCSUM_PCSD;
  rxcsum |= IXGBE_RXCSUM_IPPCSE;
  _mmio->mmio_write32(IXGBE_RXCSUM, rxcsum);

  mrqc = IXGBE_MRQC_RSSEN;

  /* Perform hash on these packet types */
  mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4 |
                IXGBE_MRQC_RSS_FIELD_IPV4_TCP |
                IXGBE_MRQC_RSS_FIELD_IPV6 |
                IXGBE_MRQC_RSS_FIELD_IPV6_TCP |
                IXGBE_MRQC_RSS_FIELD_IPV4_UDP |
                IXGBE_MRQC_RSS_FIELD_IPV6_UDP;

  _mmio->mmio_write32(IXGBE_MRQC, mrqc);

}

void Intel_x540_uddk_device::configure_rx_ring(struct exo_rx_ring * ring) {
  u64 rdba = ring->dma;
  u32 rxdctl;
  u8 reg_idx = ring->reg_idx;

  /* disable queue to avoid issues while updating state */
  rxdctl = _mmio->mmio_read32(IXGBE_RXDCTL(reg_idx));
  disable_rx_queue(ring);

  _mmio->mmio_write32(IXGBE_RDBAL(reg_idx), rdba & DMA_BIT_MASK(32));
  _mmio->mmio_write32(IXGBE_RDBAH(reg_idx), rdba >> 32);
  _mmio->mmio_write32(IXGBE_RDLEN(reg_idx), ring->count * sizeof(RX_ADV_DATA_DESC));

  /* reset head and tail pointers */
  _mmio->mmio_write32(IXGBE_RDH(reg_idx), 0);
  _mmio->mmio_write32(IXGBE_RDT(reg_idx), 0);

  /* reset ntu and ntc to place SW in sync with hardwdare */
  rxdctl &= ~(IXGBE_RXDCTL_RLPMLMASK | IXGBE_RXDCTL_RLPML_EN);

  /* enable receive descriptor ring */
  rxdctl |= IXGBE_RXDCTL_ENABLE;
  _mmio->mmio_write32(IXGBE_RXDCTL(reg_idx), rxdctl);
  rx_desc_queue_enable(ring);

}

void Intel_x540_uddk_device::disable_rx_queue(struct exo_rx_ring *ring) {
  int wait_loop = IXGBE_MAX_RX_DESC_POLL;
  u32 rxdctl;
  u8 reg_idx = ring->reg_idx;

  rxdctl = _mmio->mmio_read32(IXGBE_RXDCTL(reg_idx));
  rxdctl &= ~IXGBE_RXDCTL_ENABLE;

  /* write value back with RXDCTL.ENABLE bit cleared */
  _mmio->mmio_write32(IXGBE_RXDCTL(reg_idx), rxdctl);

  /* the hardware may take up to 100us to really disable the rx queue */
  do {
    usleep(1000);
    rxdctl = _mmio->mmio_read32(IXGBE_RXDCTL(reg_idx));
  } while (--wait_loop && (rxdctl & IXGBE_RXDCTL_ENABLE));

  if (!wait_loop) {
    printf("RXDCTL.ENABLE on Rx queue %d not cleared within the polling period\n", reg_idx);
  }
}

void Intel_x540_uddk_device::rx_desc_queue_enable(struct exo_rx_ring *ring) {
  int wait_loop = IXGBE_MAX_RX_DESC_POLL;
  u32 rxdctl;
  u8 reg_idx = ring->reg_idx;

  do {
    rxdctl = _mmio->mmio_read32(IXGBE_RXDCTL(reg_idx));
  } while (--wait_loop && !(rxdctl & IXGBE_RXDCTL_ENABLE));

  if (!wait_loop) {
    panic("RXDCTL.ENABLE on Rx queue %d not set within the polling period\n", reg_idx);
  }
}

void Intel_x540_uddk_device::configure_rx() {
  uint32_t rxctrl;

  /* disable receives while setting up the descriptors */
  rxctrl = _mmio->mmio_read32(IXGBE_RXCTRL);
  _mmio->mmio_write32(IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

  uint32_t rdrxctl = _mmio->mmio_read32(IXGBE_RDRXCTL);

  /* Disable RSC for ACK packets */
  _mmio->mmio_write32(IXGBE_RSCDBU,(IXGBE_RSCDBU_RSCACKDIS | _mmio->mmio_read32(IXGBE_RSCDBU)));
  rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;

  /* hardware requires some bits to be set by default */
  rdrxctl |= (IXGBE_RDRXCTL_RSCACKC | IXGBE_RDRXCTL_FCOE_WRFIX);
  rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
  _mmio->mmio_write32(IXGBE_RDRXCTL, rdrxctl);

  /* Program registers for the distribution of queues */
  setup_mrqc();

  /* Allocating RX desc buffer and packet buffer */
  addr_t desc_v,desc_p;
  int bytes_to_alloc = NUM_RX_QUEUES * NUM_RX_DESCRIPTORS_PER_QUEUE * sizeof(RX_ADV_DATA_DESC);

  /* create memory space for RX descriptor ring */
  void * temp;
  if (_imem->alloc((addr_t *)&temp, DESC_ALLOCATOR, _index, rx_core[0]) != Exokernel::S_OK) {
    panic("DESC_ALLOCATOR failed!\n");
  }
  assert(temp);

  desc_v=(addr_t)temp;
  desc_p=(addr_t)_imem->get_phys_addr(temp, DESC_ALLOCATOR, _index);
  assert(desc_p);

  __builtin_memset((void*)desc_v, 0, bytes_to_alloc);
  assert(desc_p % 16 == 0);

  addr_t pkt_v,pkt_p;
  /*
   * Setup the HW Rx Head and Tail Descriptor Pointers and
   * the Base and Length of the Rx Descriptor Ring
   */
  for (unsigned i = 0; i < NUM_RX_QUEUES; i++) {
    __builtin_memset(&rx_ring[i], 0, sizeof(struct exo_rx_ring));
    rx_ring[i].reg_idx=i;
    rx_ring[i].count=NUM_RX_DESCRIPTORS_PER_QUEUE;
    rx_ring[i].dma=desc_p+i*(NUM_RX_DESCRIPTORS_PER_QUEUE * sizeof(RX_ADV_DATA_DESC));
    rx_ring[i].desc=(void *) (desc_v+i*(NUM_RX_DESCRIPTORS_PER_QUEUE * sizeof(RX_ADV_DATA_DESC)));

    for(unsigned j = 0; j < NUM_RX_DESCRIPTORS_PER_QUEUE; j++ ) {
      if (_imem->alloc((addr_t *)&temp, PACKET_ALLOCATOR, _index, rx_core[i/2]) != Exokernel::S_OK) {
        panic("PACKET_ALLOCATOR failed!\n");
      }
      assert(temp);

      pkt_v=(addr_t)temp;
      pkt_p=(addr_t)_imem->get_phys_addr(temp, PACKET_ALLOCATOR, _index);
      __builtin_memset((void *)pkt_v, 0, PKT_MAX_SIZE);

      rx_ring[i].rx_buf[j].phys_addr = pkt_p;
      rx_ring[i].rx_buf[j].virt_addr = pkt_v;
      
      rx_desc[i][j] = (volatile RX_ADV_DATA_DESC*)((addr_t)rx_ring[i].desc + (j * sizeof(RX_ADV_DATA_DESC)));
      rx_desc[i][j]->read.hdr_addr   = (addr_t)pkt_p;
      rx_desc[i][j]->read.pkt_addr   = (addr_t)pkt_p;
    }

    PLOG("RX Queue[%d] First Desc Address (v=%p, p=%p) First Packet Address (v=%p, p=%p)",
         i,(void *)rx_ring[i].desc, (void *)rx_ring[i].dma, 
         (void *)(rx_ring[i].rx_buf[0].virt_addr),(void *)(rx_ring[i].rx_buf[0].phys_addr));

    configure_rx_ring(&rx_ring[i]);
  }

  IXGBE_WRITE_FLUSH();

  /* enable all receives */
  rxctrl |= IXGBE_RXCTRL_RXEN;
  _mmio->mmio_write32(IXGBE_RXCTRL, rxctrl);

  IXGBE_WRITE_FLUSH();

}

void Intel_x540_uddk_device::configure_tx() {
  uint32_t dmatxctl;
  uint32_t i;
  setup_mtqc();
  dmatxctl = _mmio->mmio_read32(IXGBE_DMATXCTL);
  dmatxctl |= IXGBE_DMATXCTL_TE;
  _mmio->mmio_write32(IXGBE_DMATXCTL, dmatxctl);

  addr_t desc_v, desc_p;
  int bytes_to_alloc = NUM_TX_QUEUES * NUM_TX_DESCRIPTORS_PER_QUEUE * sizeof(TX_ADV_DATA_DESC);

  /* create memory space for TX descriptor ring */
  void * temp;
  if (_imem->alloc((addr_t *)&temp, DESC_ALLOCATOR, _index, rx_core[0]) != Exokernel::S_OK) {
    panic("DESC_ALLOCATOR failed!\n");
  }
  assert(temp);

  desc_v=(addr_t)temp;
  desc_p=(addr_t)_imem->get_phys_addr(temp, DESC_ALLOCATOR, _index);

  __builtin_memset((void*)desc_v, 0, bytes_to_alloc);
  assert(desc_p % 16 == 0);

  /* Setup the HW Tx Head and Tail descriptor pointers */
  for (i = 0; i < NUM_TX_QUEUES; i++) {
    __builtin_memset(&tx_ring[i], 0, sizeof(struct exo_tx_ring));
    tx_ring[i].reg_idx=i;
    tx_ring[i].dma=desc_p+i*(NUM_TX_DESCRIPTORS_PER_QUEUE * sizeof(TX_ADV_DATA_DESC));
    tx_ring[i].desc=(void *) (desc_v+i*(NUM_TX_DESCRIPTORS_PER_QUEUE * sizeof(TX_ADV_DATA_DESC)));
    PLOG("TX Queue[%d] First Desc Address (v=%p, p=%p)\n",i, (void *)tx_ring[i].desc, (void *)tx_ring[i].dma);

    tx_ring[i].tx_free_thresh = TX_FREE_THRESH;
    tx_ring[i].tx_rs_thresh = TX_RS_THRESH;
    tx_ring[i].nb_tx_desc = NUM_TX_DESCRIPTORS_PER_QUEUE;

    /* Initialize SW ring entries */
    uint16_t prev, ii;
    prev = (uint16_t) (tx_ring[i].nb_tx_desc - 1);
    struct tx_entry *txe = tx_ring[i].sw_ring;
    for (ii = 0; ii < tx_ring[i].nb_tx_desc; ii++) {
      txe[ii].last_id = ii;
      txe[prev].next_id = ii;
      prev = ii;
    }

    tx_ring[i].tx_next_dd = (uint16_t)(tx_ring[i].tx_rs_thresh - 1);
    tx_ring[i].tx_next_rs = (uint16_t)(tx_ring[i].tx_rs_thresh - 1);
    tx_ring[i].tx_tail = 0;
    tx_ring[i].nb_tx_used = 0;
    /*
     * Always allow 1 descriptor to be un-allocated to avoid
     * a H/W race condition
     */
    tx_ring[i].nb_tx_free = (uint16_t)(tx_ring[i].nb_tx_desc - 1);
    tx_ring[i].last_desc_cleaned = (uint16_t)(tx_ring[i].nb_tx_desc - 1);   

    for(unsigned j = 0; j < NUM_TX_DESCRIPTORS_PER_QUEUE; j++ ) {
      tx_desc[i][j] = (TX_ADV_DATA_DESC*)((addr_t)tx_ring[i].desc + (j * sizeof(TX_ADV_DATA_DESC)));
    }

    configure_tx_ring(&tx_ring[i]);
  }
}

void Intel_x540_uddk_device::setup_mtqc() {
  uint32_t rttdcs, mtqc;

  /* disable the arbiter while setting MTQC */
  rttdcs = _mmio->mmio_read32(IXGBE_RTTDCS);
  rttdcs |= IXGBE_RTTDCS_ARBDIS;
  _mmio->mmio_write32(IXGBE_RTTDCS, rttdcs);

  mtqc = IXGBE_MTQC_64Q_1PB; /* 64 queue, 1 packet buffer mode */
  _mmio->mmio_write32(IXGBE_MTQC, mtqc);

  /* re-enable the arbiter */
  rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
  _mmio->mmio_write32(IXGBE_RTTDCS, rttdcs);
}

void Intel_x540_uddk_device::configure_tx_ring(struct exo_tx_ring *ring) {
  uint64_t tdba = ring->dma;
  int wait_loop = 10;
  uint32_t txdctl = IXGBE_TXDCTL_ENABLE;
  uint8_t reg_idx = ring->reg_idx + TX_QUEUE_SHIFT;

  /* disable queue to avoid issues while updating state */
  _mmio->mmio_write32(IXGBE_TXDCTL(reg_idx), IXGBE_TXDCTL_SWFLSH);

  IXGBE_WRITE_FLUSH();

  _mmio->mmio_write32(IXGBE_TDBAL(reg_idx), tdba & DMA_BIT_MASK(32));
  _mmio->mmio_write32(IXGBE_TDBAH(reg_idx), tdba >> 32);
  _mmio->mmio_write32(IXGBE_TDLEN(reg_idx),
                        ring->nb_tx_desc * sizeof(TX_ADV_DATA_DESC));

  /* disable head writeback */
  _mmio->mmio_write32(IXGBE_TDWBAH(reg_idx), 0);
  _mmio->mmio_write32(IXGBE_TDWBAL(reg_idx), 0);

  /* reset head and tail pointers */
  _mmio->mmio_write32(IXGBE_TDH(reg_idx), 0);
  _mmio->mmio_write32(IXGBE_TDT(reg_idx), 0);

  txdctl |= (TX_QUEUE_WTHRESH << 16);    /* WTHRESH */

  txdctl |= (TX_QUEUE_HTHRESH << 8) |    /* HTHRESH */
             TX_QUEUE_PTHRESH;           /* PTHRESH */

  /* enable queue */
  _mmio->mmio_write32(IXGBE_TXDCTL(reg_idx), txdctl);

  /* poll to verify queue is enabled */
  do {
    usleep(1000);
    txdctl = _mmio->mmio_read32(IXGBE_TXDCTL(reg_idx));
  } while (--wait_loop && !(txdctl & IXGBE_TXDCTL_ENABLE));
  if (!wait_loop) {
    printf("Could not enable Tx Queue %d\n", reg_idx);
    assert(0);
  }

}

inline unsigned Intel_x540_uddk_device::rx_queue_2_core(unsigned queue) {
  unsigned tid = queue >> 1;
  return (rx_core[tid]);
}

void Intel_x540_uddk_device::non_sfp_link_config() {
  int negotiate = 0;
  u32 autoneg;
  bool link_up = false;
  check_mac_link(&autoneg, &link_up, false);
  if (autoneg==IXGBE_LINK_SPEED_1GB_FULL)
    printf("link speed = 1GB - link_up=%x\n",link_up);
  else if (autoneg==IXGBE_LINK_SPEED_100_FULL)
    printf("link speed = 100MB - link_up=%x\n",link_up);
  else if (autoneg==IXGBE_LINK_SPEED_10GB_FULL)
    printf("link speed = 10GB - link_up=%x\n",link_up);
  else
    printf("link speed = UNKNOWN - link_up=%x\n",link_up);
}

void Intel_x540_uddk_device::irq_enable(bool queues, bool flush) {
  u32 mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);
  mask |= IXGBE_EIMS_ECC;
  mask |= IXGBE_EIMS_MAILBOX;
  mask |= IXGBE_EIMS_FLOW_DIR;
  _mmio->mmio_write32(IXGBE_EIMS, mask);

  if (queues)
    //irq_enable_queues(~0);
    irq_enable_queues(0xffff);
  if (flush)
    IXGBE_WRITE_FLUSH();

}

void Intel_x540_uddk_device::irq_enable_queues(u64 qmask) {
  u32 mask;
  mask = (qmask & 0xFFFFFFFF);
  if (mask)
    _mmio->mmio_write32(IXGBE_EIMS_EX(0), mask);
  mask = (qmask >> 32);
  if (mask)
    _mmio->mmio_write32(IXGBE_EIMS_EX(1), mask);
}

void Intel_x540_uddk_device::up_complete() {
  u32 ctrl_ext;
  /* Let firmware know the driver has taken over */
  ctrl_ext = _mmio->mmio_read32(IXGBE_CTRL_EXT);
  _mmio->mmio_write32(IXGBE_CTRL_EXT, ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);

  /* General Purpose Interrupt Enable */
  u32 gpie = IXGBE_GPIE_MSIX_MODE | IXGBE_GPIE_PBA_SUPPORT;

  /* Set auto-clear on */
  gpie |= IXGBE_GPIE_EIAME;
  _mmio->mmio_write32(IXGBE_EIAM_EX(0), 0xFFFFFFFF);
  _mmio->mmio_write32(IXGBE_EIAM_EX(1), 0xFFFFFFFF);
  _mmio->mmio_write32(IXGBE_GPIE, gpie);

  configure_msix();

  non_sfp_link_config();
  
  /* clear any pending interrupts, may auto mask */
  _mmio->mmio_read32(IXGBE_EICR);
  irq_enable(true, true);

  /* Set PF Reset Done bit so PF/VF Mail Ops can work */
  ctrl_ext = _mmio->mmio_read32(IXGBE_CTRL_EXT);
  ctrl_ext |= IXGBE_CTRL_EXT_PFRSTD;
  _mmio->mmio_write32(IXGBE_CTRL_EXT, ctrl_ext);

}

void Intel_x540_uddk_device::check_mac_link(ixgbe_link_speed* speed, 
                                            bool* link_up, 
                                            bool link_up_wait_to_complete) {
  u32 links_reg, links_orig;
  u32 i;

  /* clear the old state */
  links_orig = _mmio->mmio_read32(IXGBE_LINKS);
  links_reg = _mmio->mmio_read32(IXGBE_LINKS);

  if (links_orig != links_reg) {
    printf("LINKS changed from %08X to %08X\n",
                          links_orig, links_reg);
  }

  if (link_up_wait_to_complete) {
    for (i = 0; i < IXGBE_LINK_UP_TIME; i++) {
      if (links_reg & IXGBE_LINKS_UP) {
        *link_up = true;
        break;
      } else {
        *link_up = false;
      }
      sleep(1);
      links_reg = _mmio->mmio_read32(IXGBE_LINKS);
    }
  }
  else {
    if (links_reg & IXGBE_LINKS_UP)
      *link_up = true;
    else
      *link_up = false;
  }

  if ((links_reg & IXGBE_LINKS_SPEED_82599) == IXGBE_LINKS_SPEED_10G_82599)
    *speed = IXGBE_LINK_SPEED_10GB_FULL;
  else if ((links_reg & IXGBE_LINKS_SPEED_82599) == IXGBE_LINKS_SPEED_1G_82599)
    *speed = IXGBE_LINK_SPEED_1GB_FULL;
  else if ((links_reg & IXGBE_LINKS_SPEED_82599) == IXGBE_LINKS_SPEED_100_82599)
    *speed = IXGBE_LINK_SPEED_100_FULL;
  else
    *speed = IXGBE_LINK_SPEED_UNKNOWN;

}

void Intel_x540_uddk_device::reset_rx_desc(unsigned queue, unsigned index, bool new_pkt) {
  if (new_pkt) {
    unsigned tid = queue >> 1;
    unsigned core = rx_core[tid];
    /* create a new packet buffer */
    addr_t pkt_v, pkt_p;
    void * temp;
    if (_imem->alloc((addr_t *)&temp, PACKET_ALLOCATOR, _index, core) != Exokernel::S_OK) {
      panic("PACKET_ALLOCATOR failed!\n");
    }
    assert(temp);
//    printf("available frame numbers: %ld\n", _imem->get_num_available_blocks(PACKET_ALLOCATOR));
    pkt_v=(addr_t)temp;
    pkt_p=(addr_t)_imem->get_phys_addr(temp, PACKET_ALLOCATOR, _index);
    assert(pkt_p);
    
    rx_ring[queue].rx_buf[index].phys_addr = pkt_p;
    rx_ring[queue].rx_buf[index].virt_addr = pkt_v;
  }

  addr_t dma_addr = rx_ring[queue].rx_buf[index].phys_addr;
  rx_desc[queue][index]->read.hdr_addr = dma_addr;
  rx_desc[queue][index]->read.pkt_addr = dma_addr;
}

void Intel_x540_uddk_device::interrupt_handler(unsigned tid) {
  uint32_t pktlen;
  uint32_t status;
  unsigned rxdp;
  unsigned queue = tid * 2;
  unsigned core = rx_core[tid];
  uint8_t* pkt;
  assert(tid>=0);
  assert(tid<NUM_RX_THREADS_PER_NIC);
  int_counter[tid].ctr++;
  int_total_counter++;
  rxdp = rx_ring[queue].rx_tail;

  status = rx_desc[queue][rxdp]->wb.upper.status_error;

  //_mmio->mmio_read32(IXGBE_EICR);
  
  int s[LOOK_AHEAD], nb_dd;
  int i,j;
  uint64_t count=0;

#ifdef POLLING_MODE_ENABLED
  while (1) {
#else
  while (status & IXGBE_RXDADV_STAT_DD) {
#endif

#ifdef POLLING_MODE_ENABLED
    rxdp = rx_ring[queue].rx_tail;
    status = rx_desc[queue][rxdp]->wb.upper.status_error;
    if (!(status & IXGBE_RXDADV_STAT_DD)) continue;
#endif

    count++;

     /*
      * Scan LOOK_AHEAD descriptors at a time to determine which descriptors
      * reference packets that are ready to be received.
      */

    int nb_rx = 0;

    for (i = 0; i < IXGBE_RX_MAX_BURST; i += LOOK_AHEAD) {

      if (i != 0) rxdp = (rxdp+LOOK_AHEAD) % NUM_RX_DESCRIPTORS_PER_QUEUE;

      /* Read desc statuses backwards to avoid race condition */
      for (j = LOOK_AHEAD-1; j >= 0; --j) {
        int rxdp_r = (rxdp+j) % NUM_RX_DESCRIPTORS_PER_QUEUE;
        s[j] = rx_desc[queue][rxdp_r]->wb.upper.status_error;
      }

      /* Clear everything but the status bits (LSB) */
      for (j = 0; j < LOOK_AHEAD; ++j)
        s[j] &= IXGBE_RXDADV_STAT_DD;

      /* Compute how many status bits were set */
      nb_dd = s[0]+s[1]+s[2]+s[3]+s[4]+s[5]+s[6]+s[7];
      nb_rx += nb_dd;
      
      for (j = 0; j < nb_dd; ++j) {
        int rxdp_r = (rxdp+j) % NUM_RX_DESCRIPTORS_PER_QUEUE;

        pktlen = rx_desc[queue][rxdp_r]->wb.upper.length;
        pkt = (uint8_t *) (rx_ring[queue].rx_buf[rxdp_r].virt_addr);

        /**  Interface to stack component **/
        bool new_pkt = false;
        pkt_status_t t = (pkt_status_t) _istack->receive_packet((pkt_buffer_t)pkt, pktlen, _index, queue);

        switch (t) {
         case KEEP_THIS_PACKET: 
           //allocate a new packet buffer in the RX descriptor
           new_pkt = true;
           break;
         case REUSE_THIS_PACKET:
           //reuse the old packet buffer in the RX descriptor
           break;
         default:
           PINF("[RX Queue %d] Found an error when processing a packet, error code (%x)\n",queue,t);
         }

         reset_rx_desc(queue,rxdp_r,new_pkt);
      }

      /* stop if all requested packets could not be received */
      if (nb_dd != LOOK_AHEAD)
        break;
    }

    unsigned tmp = rx_ring[queue].rx_tail + nb_rx;
    _mmio->mmio_write32(IXGBE_RDT(queue),(tmp-1) % NUM_RX_DESCRIPTORS_PER_QUEUE);

    rx_ring[queue].rx_tail = tmp % NUM_RX_DESCRIPTORS_PER_QUEUE;

#ifndef POLLING_MODE_ENABLED
    rxdp = rx_ring[queue].rx_tail;
    status = rx_desc[queue][rxdp]->wb.upper.status_error;
#endif

  }

  /* mask the interrupt bit so that new interrupt will be triggered */
  uint32_t eims = (1 << tid);
  _mmio->mmio_write32(IXGBE_EIMS,eims);
}

void Intel_x540_uddk_device::reg_info() {
  printf("========== NIC Registers Info ==========\n");
    
  printf("ATLASCTL %x\n",_mmio->mmio_read32(IXGBE_ATLASCTL));
  printf("AUTOC %x\n",_mmio->mmio_read32(IXGBE_AUTOC));
  printf("AUTOC2 %x\n",_mmio->mmio_read32(IXGBE_AUTOC2));
  printf("CORECTL %x\n",_mmio->mmio_read32(IXGBE_CORECTL));
  printf("CTRL %x\n",_mmio->mmio_read32(IXGBE_CTRL));
  printf("CTRL_EXT %x\n",_mmio->mmio_read32(IXGBE_CTRL_EXT));
  printf("DCA_RXCTRL(0) %x\n",_mmio->mmio_read32(IXGBE_DCA_RXCTRL(0)));
  printf("DCA_TXCTRL(0) %x\n",_mmio->mmio_read32(IXGBE_DCA_TXCTRL(0)));
  printf("DCA_TXCTRL_82599(0) %x\n",_mmio->mmio_read32(IXGBE_DCA_TXCTRL_82599(0)));
  printf("DMATXCTL %x\n",_mmio->mmio_read32(IXGBE_DMATXCTL));
  printf("DPMCS %x\n",_mmio->mmio_read32(IXGBE_DPMCS));
  printf("DTXCTL %x\n",_mmio->mmio_read32(IXGBE_DTXCTL));
  printf("EEC %x\n",_mmio->mmio_read32(IXGBE_EEC));
  printf("EERD %x\n",_mmio->mmio_read32(IXGBE_EERD));
  printf("EEWR %x\n",_mmio->mmio_read32(IXGBE_EEWR));
  printf("EIMC %x\n",_mmio->mmio_read32(IXGBE_EIMC));
  printf("EIMC_EX(0) %x\n",_mmio->mmio_read32(IXGBE_EIMC_EX(0)));
  printf("EIMS %x\n",_mmio->mmio_read32(IXGBE_EIMS));
  printf("ESDP %x\n",_mmio->mmio_read32(IXGBE_ESDP));
  printf("EXVET %x\n",_mmio->mmio_read32(IXGBE_EXVET));

  printf("FCCFG %x\n",_mmio->mmio_read32(IXGBE_FCCFG));
  printf("FCRTH(0) %x\n",_mmio->mmio_read32(IXGBE_FCRTH(0)));
  printf("FCRTH_82599(0) %x\n",_mmio->mmio_read32(IXGBE_FCRTH_82599(0)));
  printf("FCRTL(0) %x\n",_mmio->mmio_read32(IXGBE_FCRTL(0)));
  printf("FCRTL_82599(0) %x\n",_mmio->mmio_read32(IXGBE_FCRTL_82599(0)));
  printf("FCRTV %x\n",_mmio->mmio_read32(IXGBE_FCRTV));
  printf("FCTRL %x\n",_mmio->mmio_read32(IXGBE_FCTRL));
  printf("FCTTV(0) %x\n",_mmio->mmio_read32(IXGBE_FCTTV(0)));
  printf("FDIRCMD %x\n",_mmio->mmio_read32(IXGBE_FDIRCMD));
  printf("FDIRFREE %x\n",_mmio->mmio_read32(IXGBE_FDIRFREE));
  printf("FDIRHASH %x\n",_mmio->mmio_read32(IXGBE_FDIRHASH));
  printf("FDIRHKEY %x\n",_mmio->mmio_read32(IXGBE_FDIRHKEY));
  printf("FDIRCTRL %x\n",_mmio->mmio_read32(IXGBE_FDIRCTRL));
  printf("FDIRDIP4M %x\n",_mmio->mmio_read32(IXGBE_FDIRDIP4M));
  printf("FDIRIP6M %x\n",_mmio->mmio_read32(IXGBE_FDIRIP6M));
  printf("FDIRIPDA %x\n",_mmio->mmio_read32(IXGBE_FDIRIPDA));
  printf("FDIRIPSA %x\n",_mmio->mmio_read32(IXGBE_FDIRIPSA));
  printf("FDIRM %x\n",_mmio->mmio_read32(IXGBE_FDIRM));
  printf("FDIRPORT %x\n",_mmio->mmio_read32(IXGBE_FDIRPORT));
  printf("FDIRSIP4M %x\n",_mmio->mmio_read32(IXGBE_FDIRSIP4M));

  printf("IXGBE_FDIRSIPv6(0) %x\n",_mmio->mmio_read32(IXGBE_FDIRSIPv6(0)));
  printf("IXGBE_FDIRSKEY %x\n",_mmio->mmio_read32(IXGBE_FDIRSKEY));
  printf("IXGBE_FDIRTCPM %x\n",_mmio->mmio_read32(IXGBE_FDIRTCPM));
  printf("IXGBE_FDIRUDPM %x\n",_mmio->mmio_read32(IXGBE_FDIRUDPM));
  printf("IXGBE_FDIRVLAN %x\n",_mmio->mmio_read32(IXGBE_FDIRVLAN));
  printf("IXGBE_FLEX_MNG %x\n",_mmio->mmio_read32(IXGBE_FLEX_MNG));
  printf("IXGBE_GCR %x\n",_mmio->mmio_read32(IXGBE_GCR));
  printf("IXGBE_GCR_EXT %x\n",_mmio->mmio_read32(IXGBE_GCR_EXT));
  printf("IXGBE_GHECCR %x\n",_mmio->mmio_read32(IXGBE_GHECCR));
  printf("IXGBE_GPIE %x\n",_mmio->mmio_read32(IXGBE_GPIE));
  printf("IXGBE_GSSR %x\n",_mmio->mmio_read32(IXGBE_GSSR));
  printf("IXGBE_HICR %x\n",_mmio->mmio_read32(IXGBE_HICR));
  printf("IXGBE_HLREG0 %x\n",_mmio->mmio_read32(IXGBE_HLREG0));
  printf("IXGBE_I2CCTL %x\n",_mmio->mmio_read32(IXGBE_I2CCTL));
  printf("IXGBE_LEDCTL %x\n",_mmio->mmio_read32(IXGBE_LEDCTL));
  printf("IXGBE_MACC %x\n",_mmio->mmio_read32(IXGBE_MACC));
  printf("IXGBE_MAXFRS %x\n",_mmio->mmio_read32(IXGBE_MAXFRS));
  printf("IXGBE_MBVFICR(0) %x\n",_mmio->mmio_read32(IXGBE_MBVFICR(0)));
  printf("IXGBE_MCSTCTRL %x\n",_mmio->mmio_read32(IXGBE_MCSTCTRL));
  printf("IXGBE_MFLCN %x\n",_mmio->mmio_read32(IXGBE_MFLCN));

  printf("IXGBE_MPSAR_HI(0) %x\n",_mmio->mmio_read32(IXGBE_MPSAR_HI(0)));
  printf("IXGBE_MPSAR_LO(0) %x\n",_mmio->mmio_read32(IXGBE_MPSAR_LO(0)));
  printf("IXGBE_MRCTL(0) %x\n",_mmio->mmio_read32(IXGBE_MRCTL(0)));
  printf("IXGBE_MRQC %x\n",_mmio->mmio_read32(IXGBE_MRQC));
  printf("IXGBE_MSCA %x\n",_mmio->mmio_read32(IXGBE_MSCA));
  printf("IXGBE_MSRWD %x\n",_mmio->mmio_read32(IXGBE_MSRWD));
  printf("IXGBE_MTA(0) %x\n",_mmio->mmio_read32(IXGBE_MTA(0)));
  printf("IXGBE_MTQC %x\n",_mmio->mmio_read32(IXGBE_MTQC));
  printf("IXGBE_PCS1GANA %x\n",_mmio->mmio_read32(IXGBE_PCS1GANA));
  printf("IXGBE_PCS1GLCTL %x\n",_mmio->mmio_read32(IXGBE_PCS1GLCTL));
  printf("IXGBE_PDPMCS %x\n",_mmio->mmio_read32(IXGBE_PDPMCS));
  printf("IXGBE_PFDTXGSWC %x\n",_mmio->mmio_read32(IXGBE_PFDTXGSWC));
  printf("IXGBE_PFMAILBOX(0) %x\n",_mmio->mmio_read32(IXGBE_PFMAILBOX(0)));
  printf("IXGBE_PFMBMEM(0) %x\n",_mmio->mmio_read32(IXGBE_PFMBMEM(0)));
  printf("IXGBE_PFVFSPOOF(0) %x\n",_mmio->mmio_read32(IXGBE_PFVFSPOOF(0)));
  printf("IXGBE_PSRTYPE(0) %x\n",_mmio->mmio_read32(IXGBE_PSRTYPE(0)));
  printf("IXGBE_QDE %x\n",_mmio->mmio_read32(IXGBE_QDE));
  printf("IXGBE_RAH(0) %x\n",_mmio->mmio_read32(IXGBE_RAH(0)));
  printf("IXGBE_RAL(0) %x\n",_mmio->mmio_read32(IXGBE_RAL(0)));
  printf("IXGBE_RETA(0) %x\n",_mmio->mmio_read32(IXGBE_RETA(0)));
  
  printf("IXGBE_RDBAH(0) %x\n",_mmio->mmio_read32(IXGBE_RDBAH(0)));
  printf("IXGBE_RDBAL(0) %x\n",_mmio->mmio_read32(IXGBE_RDBAL(0)));
  printf("IXGBE_RDH(0) %x\n",_mmio->mmio_read32(IXGBE_RDH(0)));
  printf("IXGBE_RDLEN(0) %x\n",_mmio->mmio_read32(IXGBE_RDLEN(0)));
  printf("IXGBE_RDRXCTL %x\n",_mmio->mmio_read32(IXGBE_RDRXCTL));
  printf("IXGBE_RDT(0) %x\n",_mmio->mmio_read32(IXGBE_RDT(0)));
  printf("IXGBE_RMCS %x\n",_mmio->mmio_read32(IXGBE_RMCS));
  printf("IXGBE_RQSMR(0) %x\n",_mmio->mmio_read32(IXGBE_RQSMR(0)));
  printf("IXGBE_RSSRK(0) %x\n",_mmio->mmio_read32(IXGBE_RSSRK(0)));
  printf("IXGBE_RT2CR(0) %x\n",_mmio->mmio_read32(IXGBE_RT2CR(0)));
  printf("IXGBE_RTRPCS %x\n",_mmio->mmio_read32(IXGBE_RTRPCS));
  printf("IXGBE_RTRPT4C(0) %x\n",_mmio->mmio_read32(IXGBE_RTRPT4C(0)));
  printf("IXGBE_RTRUP2TC %x\n",_mmio->mmio_read32(IXGBE_RTRUP2TC));
  printf("IXGBE_RTTBCNRC %x\n",_mmio->mmio_read32(IXGBE_RTTBCNRC));
  printf("IXGBE_RTTDCS %x\n",_mmio->mmio_read32(IXGBE_RTTDCS));
  printf("IXGBE_RTTDT1C %x\n",_mmio->mmio_read32(IXGBE_RTTDT1C));
  printf("IXGBE_RTTDT2C(0) %x\n",_mmio->mmio_read32(IXGBE_RTTDT2C(0)));
  printf("IXGBE_RTTDQSEL %x\n",_mmio->mmio_read32(IXGBE_RTTDQSEL));
  printf("IXGBE_RTTPCS %x\n",_mmio->mmio_read32(IXGBE_RTTPCS));
  printf("IXGBE_RTTPT2C(0) %x\n",_mmio->mmio_read32(IXGBE_RTTPT2C(0)));


  printf("IXGBE_RTTUP2TC %x\n",_mmio->mmio_read32(IXGBE_RTTUP2TC));
  printf("IXGBE_RUPPBMR %x\n",_mmio->mmio_read32(IXGBE_RUPPBMR));
  printf("IXGBE_RXCSUM %x\n",_mmio->mmio_read32(IXGBE_RXCSUM));
  printf("IXGBE_RXCTRL %x\n",_mmio->mmio_read32(IXGBE_RXCTRL));
  printf("IXGBE_RXDCTL(0) %x\n",_mmio->mmio_read32(IXGBE_RXDCTL(0)));
  printf("IXGBE_RXPBSIZE(0) %x\n",_mmio->mmio_read32(IXGBE_RXPBSIZE(0)));
  printf("IXGBE_SECRXCTRL %x\n",_mmio->mmio_read32(IXGBE_SECRXCTRL));
  printf("IXGBE_SECTXMINIFG %x\n",_mmio->mmio_read32(IXGBE_SECTXMINIFG));
  printf("IXGBE_SRRCTL(0) %x\n",_mmio->mmio_read32(IXGBE_SRRCTL(0)));
  printf("IXGBE_SWFW_SYNC %x\n",_mmio->mmio_read32(IXGBE_SWFW_SYNC));
  printf("IXGBE_TDBAH(0) %x\n",_mmio->mmio_read32(IXGBE_TDBAH(16)));
  printf("IXGBE_TDBAL(0) %x\n",_mmio->mmio_read32(IXGBE_TDBAL(16)));
  printf("IXGBE_TDH(0) %x\n",_mmio->mmio_read32(IXGBE_TDH(16)));
  printf("IXGBE_TDLEN(0) %x\n",_mmio->mmio_read32(IXGBE_TDLEN(16)));
  printf("IXGBE_TDPT2TCCR(0) %x\n",_mmio->mmio_read32(IXGBE_TDPT2TCCR(0)));
  printf("IXGBE_TDT(0) %x\n",_mmio->mmio_read32(IXGBE_TDT(16)));
  printf("IXGBE_TDTQ2TCCR(0) %x\n",_mmio->mmio_read32(IXGBE_TDTQ2TCCR(0)));
  printf("IXGBE_TQSM(0) %x\n",_mmio->mmio_read32(IXGBE_TQSM(0)));
  printf("IXGBE_TQSMR(0) %x\n",_mmio->mmio_read32(IXGBE_TQSMR(0)));
  
  printf("IXGBE_TXDCTL(0) %x\n",_mmio->mmio_read32(IXGBE_TXDCTL(16)));
  printf("IXGBE_TXPBSIZE(0) %x\n",_mmio->mmio_read32(IXGBE_TXPBSIZE(0)));
  printf("IXGBE_TXPBTHRESH(0) %x\n",_mmio->mmio_read32(IXGBE_TXPBTHRESH(0)));
  printf("IXGBE_UTA(0) %x\n",_mmio->mmio_read32(IXGBE_UTA(0)));
  printf("IXGBE_VFDCA_TXCTRL(0) %x\n",_mmio->mmio_read32(IXGBE_VFDCA_TXCTRL(16)));
  printf("IXGBE_VFLREC(0) %x\n",_mmio->mmio_read32(IXGBE_VFLREC(0)));
  printf("IXGBE_VFMAILBOX %x\n",_mmio->mmio_read32(IXGBE_VFMAILBOX));
  printf("IXGBE_VFMBMEM %x\n",_mmio->mmio_read32(IXGBE_VFMBMEM));
  printf("IXGBE_VFPSRTYPE %x\n",_mmio->mmio_read32(IXGBE_VFPSRTYPE));
  printf("IXGBE_VFRDBAH(0) %x\n",_mmio->mmio_read32(IXGBE_VFRDBAH(0)));
  printf("IXGBE_VFRDBAL(0) %x\n",_mmio->mmio_read32(IXGBE_VFRDBAL(0)));
  printf("IXGBE_VFRDH(0) %x\n",_mmio->mmio_read32(IXGBE_VFRDH(0)));
  printf("IXGBE_VFRDLEN(0) %x\n",_mmio->mmio_read32(IXGBE_VFRDLEN(0)));
  printf("IXGBE_VFRDT(0) %x\n",_mmio->mmio_read32(IXGBE_VFRDT(0)));
  printf("IXGBE_VFRE(0) %x\n",_mmio->mmio_read32(IXGBE_VFRE(0)));
  printf("IXGBE_VFRXDCTL(0) %x\n",_mmio->mmio_read32(IXGBE_VFRXDCTL(0)));
  printf("IXGBE_VFSRRCTL(0) %x\n",_mmio->mmio_read32(IXGBE_VFSRRCTL(0)));
  printf("IXGBE_VFTA(0) %x\n",_mmio->mmio_read32(IXGBE_VFTA(0)));
  printf("IXGBE_VFTAVIND(0,0) %x\n",_mmio->mmio_read32(IXGBE_VFTAVIND(0,0)));
  printf("IXGBE_VFTDBAH(0) %x\n",_mmio->mmio_read32(IXGBE_VFTDBAH(0)));


  printf("IXGBE_VFTDBAL(0) %x\n",_mmio->mmio_read32(IXGBE_VFTDBAL(0)));
  printf("IXGBE_VFTDH(0) %x\n",_mmio->mmio_read32(IXGBE_VFTDH(0)));
  printf("IXGBE_VFTDLEN(0) %x\n",_mmio->mmio_read32(IXGBE_VFTDLEN(0)));
  printf("IXGBE_VFTDT(0) %x\n",_mmio->mmio_read32(IXGBE_VFTDT(0)));
  printf("IXGBE_VFTE(0) %x\n",_mmio->mmio_read32(IXGBE_VFTE(0)));
  printf("IXGBE_VFTXDCTL(0) %x\n",_mmio->mmio_read32(IXGBE_VFTXDCTL(0)));
  printf("IXGBE_VLNCTRL %x\n",_mmio->mmio_read32(IXGBE_VLNCTRL));
  printf("IXGBE_VLVF(0) %x\n",_mmio->mmio_read32(IXGBE_VLVF(0)));
  printf("IXGBE_VLVFB(0) %x\n",_mmio->mmio_read32(IXGBE_VLVFB(0)));
  printf("IXGBE_VMECM(0) %x\n",_mmio->mmio_read32(IXGBE_VMECM(0)));
  printf("IXGBE_VMOLR(0) %x\n",_mmio->mmio_read32(IXGBE_VMOLR(0)));
  printf("IXGBE_VMRVLAN(0) %x\n",_mmio->mmio_read32(IXGBE_VMRVLAN(0)));
  printf("IXGBE_VMRVM(0) %x\n",_mmio->mmio_read32(IXGBE_VMRVM(0)));
  printf("IXGBE_VMVIR(0) %x\n",_mmio->mmio_read32(IXGBE_VMVIR(0)));
  printf("IXGBE_VT_CTL %x\n",_mmio->mmio_read32(IXGBE_VT_CTL));
  printf("IXGBE_VTEIMC %x\n",_mmio->mmio_read32(IXGBE_VTEIMC));
}

inline void Intel_x540_uddk_device::tx4(struct exo_mbuf ** tx_pkts, unsigned tx_queue, unsigned pos) {
  TX_DESC * desc_ptr = (TX_DESC *)tx_desc[tx_queue][pos];
  unsigned i;
  for (i = 0; i < 4; i++) {
    desc_ptr->address = (addr_t)((*tx_pkts)->phys_addr);
    desc_ptr->length = (*tx_pkts)->len;
    desc_ptr->cmd = 3;
    desc_ptr->sta = 0;
    desc_ptr++;
    tx_pkts++;
  }
}

inline void Intel_x540_uddk_device::tx1(struct exo_mbuf ** tx_pkts, unsigned tx_queue, unsigned pos) {
  TX_DESC * desc_ptr = (TX_DESC *)tx_desc[tx_queue][pos];
  desc_ptr->address = (addr_t)((*tx_pkts)->phys_addr);
  desc_ptr->length = (*tx_pkts)->len;
  desc_ptr->cmd = 3;
  desc_ptr->sta = 0;
}

/*
 * Check for descriptors with their DD bit set and free bufs.
 * Return the total number of buffers freed.
 * This function is only used with simple send function.
 */
inline unsigned Intel_x540_uddk_device::tx_free_bufs(unsigned tx_queue) {
  unsigned i;
  uint32_t status;
  unsigned pos;
  struct exo_tx_ring * txr = &tx_ring[tx_queue];

  /* check DD bit on threshold descriptor */
  pos = txr->tx_next_dd;
  status = ((TX_DESC *)tx_desc[tx_queue][pos])->sta;

  if (!(status & IXGBE_ADVTXD_STAT_DD)) {
    return 0;
  }

  /*
   * first buffer to free from S/W ring is at index
   * tx_next_dd - (tx_rs_thresh-1)
   */  
  for (i = txr->tx_next_dd - (txr->tx_rs_thresh-1); 
       i < txr->tx_rs_thresh; i++)
    exo_prefetch0((volatile void *)txr->sw_ring[i].tx_buf.virt_addr);
  
  /* free buffers one at a time */
  for (i = txr->tx_next_dd - (txr->tx_rs_thresh-1); i < txr->tx_rs_thresh; i++) { 
    // if FREE_OK flag is set, free the packet buffer
    if ((txr->sw_ring[i].tx_buf.flag & 1) == 1)
      _imem->free((void *)(txr->sw_ring[i].tx_buf.virt_addr), PACKET_ALLOCATOR, _index);

    // free the metadata header
    __builtin_memset(&(txr->sw_ring[i]),0,sizeof(struct tx_entry));
  }

  /* buffers were freed, update counters */
  txr->nb_tx_free = (uint16_t)(txr->nb_tx_free + txr->tx_rs_thresh);
  txr->tx_next_dd = (uint16_t)(txr->tx_next_dd + txr->tx_rs_thresh);
  if (txr->tx_next_dd >= txr->nb_tx_desc)
    txr->tx_next_dd = (uint16_t)(txr->tx_rs_thresh - 1);

  return txr->tx_rs_thresh;
}

/** 
 * Full-feature packet send. Each frame consists of multiple segments (up to 3). IP checksum offloading.
 * 
 * @param tx_pkts 
 * @param nb_pkts
 * @param tx_queue
 */
uint16_t Intel_x540_uddk_device::multi_send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned tx_queue) {
  uint32_t ctx = 0;
  struct exo_tx_ring * txq = &tx_ring[tx_queue];
  uint16_t tx_id = txq->tx_tail;
  struct tx_entry *sw_ring = txq->sw_ring;
  volatile TX_ADV_DATA_DESC *txr = (volatile TX_ADV_DATA_DESC *)txq->desc;
  volatile TX_ADV_DATA_DESC *txd;
  struct tx_entry *txe, *txn;
  uint16_t nb_tx;
  uint32_t new_ctx;
  struct exo_mbuf *tx_pkt;
  uint32_t pkt_len;
  uint16_t tx_last;
  uint16_t nb_used;

  txe = &sw_ring[tx_id];
  /* Determine if the descriptor ring needs to be cleaned. */
  if ((txq->nb_tx_desc - txq->nb_tx_free) > txq->tx_free_thresh) {
    xmit_cleanup(txq);
  }

  uint32_t first_desc_cmd = IXGBE_ADVTXD_DTYP_DATA | IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DCMD_IFCS;
  uint32_t first_desc_status = IXGBE_ADVTXD_CC | IXGBE_ADVTXD_POPTS_IXSM;
  uint32_t last_desc_cmd= first_desc_cmd | IXGBE_ADVTXD_DCMD_EOP;

  /* TX loop */
  for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
    new_ctx = 1;
    tx_pkt = *tx_pkts++;
    pkt_len = tx_pkt->len + tx_pkt->seg_len[0] + tx_pkt->seg_len[1];
    /*
     * Keep track of how many descriptors are used this loop
     * This will always be the number of segments + the number of
     * Context descriptors required to transmit the packet
     */
    nb_used = (uint16_t)(tx_pkt->nb_segment + new_ctx);

    /*
     * The number of descriptors that must be allocated for a
     * packet is the number of segments of that packet, plus 1
     * Context Descriptor for the hardware offload, if any.
     * Determine the last TX descriptor to allocate in the TX ring
     * for the packet, starting from the current position (tx_id)
     * in the ring.
     */
    tx_last = (uint16_t) (tx_id + nb_used - 1);

    /* Circular ring */
    if (tx_last >= txq->nb_tx_desc)
      tx_last = (uint16_t) (tx_last - txq->nb_tx_desc);

#if 0
    printf("NIC=%u queue=%u pktlen=%u tx_first=%u tx_last=%u\n",
               (unsigned) _index,
               (unsigned) txq->reg_idx,
               (unsigned) pkt_len,
               (unsigned) tx_id,
               (unsigned) tx_last);
#endif
    /*
     * Make sure there are enough TX descriptors available to
     * transmit the entire packet.
     * nb_used better be less than or equal to txq->tx_rs_thresh
     */
    if (nb_used > txq->nb_tx_free) {
//      printf("Not enough free TX descriptors nb_used=%4u nb_free=%4u (NIC=%d queue=%d)\n",
//                   nb_used, txq->nb_tx_free,
//                   _index, txq->reg_idx);

      if (xmit_cleanup(txq) != 0) {
        /* Could not clean any descriptors */
        if (nb_tx == 0)
          return (0);
        goto end_of_tx;
      }
    }

    /*
     * By now there are enough free TX descriptors to transmit
     * the packet.
     */
    assert(nb_used <= txq->nb_tx_free);

    /* prepare the context descriptor */
    TX_ADV_CONTEXT_DESC * tx_adv_context_desc = (TX_ADV_CONTEXT_DESC *) tx_desc[tx_queue][tx_id];
    tx_adv_context_desc->vlan_macip_lens= 20 | (14<<9); // IP header len (20) and MAC len (14)
    tx_adv_context_desc->special= IXGBE_ADVTXD_TUCMD_IPV4 | IXGBE_ADVTXD_DTYP_CTXT | IXGBE_ADVTXD_DCMD_DEXT;
    txn = &sw_ring[txe->next_id];
    txe->last_id = tx_last;
    tx_id = txe->next_id;
    txe = txn;

    txd = &txr[tx_id];

    /* copy tx_pkt to sw_ring corresponding slot so that later on we can free packet memory*/
    txe->tx_buf = *tx_pkt; 
  
    if (tx_pkt->nb_segment == 1) {
      /* prepare the only data descriptor */
      TX_ADV_DATA_DESC * tx_adv_data_desc = (TX_ADV_DATA_DESC *) (tx_desc[tx_queue][tx_id]);
      tx_adv_data_desc->address = (addr_t)(tx_pkt->phys_addr);
      tx_adv_data_desc->cmd_type_len = tx_pkt->len | last_desc_cmd;
      tx_adv_data_desc->olinfo_status = first_desc_status | (pkt_len<<IXGBE_ADVTXD_PAYLEN_SHIFT);
    } 
    else {
      /* prepare the first descriptor */
      TX_ADV_DATA_DESC * tx_adv_data_desc = (TX_ADV_DATA_DESC *) (tx_desc[tx_queue][tx_id]);
      tx_adv_data_desc->address = (addr_t)(tx_pkt->phys_addr);
      tx_adv_data_desc->cmd_type_len = tx_pkt->len | first_desc_cmd;
      tx_adv_data_desc->olinfo_status = first_desc_status | (pkt_len<<IXGBE_ADVTXD_PAYLEN_SHIFT);
 
      /* update pointers */
      txn = &sw_ring[txe->next_id];
      txe->last_id = tx_last;
      tx_id = txe->next_id;
      txe = txn;

      txd = &txr[tx_id];

      /* prepare the second and third descriptors if needed */
      tx_adv_data_desc = (TX_ADV_DATA_DESC *) (tx_desc[tx_queue][tx_id]);

      if (tx_pkt->seg_len[0] != 0 && tx_pkt->seg_len[1] != 0) {
        assert(tx_pkt->nb_segment == 3);
        /* CASE 1: this packet contains both app header and app payload */
        /* app header piece */
        tx_adv_data_desc->address = (addr_t)(tx_pkt->phys_addr_seg[0]);
        tx_adv_data_desc->cmd_type_len = tx_pkt->seg_len[0] | first_desc_cmd;

        txn = &sw_ring[txe->next_id];
        txe->last_id = tx_last;
        tx_id = txe->next_id;
        txe = txn;

        txd = &txr[tx_id];

        /* app payload piece */
        tx_adv_data_desc = (TX_ADV_DATA_DESC *) (tx_desc[tx_queue][tx_id]);
        tx_adv_data_desc->address = (addr_t)(tx_pkt->phys_addr_seg[1]);
        tx_adv_data_desc->cmd_type_len = tx_pkt->seg_len[1] | last_desc_cmd;
      }
      else if (tx_pkt->seg_len[0] != 0 && tx_pkt->seg_len[1] == 0) {
        assert(tx_pkt->nb_segment == 2);
        /* CASE 2: this packet contains only app header */
        tx_adv_data_desc->address = (addr_t)(tx_pkt->phys_addr_seg[0]);
        tx_adv_data_desc->cmd_type_len = tx_pkt->seg_len[0] | last_desc_cmd;
      }
      else if (tx_pkt->seg_len[0] == 0 && tx_pkt->seg_len[1] != 0) {
        assert(tx_pkt->nb_segment == 2);
        /* CASE 3: this packet contains only app payload */
        tx_adv_data_desc->address = (addr_t)(tx_pkt->phys_addr_seg[1]);
        tx_adv_data_desc->cmd_type_len = tx_pkt->seg_len[1] | last_desc_cmd;
      } 
    }  

    /* update pointers */
    txn = &sw_ring[txe->next_id];
    txe->last_id = tx_last;
    tx_id = txe->next_id;
    txe = txn;
    
    txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
    txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

    /* Set RS bit only on threshold packets' last descriptor */
    if (txq->nb_tx_used >= txq->tx_rs_thresh) {
//      printf("Setting RS bit on TXD id=%4u (NIC=%d queue=%d)\n",
//                                        tx_last, _index, txq->reg_idx);

      /* Update txq RS bit counters */
      txq->nb_tx_used = 0;
      txd->cmd_type_len = txd->cmd_type_len | IXGBE_ADVTXD_DCMD_RS;
    }
  }

  end_of_tx:
    _mm_sfence();
  /*
   * Set the Transmit Descriptor Tail (TDT)
   */
  //printf("NIC=%u queue_id=%u tx_tail=%u nb_tx=%u\n",
  //                 _index, (unsigned) txq->reg_idx,
  //                 (unsigned) tx_id, (unsigned) nb_tx);
  _mmio->mmio_write32(IXGBE_TDT(tx_queue+TX_QUEUE_SHIFT), tx_id);
  txq->tx_tail = tx_id;
  return nb_tx; 
}

/* Reset transmit descriptors after they have been used */
inline int Intel_x540_uddk_device::xmit_cleanup(struct exo_tx_ring *txq) {
  struct tx_entry *sw_ring = txq->sw_ring;
  volatile union ixgbe_adv_tx_desc *txr = (volatile union ixgbe_adv_tx_desc*)txq->desc;
  uint16_t last_desc_cleaned = txq->last_desc_cleaned;
  uint16_t nb_tx_desc = txq->nb_tx_desc;
  uint16_t desc_to_clean_to;
  uint16_t nb_tx_to_clean;

  /* Determine the last descriptor needing to be cleaned */
  desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
  if (desc_to_clean_to >= nb_tx_desc)
    desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

  /* Check to make sure the last descriptor to clean is done */
  desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
  if (!(txr[desc_to_clean_to].wb.status & IXGBE_TXD_STAT_DD)) {
//    printf("TX descriptor %4u is not done (NIC=%d queue=%d)\n", desc_to_clean_to,
//                _index, txq->reg_idx);
    /* Failed to clean any descriptors, better luck next time */
    return -1;
  }

  /* Figure out how many descriptors will be cleaned */
  if (last_desc_cleaned > desc_to_clean_to)
    nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
                                                        desc_to_clean_to);
  else
    nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
                                                last_desc_cleaned);

  //printf("Cleaning %4u TX descriptors: %4u to %4u (NIC=%d queue=%d)\n",
  //                      nb_tx_to_clean, last_desc_cleaned, desc_to_clean_to,
  //                      _index, txq->reg_idx);

  /* free packet memory that have been transmitted between last_desc_cleaned to desc_to_clean_to */
  if (last_desc_cleaned > desc_to_clean_to) {
    /* wrap up in circular ring */
    /* first segment */
    for (unsigned i = last_desc_cleaned + 1; i < txq->nb_tx_desc; i++) {
      struct exo_mbuf * mbuf = &(txq->sw_ring[i].tx_buf);
      if (((mbuf->flag) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr), (mbuf->flag >> 4), _index);
      }
      if (((mbuf->seg_flag[0]) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr_seg[0]), (mbuf->seg_flag[0] >> 4), _index);
      }
      if (((mbuf->seg_flag[1]) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr_seg[1]), (mbuf->seg_flag[1] >> 4), _index);
      }

      // Decrement reference counter for a frame list that was used in a response.
      if (mbuf->pbuf_list != NULL) {
        pbuf_t * pbuf_list = (pbuf_t *)(mbuf->pbuf_list);
        assert(pbuf_list->ref_counter > 0);
        int64_t ref_c = __sync_sub_and_fetch(&(pbuf_list->ref_counter), 1);
      }

      __builtin_memset(&(txq->sw_ring[i]),0,sizeof(struct tx_entry));
      txq->sw_ring[i].next_id = (i + 1) % (txq->nb_tx_desc);
      txq->sw_ring[i].last_id = i;
    }

    /* second segment */
    for (unsigned i = 0; i <= desc_to_clean_to; i++) {
      struct exo_mbuf * mbuf = &(txq->sw_ring[i].tx_buf);
      if (((mbuf->flag) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr), (mbuf->flag >> 4), _index);
      }
      if (((mbuf->seg_flag[0]) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr_seg[0]), (mbuf->seg_flag[0] >> 4), _index);
      }
      if (((mbuf->seg_flag[1]) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr_seg[1]), (mbuf->seg_flag[1] >> 4), _index);
      }

      // Decrement reference counter for a frame list that was used in a response.
      if (mbuf->pbuf_list != NULL) {
        pbuf_t * pbuf_list = (pbuf_t *)(mbuf->pbuf_list);
        assert(pbuf_list->ref_counter > 0);
        int64_t ref_c = __sync_sub_and_fetch(&(pbuf_list->ref_counter), 1);
      }

      __builtin_memset(&(txq->sw_ring[i]),0,sizeof(struct tx_entry));
      txq->sw_ring[i].next_id = (i + 1) % (txq->nb_tx_desc);
      txq->sw_ring[i].last_id = i;
    }
  }
  else {
    for (unsigned i = last_desc_cleaned + 1; i <= desc_to_clean_to; i++) {
      struct exo_mbuf * mbuf = &(txq->sw_ring[i].tx_buf); 
      if (((mbuf->flag) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr), (mbuf->flag >> 4), _index);
      }
      if (((mbuf->seg_flag[0]) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr_seg[0]), (mbuf->seg_flag[0] >> 4), _index);
      }
      if (((mbuf->seg_flag[1]) & 1) == 1) {
        _imem->free((void *)(mbuf->virt_addr_seg[1]), (mbuf->seg_flag[1] >> 4), _index);
      }

      // Decrement reference counter for a frame list that was used in a response.
      if (mbuf->pbuf_list != NULL) {
        pbuf_t * pbuf_list = (pbuf_t *)(mbuf->pbuf_list);
        assert(pbuf_list->ref_counter > 0);
        int64_t ref_c = __sync_sub_and_fetch(&(pbuf_list->ref_counter), 1);
      }

      __builtin_memset(&(txq->sw_ring[i]),0,sizeof(struct tx_entry));
      txq->sw_ring[i].next_id = (i + 1) % (txq->nb_tx_desc);
      txq->sw_ring[i].last_id = i;
    }
  }

  /*
   * The last descriptor to clean is done, so that means all the
   * descriptors from the last descriptor that was cleaned
   * up to the last descriptor with the RS bit set
   * are done. Only reset the threshold descriptor.
   */
  txr[desc_to_clean_to].wb.status = 0;

  /* Update the txq to reflect the last descriptor that was cleaned */
  txq->last_desc_cleaned = desc_to_clean_to;
  txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

  /* No Error */
  return 0;
}

/** 
 * Simple packet send. One packet per descriptor. No offloading.
 * 
 * @param tx_pkts 
 * @param nb_pkts
 * @param tx_queue
 * @return The number of packets that are sent out.
 */
uint16_t Intel_x540_uddk_device::send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned tx_queue) {
  unsigned pos;
  uint16_t n;
  struct exo_tx_ring * txr = &tx_ring[tx_queue];
  /*
   * Begin scanning the H/W ring for done descriptors when the
   * number of available descriptors drops below tx_free_thresh.  For
   * each done descriptor, free the associated buffer.
   */
  if (txr->nb_tx_free < txr->tx_free_thresh)
    tx_free_bufs(tx_queue);

  /* Only use descriptors that are available. */
  nb_pkts = (uint16_t)EXO_MIN(txr->nb_tx_free, nb_pkts);

  if (nb_pkts == 0)
    return 0;

  /* Use exactly nb_pkts descriptors */
  txr->nb_tx_free = (uint16_t)(txr->nb_tx_free - nb_pkts);
  txr->nb_tx_used = (uint16_t)(txr->nb_tx_used + nb_pkts);

  /*
   * See if we're going to wrap-around. If so, handle the top
   * of the descriptor ring first, then do the bottom.  If not,
   * the processing looks just like the "bottom" part anyway...
   */
  if ((txr->tx_tail + nb_pkts) > txr->nb_tx_desc) {
    n = (uint16_t)(txr->nb_tx_desc - txr->tx_tail);
    tx_fill_hw_ring(tx_pkts, n, tx_queue);

    /*
     * We know that the last descriptor in the ring will need to
     * have its RS bit set because tx_rs_thresh has to be
     * a divisor of the ring size
     */
    pos = txr->tx_next_rs;
    ((TX_DESC *)tx_desc[tx_queue][pos])->cmd |= (1 << 3);
    txr->tx_next_rs = (uint16_t)(txr->tx_rs_thresh - 1);
    txr->tx_tail = 0;
  }

  /* Fill H/W descriptor ring with packet data */
  tx_fill_hw_ring(tx_pkts + n, (uint16_t)(nb_pkts - n), tx_queue);  
  txr->tx_tail = (uint16_t)(txr->tx_tail + (nb_pkts - n));

  /*
   * Determine if RS bit should be set
   * This is what we actually want:
   *   if ((tx_tail - 1) >= txq->tx_next_rs)
   * but instead of subtracting 1 and doing >=, we can just do
   * greater than without subtracting.
   */
  if (txr->tx_tail > txr->tx_next_rs) {
    pos = txr->tx_next_rs;
    ((TX_DESC *)tx_desc[tx_queue][pos])->cmd |= (1 << 3);    
    txr->tx_next_rs = (uint16_t)(txr->tx_next_rs + txr->tx_rs_thresh);
    if (txr->tx_next_rs >= txr->nb_tx_desc)
      txr->tx_next_rs = (uint16_t)(txr->tx_rs_thresh - 1);
  }

  /*
   * Check for wrap-around. This would only happen if we used
   * up to the last descriptor in the ring, no more, no less.
   */
  if (txr->tx_tail >= txr->nb_tx_desc)
    txr->tx_tail = 0;

  /* update tail pointer */
  _mm_sfence();
  _mmio->mmio_write32(IXGBE_TDT(tx_queue+TX_QUEUE_SHIFT), txr->tx_tail);

  //printf("sent %d packets, tail(%d), free(%d), next_dd(%d), next_rs(%d)\n",nb_pkts, txr->tx_tail, txr->nb_tx_free, txr->tx_next_dd, txr->tx_next_rs); 
  return nb_pkts;
}

inline void Intel_x540_uddk_device::tx_fill_hw_ring(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned queue) {
  const unsigned N_PER_LOOP = 4;
  const unsigned N_PER_LOOP_MASK = N_PER_LOOP-1;
  unsigned mainpart, leftover;
  unsigned i,j;
  struct exo_tx_ring * txr = &tx_ring[queue];
  unsigned pos = txr->tx_tail;
  
  /*
   * Process most of the packets in chunks of N pkts.  Any
   * leftover packets will get processed one at a time.
   */ 
  mainpart = (nb_pkts & ((uint32_t) ~N_PER_LOOP_MASK));
  leftover = (nb_pkts & ((uint32_t)  N_PER_LOOP_MASK));

  for (i = 0; i < mainpart; i += N_PER_LOOP) {
    for (j = 0; j < N_PER_LOOP; ++j) {
      txr->sw_ring[pos+i+j].tx_buf = **(tx_pkts + i + j);
    }
    tx4(tx_pkts + i, queue, pos+i); 
  }

  if (leftover > 0) {
    for (i = 0; i < leftover; ++i) {
      txr->sw_ring[pos + mainpart + i].tx_buf = **(tx_pkts + mainpart + i);  
      tx1(tx_pkts + mainpart + i, queue, pos + mainpart + i);  
    }
  }
}
