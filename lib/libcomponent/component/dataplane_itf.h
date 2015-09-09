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
                           unsigned port
                           ) = 0;


  /** 
   * Asynchronously read/write a block of data
   * 
   * @param io_request Generalized IO request
   * @param port Port/queue to issue request to
   * 
   * @return S_OK on success.
   */
  virtual status_t async_io(io_request_t io_request,
                            unsigned port
                            ) = 0;

  /** 
   * Asynchronously issue a batch (group) of IO operations.
   * 
   * @param io_requests Array of opaque IO request pointers
   * @param length Number of requests
   * @param port Device port to issue request to
   * 
   * @return S_OK on success.
   */
  virtual status_t async_io_batch(io_request_t* io_requests,
                                  size_t length,
                                  unsigned port
                                  ) = 0;

  /** 
   * Wait for all I/O operations to complete
   * 
   * @param port Device port
   * 
   * @return S_OK on success.
   */
  virtual status_t wait_io_completion(unsigned port) = 0;

  /** 
   * Flush IO device
   * 
   * @param nsid Namespace identifier
   * @param port Device port
   * 
   * @return S_OK on success.
   */
  virtual status_t flush(unsigned nsid,
                         unsigned port
                         ) = 0;

};

#endif // __DATAPLANE_ITF_H__
