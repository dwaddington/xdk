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


#ifndef __XML_CONFIG_PARSER_H__
#define __XML_CONFIG_PARSER_H__

#include <tinyxml.h>

#include <set>
#include <string>

/** Container of configuration information. */
struct Config_params {
  unsigned nic_num;
  unsigned channels_per_nic;
  unsigned cpus_per_nic;
  unsigned shm_max_size;
  unsigned rx_desc_per_queue;
  unsigned tx_desc_per_queue;
  unsigned frame_num_per_core;
  unsigned jd_num_per_core;
  unsigned mbuf_num_per_core;
  unsigned pbuf_num_per_core;
  unsigned net_header_num_per_core;
  unsigned udp_pcb_num_per_core;
  unsigned ip_reass_num_per_core;
  unsigned meta_data_num_per_core;
  unsigned channel_size;
  unsigned eviction_period;
  unsigned request_rate;
  unsigned stats_num;
  unsigned client_rx_flow_num;
  unsigned flex_byte_pos;
  unsigned server_timestamp;
  unsigned server_port;
  std::string tx_threads_cpu_mask;
  std::string rx_threads_cpu_mask;
  std::string server_ip[4];
};


/** Parser of XML configuration file. */
class XML_config_parser {

public:

//  XML_config_parser(const std::string& filename);
/** 
 * Parses the XML configuration file and populates the parameters data
 * structure.  
 */
static 
void parse(const std::string& filename, 
           Config_params& params,
           bool verbose = false);

//TODO
static
unsigned get_uint(const std::string var_name) 
{return 0;}

static
std::string get_string(const std::string var_name) 
{return var_name;}

private:

  // Private default constructor. 
  XML_config_parser() {}

  // Private destructor. 
  virtual ~XML_config_parser() {} 

	//TiXmlHandle _root_hdl;
};
#endif


