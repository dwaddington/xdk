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

#ifndef __X540_DEVICE_H__
#define __X540_DEVICE_H__

#include <libexo.h>
#include "driver_config.h"
#include "x540_types.h"
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include <net/protocol.h>
#include <x540/xml_config_parser.h>

#define PKT_MAX_SIZE                          (2048-64)
#define LOOK_AHEAD                            (8)
#define TX_QUEUE_SHIFT                        (16)
#define TX_QUEUE_WTHRESH                      (0)
#define TX_QUEUE_PTHRESH                      (36)
#define TX_QUEUE_HTHRESH                      (0)
#define FDIR_MAX_FILTER_LENGTH                (10)
#define FDIR_MIN_FILTER_THRESH                (4)
#define IXGBE_MAX_RX_DESC_POLL                (10)
#define IXGBE_MAX_SECRX_POLL                  (40)
#define TX_FREE_THRESH                        (64)
#define TX_RS_THRESH                          (64)
#define IXGBE_RX_MAX_BURST                    (64)
#define IXGBE_TX_MAX_BURST                    (64)

#define FLOW_SIGNATURE_0                      (0x0000)
#define FLOW_SIGNATURE_1                      (0x0100)
#define FLOW_SIGNATURE_2                      (0x0200)
#define FLOW_SIGNATURE_3                      (0x0300)
#define FLOW_SIGNATURE_4                      (0x0400)

using namespace Exokernel;

class Intel_x540_uddk_device : public Exokernel::Pci_express_device {

public:
  
  /** 
   * Constructor
   * 
   */
  Intel_x540_uddk_device(INic * inic, IStack * istack, 
                         IMem * imem, unsigned index, Config_params * params) : Exokernel::Pci_express_device(index, 0x8086, 0x1528) {
    _params = params;
    rx_threads_cpu_mask = _params->rx_threads_cpu_mask;
    cpus_per_nic = _params->cpus_per_nic;
    flex_byte_pos = _params->flex_byte_pos;
    server_timestamp = _params->server_timestamp;
    msix_vector_num = _params->channels_per_nic;

    server_ip = _params->server_ip[index]; 

    _irq = (unsigned *) malloc(msix_vector_num * sizeof(unsigned));

    assert(inic);
    assert(istack);
    _inic = inic;
    _istack = istack;
    _imem = imem;
    _index = index;
    _mmio = pci_memory_region(0); // IX540 flash BAR (see 3.4.4.1)
    msix_support() ? PINF("MSI-X detection OK.") : PINF("Error: MSI-X not supported."); 

    _inic->set_comp_state(NIC_CREATED_STATE, index); // Memory component can start
  }
  
  unsigned* _irq;
  std::vector<unsigned> _msi_vectors;

  struct exo_tx_ring tx_ring[NUM_TX_QUEUES];
  struct exo_rx_ring rx_ring[NUM_RX_QUEUES];

  volatile TX_ADV_DATA_DESC*                   tx_desc[NUM_TX_QUEUES][NUM_TX_DESCRIPTORS_PER_QUEUE];
  volatile RX_ADV_DATA_DESC*                   rx_desc[NUM_RX_QUEUES][NUM_RX_DESCRIPTORS_PER_QUEUE];

  INic * _inic;
  IStack * _istack;
  IMem * _imem;
  Config_params * _params;

private:

  class Nic_memory {
  private:
    addr_t         _vaddr;
    addr_t         _paddr;
    uint32_t       _size;
    Exokernel::Pci_express_device * _owner;

  public:
    Nic_memory(Exokernel::Pci_express_device * owner, 
               addr_t paddr, size_t size) : 
      _paddr(paddr), _vaddr(0), _size(size), _owner(owner) {
      assert(owner);
      _vaddr = (addr_t) (((Intel_x540_uddk_device *)owner)->_mmio);
      assert(_vaddr);
      PLOG("NIC_memory: paddr=0x%lx vaddr=0x%lx", _paddr, _vaddr);
    }
    
    INLINE uint32_t mmio_read32(uint32_t reg) {
       assert(_vaddr);
      return *((volatile uint32_t *)((_vaddr+reg)));
    }

    INLINE void mmio_write32(uint32_t reg, uint32_t val) {
      assert(_vaddr);
      *((volatile uint32_t *)((_vaddr+reg))) = val;
    }

  };

public:
  std::string rx_threads_cpu_mask;
  unsigned cpus_per_nic;
  unsigned flex_byte_pos;
  unsigned server_timestamp;
  unsigned msix_vector_num;
  std::string server_ip;
  uint8_t mac[6];
  struct eth_addr ethaddr;
  uint8_t ip[4];
  ip_addr_t ip_addr;
  unsigned _index;
  uint32_t _msix_cap_ptr;
  std::vector<unsigned> _msix_vector_list;
  Exokernel::Device_sysfs::Pci_mapped_memory_region * _mmio;
  uint32_t num_q_vectors;
  uint32_t rx_pb_size;
  uint32_t max_tx_queues;
  uint32_t max_rx_queues;
  uint32_t fdir_pballoc;
  uint8_t msi_number[NUM_RX_THREADS_PER_NIC];  
  uint16_t flow_signature[NUM_RX_THREADS_PER_NIC];
  uint16_t rx_core[NUM_RX_THREADS_PER_NIC];

  union {
    struct {
      unsigned pkt;
      cpu_time_t timer;
      unsigned accum_pkt;
    } ctr;
    char padding[CACHE_LINE_SIZE];
  } recv_counter[NUM_RX_QUEUES];

  union {
    unsigned ctr;
    char padding[CACHE_LINE_SIZE];
  } int_counter[NUM_RX_THREADS_PER_NIC];

  unsigned int_total_counter;
  bool ready[NUM_RX_THREADS_PER_NIC];
  bool is_up;
  bool all_setup_done;
  bool rx_start_ok;


  /**
    *  Send packets using simple tx descriptor. One packet per descriptor. 
    *
    *  @param tx_pkts The pointer for the packet buffer chain.
    *  @param nb_pkts The number of packets to be sent.
    *  @param queue The TX queue.
    *  @return The actual number of packets that have been sent.
    */
  uint16_t send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned queue);

  /**
    *  Send packets using advanced tx descriptor. Each packet can consists of up to 3 segments. IP checksum offload support.
    *
    *  @param tx_pkts The pointer for the packet buffer chain.
    *  @param nb_pkts The number of packets to be sent.
    *  @param queue The TX queue.
    *  @return The actual number of packets that have been sent.
    */
  uint16_t multi_send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned queue);
  inline int xmit_cleanup(struct exo_tx_ring *txq);
  inline void tx4(struct exo_mbuf ** tx_pkts, unsigned queue, unsigned pos);
  inline void tx1(struct exo_mbuf ** tx_pkts, unsigned queue, unsigned pos);
  inline unsigned tx_free_bufs(unsigned queue);
  inline void tx_fill_hw_ring(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned queue);
  
  bool msix_support();
  void init_device();
  bool nic_configure(bool kvcache_server = true);
  void init_nic_param();
  bool reset_hw();
  bool start_hw();
  void core_configure(bool kvcache_server);
  void up_complete();
  void stop_adapter();
  void clear_tx_pending();
  void clear_hw_counters();
  void setup_rx_threads();
  void configure_msix();
  void configure_pb();
  void set_rx_mode();
  void reg_info();
  void disable_sec_rx_path();
  void enable_sec_rx_path();
  void configure_rx();
  void configure_tx();
  void non_sfp_link_config();
  void irq_enable(bool queues, bool flush);
  void set_ivar(s8 direction, u8 queue, u8 msix_vector);
  void setup_rxpba(unsigned num_pb, uint32_t headroom, int strategy);
  void irq_enable_queues(u64 qmask);
  void check_mac_link(ixgbe_link_speed *speed, bool *link_up, bool link_up_wait);
  void setup_mrqc();
  void configure_rx_ring(struct exo_rx_ring * ring);
  void setup_mtqc();
  void configure_tx_ring(struct exo_tx_ring *ring);
  inline unsigned rx_queue_2_core(unsigned queue);
  void disable_rx_queue(struct exo_rx_ring *ring);
  void rx_desc_queue_enable(struct exo_rx_ring *ring);
  void activate(unsigned num);
  void wait_for_activate();
  void interrupt_handler(unsigned num);
  void reset_rx_desc(unsigned queue, unsigned index, bool new_pkt);
  void setup_msi_interrupt();
  void enable_msi();
  void add_ip(char * ip);
  void parse_ip_format(char *s, uint8_t * ip);
  void filter_setup(bool kvcache_server);
};

#endif // __X540_DEVICE_H__
