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
  status_t init_device(unsigned instance, config_t config = NULL);
  Exokernel::Device * get_device();
  status_t shutdown_device();

  DUMMY_IBASE_CONTROL;

  // IBlockData
  //
  status_t sync_read_block(void * buffer_virt, /* must be 512 byte aligned */
                           addr_t buffer_phys, 
                           off_t offset,       /* store offset */
                           size_t num_blocks,  /* each block is 512 bytes */
                           unsigned port       /* device port */
                           );
  
  status_t sync_write_block(void * buffer_virt, /* must be 512 byte aligned */
                            addr_t buffer_phys, 
                            off_t offset,       /* store offset */
                            size_t num_blocks,  /* each block is 512 bytes */
                            unsigned port       /* device port */
                            );

  status_t sync_io(io_request_t io_request,
                   unsigned port,
                   unsigned device
                  );

  status_t async_io(io_request_t io_request,
                    notify_t notify,
                    unsigned port,
                    unsigned device
                   );

  status_t async_io_batch(io_request_t* io_requests,
                          size_t length,
                          notify_t notify,
                          unsigned port,
                          unsigned device
                          );

  status_t wait_io_completion(unsigned port,
                              unsigned device
                             );

  status_t flush(unsigned nsid,
                 unsigned port,
                 unsigned device
                 );


};






#endif // __NVME_DRV_COMPONENT_H__
