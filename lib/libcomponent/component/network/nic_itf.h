/* 
   LICENCE TO BE APPENDED
*/

#ifndef _NIC_ITF_H_
#define _NIC_ITF_H_

#include <component.h>
#include <interface.h>

using namespace Exokernel;

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
class INic : public Exokernel::Interface_base
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
