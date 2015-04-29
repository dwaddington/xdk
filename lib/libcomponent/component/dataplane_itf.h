#ifndef __DATAPLANE_ITF_H__
#define __DATAPLANE_ITF_H__

#include <common/types.h>

class IBlockData {
public:

  /** 
   * Synchronously read/write a block of data
   * 
   * @param io_request Opaque pointer to an IO operation
   * @param port Device port to write to
   * @param device Device instance
   * 
   * @return S_OK on success.
   */
  virtual status_t sync_io(io_request_t io_request,
                           unsigned port,
                           unsigned device=0
                           ) = 0;


  /** 
   * Asynchronously read/write a block of data
   * 
   * @param io_request 
   * @param notify 
   * @param port 
   * @param device 
   * 
   * @return S_OK on success.
   */
  virtual status_t async_io(io_request_t io_request,
                            notify_t notify,
                            unsigned port,
                            unsigned device=0
                            ) = 0;

  /** 
   * Asynchronously issue a batch (group) of IO operations.
   * 
   * @param io_requests Array of opaque IO request pointers
   * @param length Number of requests
   * @param notify Notify object
   * @param port Device port to issue request to
   * @param device Device instance to issue request to
   * 
   * @return S_OK on success.
   */
  virtual status_t async_io_batch(io_request_t* io_requests,
                                  size_t length,
                                  notify_t notify,
                                  unsigned port,
                                  unsigned device=0
                                  ) = 0;

  /** 
   * Wait for all I/O operations to complete
   * 
   * @param port Device port
   * @param device Device instance
   * 
   * @return S_OK on success.
   */
  virtual status_t wait_io_completion(unsigned port,
                                      unsigned device=0
                                     ) = 0;

  /** 
   * Flush IO device
   * 
   * @param nsid Namespace identifier
   * @param port Device port
   * @param device Device identifier
   * 
   * @return S_OK on success.
   */
  virtual status_t flush(unsigned nsid,
                         unsigned port,
                         unsigned device=0
                         ) = 0;

};

#endif // __DATAPLANE_ITF_H__
