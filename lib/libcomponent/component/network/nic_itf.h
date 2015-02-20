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

#ifndef _NIC_ITF_H_
#define _NIC_ITF_H_

#include "../base.h"
#include "../interface.h"
#ifdef DATAHAWK
  #include <connection.h>
#endif

using namespace Component;

/**
 * NIC component state type.
 */
enum {
  NIC_INIT_STATE = 0,    /**< The init state. */
  NIC_CREATED_STATE = 1, /**< The state when driver object is instantiated. */
  NIC_TX_DONE_STATE = 2, /**< The state when TX unit routine is done. */
  NIC_RX_DONE_STATE = 3, /**< The state when RX unit routine is done. */
  NIC_READY_STATE = 4,   /**< The state when NIC component is up running. */
};

/**
 * NIC component init argument struct.
 */
typedef struct {
  params_config_t params;    /**< The params config struct */
} nic_arg_t;

/** 
 * Interface definition for INic.
 * 
 */
class INic : public Component::Interface_base
{
public:
  DECLARE_INTERFACE_UUID(0x211dc467,0x81a0,0x4061,0x90e1,0x8ba7,0x5412,0x03fd);

  /**
   * To send a burst of packets out with simple path. No offloading feature. No segments.
   *
   * @param p Pointer to an array of packet buffer struct. Each struct contains a single packet frame.
   * @param cnt The number of packets to be sent. The acutal sent number will be returned.
   * @param device The NIC identifier.
   * @param queue The TX queue where the packets are sent.
   * @return The return status.
   */
  virtual status_t send_packets_simple(pkt_buffer_t* p, size_t& cnt, unsigned device, unsigned queue) = 0;

    /**
   * To send a burst of packets out with full-featured path. Support offloading and segments.
   *
   * @param p Pointer to an array of packet buffer struct. Each struct contains a single packet frame.
   * @param cnt The number of packets to be sent. The acutal sent number will be returned.
   * @param device The NIC identifier.
   * @param queue The TX queue where the packets are sent.
   * @return The return status.
   */
  virtual status_t send_packets(pkt_buffer_t* p, size_t& cnt, unsigned device, unsigned queue) = 0;

  /**
   * To obtain the device driver handle.
   *
   * @param device The NIC identifier.
   * @return The pointer to the device driver object.
   */
  virtual device_handle_t driver(unsigned device) = 0;
};

#endif
