#ifndef __NVME_DRV_COMPONENT_H__
#define __NVME_DRV_COMPONENT_H__

#include <component/block_device_itf.h>
#include "nvme_device.h"

class NVME_driver_component : public IBlockDevice
{
private:
  NVME_device * _dev;

public:  
  DECLARE_COMPONENT_UUID(0x32211000,0xe72d,0x4ddd,0x34d9,0x3e,0x44,0x20,0x15,0x20,0x15);

  NVME_driver_component() : _dev(NULL) {
  }

  virtual ~NVME_driver_component() {
  }

public:
  void * query_interface(Component::uuid_t& itf_uuid) {
    if(itf_uuid == IBlockDevice::iid()) {
      add_ref(); // implicitly add reference
      return (void *) static_cast<IBlockDevice *>(this);
    }
    else 
      return NULL; // we don't support this interface
  };

  void unload() {
    printf("Unloading component.\n");
    delete this;
  }

  // IDeviceControl
  //

  /** 
   * Initialize device
   * 
   * @param instance Device instance counting from 0 (determined by PCI order)
   * @param config Configuration string (e.g., file=config.xml&param2=foobar)
   * 
   * @return S_OK on success
   */
  status_t init_device(unsigned instance, const char * config);


  Exokernel::Device * get_device();
  status_t shutdown_device();

  DUMMY_IBASE_CONTROL;

  // IBlockData
  //

  /** 
   * Synchronously (block until completion) read N blocks
   * 
   * @param buffer_virt Pointer to DMA buffer (must be device block size (e.g., 4K) aligned)
   * @param buffer_phys Physical address of DMA buffer (must be device block size (e.g., 4K) aligned)
   * @param offset Offset in blocks 
   * @param num_blocks Number of blocks to read
   * @param port Port/queue to use
   * 
   * @return S_OK on success
   */
  status_t sync_read_block(void * buffer_virt, 
                           addr_t buffer_phys, 
                           off_t offset,       
                           size_t num_blocks,  
                           unsigned port       
                           );

  /** 
   * Synchronously (block until completion) write N blocks
   * 
   * @param buffer_virt Pointer to DMA buffer (must be device block size (e.g., 4K) aligned)
   * @param buffer_phys Physical address of DMA buffer (must be device block size (e.g., 4K) aligned)
   * @param offset Offset in blocks 
   * @param num_blocks Number of blocks to read
   * @param port Port/queue to use
   * 
   * @return S_OK on success
   */  
  status_t sync_write_block(void * buffer_virt, 
                            addr_t buffer_phys, 
                            off_t offset,       
                            size_t num_blocks,  
                            unsigned port       
                            );

  /** 
   * Generalized non-batching synchronous IO
   * 
   * @param io_request IO request
   * @param port Queue/port
   * @param device Device instance
   * 
   * @return S_OK on success
   */
  status_t sync_io(io_request_t io_request,
                   unsigned port);


  status_t async_io(io_request_t io_request,
                    unsigned port);

  status_t async_io_batch(io_request_t* io_requests,
                          size_t length,
                          unsigned port);

  status_t wait_io_completion(unsigned port);

  status_t flush(unsigned nsid,
                 unsigned port);


};






#endif // __NVME_DRV_COMPONENT_H__
