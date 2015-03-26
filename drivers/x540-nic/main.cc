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

#include <component/base.h>
#include <mem_component.h>
#include <nic_component.h>
#include "x540/x540_device.h"
#include "xml_config_parser.h"

using namespace Exokernel;
using namespace Component;

class Nic_comp_thread : public Exokernel::Base_thread {

private:
  NicComponent * _nic;
  Config_params * _params;

private:

  void * entry(void *) {
    /* initialize nic component */
    nic_arg_t nic_arg;
    nic_arg.params = (params_config_t)_params;
    _nic->init((arg_t)&nic_arg);
    _nic->run();
    return NULL;
  }

public:
  Nic_comp_thread(NicComponent * nic, Config_params * params) : Base_thread(), _nic(nic), _params(params) {
    start();
  }

  ~Nic_comp_thread() {
    exit_thread();
  }
};

class Mem_comp_thread : public Exokernel::Base_thread {

private:
  MemComponent * _mem;
  Config_params * _params;
  unsigned _tx_queues_per_nic;
  unsigned _tx_desc_per_queue;
  unsigned _rx_threads_per_nic;
  unsigned _tx_threads_per_nic;
  unsigned _frame_num_per_core;
  unsigned _mbuf_num_per_core;
  unsigned _meta_data_num_per_core;
  unsigned _net_header_num_per_core;
 
private:

  void * entry(void *) {
    /* initialize memroy component */

    /*
       WARNING: if physical memory address is needed, the following two conditions must be met
       1. Actual block size (block_len + header) must be no larger than a huge page.
       2. Huge page size must be multiple of actual block size or the total needed memory size is less than a huge page.
    */

    mem_arg_t mem_arg;
    mem_arg.num_allocators = 5;
    mem_arg.config_list = (alloc_config_t **) malloc(sizeof(alloc_config_t *) * mem_arg.num_allocators);
  
    alloc_config_t alloc_config[mem_arg.num_allocators];
    alloc_config[0].block_size = _tx_queues_per_nic * _tx_desc_per_queue * sizeof(TX_ADV_DATA_DESC);
    alloc_config[0].num_blocks = 2;  // one block for RX DESC and the other for TX DESC
    alloc_config[0].alignment = 64;
    alloc_config[0].allocator_id = DESC_ALLOCATOR;
  
    alloc_config[1].block_size = PKT_MAX_SIZE;
    alloc_config[1].num_blocks = _frame_num_per_core * _rx_threads_per_nic;
    alloc_config[1].alignment = 64;
    alloc_config[1].allocator_id = PACKET_ALLOCATOR;
  
    alloc_config[2].block_size = sizeof(struct exo_mbuf);
    alloc_config[2].num_blocks = _mbuf_num_per_core * _rx_threads_per_nic;
    alloc_config[2].alignment = 64;
    alloc_config[2].allocator_id = MBUF_ALLOCATOR;
  
    alloc_config[3].block_size = sizeof(void *) * IXGBE_TX_MAX_BURST;
    alloc_config[3].num_blocks = _meta_data_num_per_core * _rx_threads_per_nic;
    alloc_config[3].alignment = 64;
    alloc_config[3].allocator_id = META_DATA_ALLOCATOR;

    alloc_config[4].block_size = 64;
    alloc_config[4].num_blocks = _net_header_num_per_core * _rx_threads_per_nic;
    alloc_config[4].alignment = 64;
    alloc_config[4].allocator_id = NET_HEADER_ALLOCATOR;

    alloc_config_t ** config_list = mem_arg.config_list;
    for (unsigned i = 0; i < mem_arg.num_allocators; i++) {
      config_list[i] = &alloc_config[i];
    }
    mem_arg.params = _params;

    _mem->init((arg_t)&mem_arg);

    _mem->run();

    return NULL;
  }

public:
  Mem_comp_thread(MemComponent * mem, Config_params * params) : Base_thread(), _mem(mem), _params(params) {
    _tx_queues_per_nic = _params->channels_per_nic * 2;
    _tx_desc_per_queue = _params->tx_desc_per_queue;
    _rx_threads_per_nic = _params->channels_per_nic;
    _tx_threads_per_nic = _params->channels_per_nic;
    _frame_num_per_core = _params->frame_num_per_core;
    _mbuf_num_per_core = _params->mbuf_num_per_core;
    _meta_data_num_per_core = _params->meta_data_num_per_core;
    _net_header_num_per_core = _params->net_header_num_per_core;

    start();
  }

  ~Mem_comp_thread() {
    exit_thread();
  }
};

int main(int argc, char* argv[]) {

  /* parse the config xml file */
  if (argc != 2) {
    std::cout << "USAGE: " << argv[0] << " XML_CONFIG_FILE" << std::endl;
    exit(-1);
  }

  Config_params params;
  try {
    XML_config_parser::parse("config.xml", params, true);
  }
  catch (Exokernel::Exception& e) {
    std::cout << "ERROR: " << e.cause() << std::endl;
    exit(-1);
  }

  /* load the components */
  Component::IBase * nic_comp = load_component("./libcomp_nic.so.1", NicComponent::component_id());
  Component::IBase * mem_comp = load_component("./libcomp_mem.so.1", MemComponent::component_id());

  assert(nic_comp);
  assert(mem_comp);

  std::vector<IBase *> components(2);
  components[0] = nic_comp;
  components[1] = mem_comp;
 
  bind(components);

  /* create nic component thread */
  Nic_comp_thread* nic_comp_thread;
  nic_comp_thread = new Nic_comp_thread((Component::NicComponent *)nic_comp,&params);
  assert(nic_comp_thread);

  /* create mem component thread */
  Mem_comp_thread* mem_comp_thread;
  mem_comp_thread = new Mem_comp_thread((Component::MemComponent *)mem_comp,&params);
  assert(mem_comp_thread);

  while (1) {
    sleep(100);
  }
  return 0;
}
