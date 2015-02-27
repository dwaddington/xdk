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

/** 
 * @author Juan A. Colmenares <juan.col@samsung.com>
 * @date March 31, 2014.
 */

#include "xml_config_parser.h" 
#include <stdlib.h>
#include <iostream>
#include <set>
#include <string>
#include <sstream>
#include <tinyxml.h>
#include <exo/errors.h>

/** Converts a string representation of number to its actual numerical value. */
template< typename T >
inline T str_to_num(const std::string& str) {
  std::istringstream iss(str);
  T obj;

  iss >> std::ws >> obj >> std::ws;

  if(!iss.eof()) {
    throw Exokernel::Exception("string-to-number convertion failed!");
  }

  return obj; 
}



static const std::string ROOT_ELEM_STR = "x540_driver";

void
XML_config_parser::parse(const std::string& filename, 
                         Config_params& params,
                         bool verbose) {
  
  TiXmlDocument doc;

  if (!doc.LoadFile(filename)) {
    std::stringstream ss;
    ss << "Couldn't open configuration file: " << filename;
    throw Exokernel::Exception(ss.str().c_str());
  }

  TiXmlHandle doc_hdl(&doc); 
  TiXmlElement* root = doc_hdl.FirstChildElement(ROOT_ELEM_STR).ToElement(); 
  if (root == NULL) {
    std::stringstream ss;
    ss << "XML root element is not " << ROOT_ELEM_STR;
    throw Exokernel::Exception(ss.str().c_str());
  }

  
  TiXmlHandle root_hdl(root); 
  
  // NIC_NUM  
  TiXmlElement* nic_num_elem = root_hdl.FirstChild("nic_num").ToElement();
  if (nic_num_elem == NULL) {
    throw Exokernel::Exception("Didn't specify NIC_NUM.");
  }

  if (verbose) {
    std::cout << "NIC_NUM: " << nic_num_elem->GetText() << std::endl;
  }

  unsigned nic_num = str_to_num<unsigned>(nic_num_elem->GetText());
  if (nic_num <= 0) {
    throw Exokernel::Exception("Invalid value of 'nic_num'.");
  } 

  // CHANNELS_PER_NIC
  TiXmlElement* channels_per_nic_elem = root_hdl.FirstChild("channels_per_nic").ToElement();
  if (channels_per_nic_elem == NULL) {
    throw Exokernel::Exception("Didn't specify CHANNELS_PER_NIC.");
  }

  if (verbose) {
    std::cout << "CHANNELS_PER_NIC: " << channels_per_nic_elem->GetText() << std::endl;
  }

  unsigned channels_per_nic = str_to_num<unsigned>(channels_per_nic_elem->GetText());
  if (channels_per_nic <= 0) {
    throw Exokernel::Exception("Invalid value of 'channels_per_nic'.");
  }

  // CPUS_PER_NIC
  TiXmlElement* cpus_per_nic_elem = root_hdl.FirstChild("cpus_per_nic").ToElement();
  if (cpus_per_nic_elem == NULL) {
    throw Exokernel::Exception("Didn't specify CPUS_PER_NIC.");
  }

  if (verbose) {
    std::cout << "CPUS_PER_NIC: " << cpus_per_nic_elem->GetText() << std::endl;
  }

  unsigned cpus_per_nic = str_to_num<unsigned>(cpus_per_nic_elem->GetText());
  if (cpus_per_nic <= 0) {
    throw Exokernel::Exception("Invalid value of 'cpus_per_nic'.");
  }

  // SHM_MAX_SIZE
  TiXmlElement* shm_max_size_elem = root_hdl.FirstChild("shm_max_size").ToElement();
  if (shm_max_size_elem == NULL) {
    throw Exokernel::Exception("Didn't specify SHM_MAX_SIZE.");
  }

  if (verbose) {
    std::cout << "SHM_MAX_SIZE: " << shm_max_size_elem->GetText() << std::endl;
  }

  unsigned shm_max_size = str_to_num<unsigned>(shm_max_size_elem->GetText());
  if (shm_max_size <= 0) {
    throw Exokernel::Exception("Invalid value of 'shm_max_size'.");
  }  

  // TX_DESC_PER_QUEUE
  TiXmlElement* tx_desc_per_queue_elem = root_hdl.FirstChild("tx_desc_per_queue").ToElement();
  if (tx_desc_per_queue_elem == NULL) {
    throw Exokernel::Exception("Didn't specify TX_DESC_PER_QUEUE.");
  }

  if (verbose) {
    std::cout << "TX_DESC_PER_QUEUE: " << tx_desc_per_queue_elem->GetText() << std::endl;
  }

  unsigned tx_desc_per_queue = str_to_num<unsigned>(tx_desc_per_queue_elem->GetText());
  if (tx_desc_per_queue <= 0) {
    throw Exokernel::Exception("Invalid value of 'tx_desc_per_queue'.");
  }


  // RX_DESC_PER_QUEUE
  TiXmlElement* rx_desc_per_queue_elem = root_hdl.FirstChild("rx_desc_per_queue").ToElement();
  if (rx_desc_per_queue_elem == NULL) {
    throw Exokernel::Exception("Didn't specify RX_DESC_PER_QUEUE.");
  }

  if (verbose) {
    std::cout << "RX_DESC_PER_QUEUE: " << rx_desc_per_queue_elem->GetText() << std::endl;
  }

  unsigned rx_desc_per_queue = str_to_num<unsigned>(rx_desc_per_queue_elem->GetText());
  if (rx_desc_per_queue <= 0) {
    throw Exokernel::Exception("Invalid value of 'rx_desc_per_queue'.");
  }

  // FRAME_NUM_PER_CORE
  TiXmlElement* frame_num_per_core_elem = root_hdl.FirstChild("frame_num_per_core").ToElement();
  if (frame_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify FRAME_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "FRAME_NUM_PER_CORE: " << frame_num_per_core_elem->GetText() << std::endl;
  }

  unsigned frame_num_per_core = str_to_num<unsigned>(frame_num_per_core_elem->GetText());
  if (frame_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'frame_num_per_core'.");
  }

  // JD_NUM_PER_CORE
  TiXmlElement* jd_num_per_core_elem = root_hdl.FirstChild("jd_num_per_core").ToElement();
  if (jd_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify JD_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "JD_NUM_PER_CORE: " << jd_num_per_core_elem->GetText() << std::endl;
  }

  unsigned jd_num_per_core = str_to_num<unsigned>(jd_num_per_core_elem->GetText());
  if (jd_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'jd_num_per_core'.");
  }

  // MBUF_NUM_PER_CORE
  TiXmlElement* mbuf_num_per_core_elem = root_hdl.FirstChild("mbuf_num_per_core").ToElement();
  if (mbuf_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify MBUF_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "MBUF_NUM_PER_CORE: " << mbuf_num_per_core_elem->GetText() << std::endl;
  }

  unsigned mbuf_num_per_core = str_to_num<unsigned>(mbuf_num_per_core_elem->GetText());
  if (mbuf_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'mbuf_num_per_core'.");
  }

  // META_DATA_NUM_PER_CORE
  TiXmlElement* meta_data_num_per_core_elem = root_hdl.FirstChild("meta_data_num_per_core").ToElement();
  if (meta_data_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify META_DATA_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "META_DATA_NUM_PER_CORE: " << meta_data_num_per_core_elem->GetText() << std::endl;
  }

  unsigned meta_data_num_per_core = str_to_num<unsigned>(meta_data_num_per_core_elem->GetText());
  if (meta_data_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'meta_data_num_per_core'.");
  }

  // PBUF_NUM_PER_CORE
  TiXmlElement* pbuf_num_per_core_elem = root_hdl.FirstChild("pbuf_num_per_core").ToElement();
  if (pbuf_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify PBUF_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "PBUF_NUM_PER_CORE: " << pbuf_num_per_core_elem->GetText() << std::endl;
  }

  unsigned pbuf_num_per_core = str_to_num<unsigned>(pbuf_num_per_core_elem->GetText());
  if (pbuf_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'pbuf_num_per_core'.");
  }

  // NET_HEADER_NUM_PER_CORE
  TiXmlElement* net_header_num_per_core_elem = root_hdl.FirstChild("net_header_num_per_core").ToElement();
  if (net_header_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify NET_HEADER_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "NET_HEADER_NUM_PER_CORE: " << net_header_num_per_core_elem->GetText() << std::endl;
  }

  unsigned net_header_num_per_core = str_to_num<unsigned>(net_header_num_per_core_elem->GetText());
  if (net_header_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'net_header_num_per_core'.");
  }

  // IP_REASS_NUM_PER_CORE
  TiXmlElement* ip_reass_num_per_core_elem = root_hdl.FirstChild("ip_reass_num_per_core").ToElement();
  if (ip_reass_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify IP_REASS_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "IP_REASS_NUM_PER_CORE: " << ip_reass_num_per_core_elem->GetText() << std::endl;
  }

  unsigned ip_reass_num_per_core = str_to_num<unsigned>(ip_reass_num_per_core_elem->GetText());
  if (ip_reass_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'ip_reass_num_per_core'.");
  }

  // UDP_PCB_NUM_PER_CORE
  TiXmlElement* udp_pcb_num_per_core_elem = root_hdl.FirstChild("udp_pcb_num_per_core").ToElement();
  if (udp_pcb_num_per_core_elem == NULL) {
    throw Exokernel::Exception("Didn't specify UDP_PCB_NUM_PER_CORE.");
  }

  if (verbose) {
    std::cout << "UDP_PCB_NUM_PER_CORE: " << udp_pcb_num_per_core_elem->GetText() << std::endl;
  }

  unsigned udp_pcb_num_per_core = str_to_num<unsigned>(udp_pcb_num_per_core_elem->GetText());
  if (udp_pcb_num_per_core <= 0) {
    throw Exokernel::Exception("Invalid value of 'udp_pcb_num_per_core'.");
  }

  // CHANNEL_SIZE
  TiXmlElement* channel_size_elem = root_hdl.FirstChild("channel_size").ToElement();
  if (channel_size_elem == NULL) {
    throw Exokernel::Exception("Didn't specify CHANNEL_SIZE.");
  }

  if (verbose) {
    std::cout << "CHANNEL_SIZE: " << channel_size_elem->GetText() << std::endl;
  }

  unsigned channel_size = str_to_num<unsigned>(channel_size_elem->GetText());
  if (channel_size <= 0) {
    throw Exokernel::Exception("Invalid value of 'channel_size'.");
  }

  // REQUEST_RATE
  TiXmlElement* request_rate_elem = root_hdl.FirstChild("request_rate").ToElement();
  if (request_rate_elem == NULL) {
    throw Exokernel::Exception("Didn't specify REQUEST_RATE.");
  }

  if (verbose) {
    std::cout << "REQUEST_RATE: " << request_rate_elem->GetText() << std::endl;
  }

  unsigned request_rate = str_to_num<unsigned>(request_rate_elem->GetText());
  if (request_rate <= 0) {
    throw Exokernel::Exception("Invalid value of 'request_rate'.");
  }

  // TX_THREADS_CPU_MASK
  TiXmlElement* tx_threads_cpu_mask_elem = root_hdl.FirstChild("tx_threads_cpu_mask").ToElement();
  if (tx_threads_cpu_mask_elem == NULL) {
    throw Exokernel::Exception("Didn't specify TX_THREADS_CPU_MASK.");
  }

  if (verbose) {
    std::cout << "TX_THREADS_CPU_MASK: " << tx_threads_cpu_mask_elem->GetText() << std::endl;
  }

  std::string tx_threads_cpu_mask = tx_threads_cpu_mask_elem->GetText();

  // RX_THREADS_CPU_MASK
  TiXmlElement* rx_threads_cpu_mask_elem = root_hdl.FirstChild("rx_threads_cpu_mask").ToElement();
  if (rx_threads_cpu_mask_elem == NULL) {
    throw Exokernel::Exception("Didn't specify RX_THREADS_CPU_MASK.");
  }

  if (verbose) {
    std::cout << "RX_THREADS_CPU_MASK: " << rx_threads_cpu_mask_elem->GetText() << std::endl;
  }

  std::string rx_threads_cpu_mask = rx_threads_cpu_mask_elem->GetText();

  // STATS_NUM
  TiXmlElement* stats_num_elem = root_hdl.FirstChild("stats_num").ToElement();
  if (stats_num_elem == NULL) {
    throw Exokernel::Exception("Didn't specify STATS_NUM.");
  }

  if (verbose) {
    std::cout << "STATS_NUM: " << stats_num_elem->GetText() << std::endl;
  }

  unsigned stats_num = str_to_num<unsigned>(stats_num_elem->GetText());
  if (stats_num <= 0) {
    throw Exokernel::Exception("Invalid value of 'stats_num'.");
  }

  // CLIENT_RX_FLOW_NUM
  TiXmlElement* client_rx_flow_num_elem = root_hdl.FirstChild("client_rx_flow_num").ToElement();
  if (client_rx_flow_num_elem == NULL) {
    throw Exokernel::Exception("Didn't specify CLIENT_RX_FLOW_NUM.");
  }

  if (verbose) {
    std::cout << "CLIENT_RX_FLOW_NUM: " << client_rx_flow_num_elem->GetText() << std::endl;
  }

  unsigned client_rx_flow_num = str_to_num<unsigned>(client_rx_flow_num_elem->GetText());
  if (client_rx_flow_num <= 0) {
    throw Exokernel::Exception("Invalid value of 'client_rx_flow_num'.");
  }

  // FLEX_BYTE_POS
  TiXmlElement* flex_byte_pos_elem = root_hdl.FirstChild("flex_byte_pos").ToElement();
  if (flex_byte_pos_elem == NULL) {
    throw Exokernel::Exception("Didn't specify FLEX_BYTE_POS.");
  }

  if (verbose) {
    std::cout << "FLEX_BYTE_POS: " << flex_byte_pos_elem->GetText() << std::endl;
  }

  unsigned flex_byte_pos = str_to_num<unsigned>(flex_byte_pos_elem->GetText());
  if (flex_byte_pos < 0) {
    throw Exokernel::Exception("Invalid value of 'flex_byte_pos'.");
  }

  // SERVER_TIMESTAMP
  TiXmlElement* server_timestamp_elem = root_hdl.FirstChild("server_timestamp").ToElement();
  if (server_timestamp_elem == NULL) {
    throw Exokernel::Exception("Didn't specify SERVER_TIMESTAMP.");
  }

  if (verbose) {
    std::cout << "SERVER_TIMESTAMP: " << server_timestamp_elem->GetText() << std::endl;
  }

  unsigned server_timestamp = str_to_num<unsigned>(server_timestamp_elem->GetText());
  if (server_timestamp < 0) {
    throw Exokernel::Exception("Invalid value of 'server_timestamp'.");
  }

  // SERVER_PORT
  TiXmlElement* server_port_elem = root_hdl.FirstChild("server_port").ToElement();
  if (server_port_elem == NULL) {
    throw Exokernel::Exception("Didn't specify SERVER_PORT.");
  }

  if (verbose) {
    std::cout << "SERVER_PORT: " << server_port_elem->GetText() << std::endl;
  }

  unsigned server_port = str_to_num<unsigned>(server_port_elem->GetText());
  if (server_port <= 0) {
    throw Exokernel::Exception("Invalid value of 'server_port'.");
  }

  // CLIENT_PORT
  TiXmlElement* client_port_elem = root_hdl.FirstChild("client_port").ToElement();
  if (client_port_elem == NULL) {
    throw Exokernel::Exception("Didn't specify CLIENT_PORT.");
  }

  if (verbose) {
    std::cout << "CLIENT_PORT: " << client_port_elem->GetText() << std::endl;
  }

  unsigned client_port = str_to_num<unsigned>(client_port_elem->GetText());
  if (client_port <= 0) {
    throw Exokernel::Exception("Invalid value of 'client_port'.");
  }

  // SERVER_IP
  for (unsigned i = 0; i < nic_num; i++) {
    char str[64];
    __builtin_memset(str, 0, 64);
    sprintf(str, "server_ip%u", i+1);
    TiXmlElement* server_ip_elem = root_hdl.FirstChild(str).ToElement();
    if (server_ip_elem == NULL) {
      throw Exokernel::Exception("Didn't specify SERVER_IP.");
    }

    if (verbose) {
      std::cout << "SERVER_IP: " << server_ip_elem->GetText() << std::endl;
    }

    params.server_ip[i] = server_ip_elem->GetText();
  }

  // CLIENT_IP
  for (unsigned i = 0; i < nic_num; i++) {
    char str[64];
    __builtin_memset(str, 0, 64);
    sprintf(str, "client_ip%u", i+1);
    TiXmlElement* client_ip_elem = root_hdl.FirstChild(str).ToElement();
    if (client_ip_elem == NULL) {
      throw Exokernel::Exception("Didn't specify CLIENT_IP.");
    }

    if (verbose) {
      std::cout << "CLIENT_IP: " << client_ip_elem->GetText() << std::endl;
    }

    params.client_ip[i] = client_ip_elem->GetText();
  }

  params.nic_num = nic_num;
  params.channels_per_nic = channels_per_nic;
  params.cpus_per_nic = cpus_per_nic;
  params.shm_max_size = shm_max_size;
  params.tx_desc_per_queue = tx_desc_per_queue;
  params.rx_desc_per_queue = rx_desc_per_queue;
  params.frame_num_per_core = frame_num_per_core;
  params.jd_num_per_core = jd_num_per_core;
  params.mbuf_num_per_core = mbuf_num_per_core;
  params.meta_data_num_per_core = meta_data_num_per_core;
  params.pbuf_num_per_core = pbuf_num_per_core;
  params.net_header_num_per_core = net_header_num_per_core;
  params.ip_reass_num_per_core = ip_reass_num_per_core;
  params.udp_pcb_num_per_core = udp_pcb_num_per_core;
  params.channel_size = channel_size;
  params.request_rate = request_rate;
  params.tx_threads_cpu_mask = tx_threads_cpu_mask;
  params.rx_threads_cpu_mask = rx_threads_cpu_mask;
  params.stats_num = stats_num;
  params.client_rx_flow_num = client_rx_flow_num;
  params.flex_byte_pos = flex_byte_pos;
  params.server_timestamp = server_timestamp;
  params.server_port = server_port;
  params.client_port = client_port;
}
