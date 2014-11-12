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






#include <component.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include "driver_config.h"
#include "raw_device.h"
#include "../../app/kvcache/job_desc.h"

using namespace Exokernel;
using namespace KVcache;

class Nic_comp_thread : public Exokernel::Base_thread {

private:
  volatile bool             _start;
  INic * _nic;

private:

  void * entry(void *) {
    /* initialize nic component */
    nic_arg_t nic_arg;
    nic_arg.num_nics = NIC_NUM;
    nic_arg.num_rx_queue = NUM_RX_QUEUES;
    nic_arg.rx_queue_size = NUM_RX_DESCRIPTORS_PER_QUEUE;
    nic_arg.num_tx_queue = NUM_TX_QUEUES;
    nic_arg.tx_queue_size = NUM_TX_DESCRIPTORS_PER_QUEUE;

    _nic->init((arg_t)&nic_arg);
    _nic->run();
  }

public:
  Nic_comp_thread(INic * nic) : Base_thread(), _nic(nic) {
    _start = false;
    start();
  }

  void activate() {
    _start = true;
  }

  ~Nic_comp_thread() {
    exit_thread();
  }
};

class Mem_comp_thread : public Exokernel::Base_thread {

private:
  volatile bool _start;
  IMem * _mem;

private:

  void * entry(void *) {
    /* initialize memroy component */
    mem_arg_t mem_arg;
    mem_arg.num_allocators = TOTAL_ALLOCATOR_NUM;
    mem_arg.config_list = (alloc_config_t **) malloc(sizeof(alloc_config_t *) * mem_arg.num_allocators);
  
    alloc_config_t alloc_config[mem_arg.num_allocators];
    alloc_config[0].block_size = NUM_TX_QUEUES * NUM_TX_DESCRIPTORS_PER_QUEUE * sizeof(TX_ADV_DATA_DESC);
    alloc_config[0].num_blocks = 2;  // one block for RX DESC and the other for TX DESC
    alloc_config[0].alignment = 64;
    alloc_config[0].allocator_id = DESC_ALLOCATOR;
  
    alloc_config[1].block_size = PKT_MAX_SIZE;
    alloc_config[1].num_blocks = NUM_FRAME_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[1].alignment = 64;
    alloc_config[1].allocator_id = PACKET_ALLOCATOR;
  
    alloc_config[2].block_size = sizeof(struct exo_mbuf);
    alloc_config[2].num_blocks = NUM_EXO_MBUF_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[2].alignment = 64;
    alloc_config[2].allocator_id = MBUF_ALLOCATOR;
  
    alloc_config[3].block_size = sizeof(void *) * IXGBE_TX_MAX_BURST;
    alloc_config[3].num_blocks = NUM_META_DATA_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[3].alignment = 64;
    alloc_config[3].allocator_id = META_DATA_ALLOCATOR;

    alloc_config[4].block_size = sizeof(struct pbuf);
    alloc_config[4].num_blocks = NUM_PBUF_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[4].alignment = 64;
    alloc_config[4].allocator_id = PBUF_ALLOCATOR;

    alloc_config[5].block_size = 64;
    alloc_config[5].num_blocks = NUM_NET_HEADER_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[5].alignment = 64;
    alloc_config[5].allocator_id = NET_HEADER_ALLOCATOR;

    alloc_config[6].block_size = sizeof(struct ip_reassdata);
    alloc_config[6].num_blocks = NUM_IP_REASS_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[6].alignment = 64;
    alloc_config[6].allocator_id = IP_REASS_ALLOCATOR;

    alloc_config[7].block_size = sizeof(struct udp_pcb);
    alloc_config[7].num_blocks = NUM_UDP_PCB_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[7].alignment = 64;
    alloc_config[7].allocator_id = UDP_PCB_ALLOCATOR;

    alloc_config[8].block_size = sizeof(job_descriptor_t);
    alloc_config[8].num_blocks = NUM_JD_PER_CORE * NUM_RX_THREADS_PER_NIC;
    alloc_config[8].alignment = 64;
    alloc_config[8].allocator_id = JOB_DESC_ALLOCATOR;
  
    alloc_config_t ** config_list = mem_arg.config_list;
    for (unsigned i = 0; i < mem_arg.num_allocators; i++) {
      config_list[i] = &alloc_config[i];
    }
    _mem->init((arg_t)&mem_arg);

    _mem->run();
  }

public:
  Mem_comp_thread(IMem * mem) : Base_thread(), _mem(mem) {
    _start = false;
    start();
  }

  void activate() {
    _start = true;
  }
  
  ~Mem_comp_thread() {
    exit_thread();
  }
};

int main() {

  Component_base * nic_comp = load_component("./libcomp_nic.so.1");
  Component_base * stack_comp = load_component("./libcomp_stack.so.1");
  Component_base * mem_comp = load_component("./libcomp_mem.so.1");

  assert(nic_comp);
  assert(stack_comp);
  assert(mem_comp);

  INic * nic = (INic *) nic_comp->query_interface(INic::uuid());
  IStack * stack = (IStack *) stack_comp->query_interface(IStack::uuid());
  IMem * memory = (IMem *) mem_comp->query_interface(IMem::uuid());

  assert(nic);
  assert(stack);
  assert(memory);

  /* perform third party binding */
  nic->bind(stack);
  nic->bind(memory);
  stack->bind(nic);
  stack->bind(memory);
  memory->bind(nic);
  memory->bind(stack);

  /* create nic component thread */
  Nic_comp_thread* nic_comp_thread;
  nic_comp_thread = new Nic_comp_thread(nic);
  assert(nic_comp_thread);
  nic_comp_thread->activate();

  sleep(2);

  /* create mem component thread */
  Mem_comp_thread* mem_comp_thread;
  mem_comp_thread = new Mem_comp_thread(memory);
  assert(mem_comp_thread);
  mem_comp_thread->activate();

  sleep(2);

  /* initialize stack component */
  stack_arg_t stack_arg;
  stack_arg.st = exo_UDP;
  stack->init((arg_t)&stack_arg);
  stack->run();

  while (1) {
    sleep(100);
  }
  return 0;
}

