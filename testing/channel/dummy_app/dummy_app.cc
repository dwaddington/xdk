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


#include <common/types.h>
#include <numa_channel/shm_channel.h>
#include <pthread.h>
#include <exo/shm_table.h>
#include <libexo.h>
#include <network/memory_itf.h>
#include <xml_config_parser.h>

#define APP_THREAD_CPU_MASK         (0x12)
#define CHANNEL_SIZE (1024)

using namespace std;
using namespace Component;
using namespace Exokernel;
using namespace Exokernel::Memory;

unsigned num_app_thread;
unsigned num_app_thread_per_nic;

typedef Shm_channel<char*, CHANNEL_SIZE, SPMC_circular_buffer> Shm_channel_t;
typedef char* item_t;

struct thread_info {
  unsigned core;
  unsigned local_id;
  unsigned global_id;
  pthread_t tid;
  unsigned channels_per_nic;
  unsigned nic_num;
};

void * app_thread(void * arg) {
  struct thread_info * tinfo = (struct thread_info *) arg;
  assert(tinfo != NULL);
  unsigned channel_id = tinfo->global_id;
  unsigned nic_idx = channel_id/(tinfo->channels_per_nic);
  unsigned nic_num = tinfo->nic_num;

  /* Create app side channel */
  Shm_channel_t * app_side_channel = new Shm_channel_t(channel_id, false, nic_idx);

  item_t* item; 
  unsigned count = 0;

  while (1) {
    while (app_side_channel->consume(&item) != E_SPMC_CIRBUFF_OK) {
      cpu_relax();   
    }
    count++;
    //printf("[Dummy App %d] Consumed an item at %p count = %u\n",tinfo->global_id, item, count);

    //calculate address delta based on allocator type. Refer to memory_itf.h for the allocator index;
    //item_t* my_item = (item_t*)((addr_t)item + mapped_delta[nic_idx][PBUF_ALLOCATOR]);

    ///////////////////////////////////////////////////
    //DUMMY APPLICATION LOGIC GOES HERE
    ///////////////////////////////////////////////////

    while (app_side_channel->produce(item) != E_SPMC_CIRBUFF_OK) {
      cpu_relax();
    }
    //printf("[Dummy APP %d] Produced an item! count = %u\n", tinfo->global_id, count);
  }
}

int main(int argc, char* argv[]) {

  if (argc != 2) {
    std::cout << "USAGE: " << argv[0] << " XML_CONFIG_FILE" << std::endl;
    exit(-1);
  }

  Config_params params;
  try {
    XML_config_parser::parse(argv[1], params, false);
  }
  catch (Exokernel::Exception& e) {
    std::cout << "ERROR: " << e.cause() << std::endl;
    exit(-1);
  }

  unsigned nic_num = params.nic_num;
  unsigned channels_per_nic = params.channels_per_nic;
  unsigned cpus_per_nic = params.cpus_per_nic;

  num_app_thread = nic_num * channels_per_nic;
  num_app_thread_per_nic = channels_per_nic;

  unsigned app_core[nic_num][num_app_thread_per_nic]; // app thread cpu mapping

  //obtain NUMA cpu mask 
  struct bitmask * cpumask[nic_num];
  //unsigned nodes = numa_max_node();

  for (unsigned i = 0; i < nic_num; i++) {
    cpumask[i] = numa_allocate_cpumask();
    numa_node_to_cpus(i, cpumask[i]);
    printf("[NODE %u] CPU MASK %lx\n",i, *(cpumask[i]->maskp));
  }

  /* derive cpus for APP threads from APP_THREAD_CPU_MASK */
  for (unsigned j = 0; j < nic_num; j++) {
    for (unsigned i = 0; i < num_app_thread_per_nic; i++) {
      app_core[j][i] = 0;
    }
  
    uint16_t pos = 1;
    unsigned n = 0;
    for (unsigned i = 0; i < cpus_per_nic; i++) {
      pos = 1 << i;
      if ((APP_THREAD_CPU_MASK & pos) != 0) {
        app_core[j][n] = get_cpu_id(cpumask[j],i+1);
        n++;
      }
    }
  }

  for (unsigned j = 0; j < nic_num; j++) {
    for (unsigned i = 0; i < num_app_thread_per_nic; i++)
      printf("app_core[%u][%u] = %u\n",j,i,app_core[j][i]);
  }

  cpu_set_t cpuset;
  pthread_t tid[num_app_thread];
  pthread_attr_t attr[num_app_thread];
  struct thread_info tinfo[num_app_thread];

  for (unsigned j = 0; j < nic_num; j++) {
    for (unsigned i = 0; i < num_app_thread_per_nic; i++) {
      CPU_ZERO(&cpuset);
      CPU_SET(app_core[j][i], &cpuset);

      unsigned gid = j * num_app_thread_per_nic + i;
      // Create and start a new thread.
      if (pthread_attr_init(&attr[gid]) != 0) {
        std::cerr << "pthread_attr_init(...) failed." << std::endl;
        exit(1);
      }
  
      if (pthread_attr_setaffinity_np(&attr[gid], sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "pthread_attr_setaffinity_np(...) failed." << std::endl;
        exit(1);
      }
  
      tinfo[gid].core = app_core[j][i];
      tinfo[gid].local_id = i;
      tinfo[gid].global_id = gid;
      tinfo[gid].nic_num = nic_num;
      tinfo[gid].channels_per_nic = channels_per_nic;

      int err = -1;
    
      err = pthread_create(&(tinfo[gid].tid), &attr[gid], app_thread, (void*)(&tinfo[gid]));
      if (err == 0) {
        std::cout << "pthread_create(...) created APP thread "<< tinfo[gid].global_id
                  << " on Core #" << tinfo[gid].core << std::endl;
      }
      else {
        std::cerr << "pthread_create(...) failed"<< std::endl;
        exit(1);
      }
    }
  }
  sleep(10000); 
  return 0;
}
