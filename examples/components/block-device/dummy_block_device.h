#include <component/block_device_itf.h>

/** 
 * Interface IHello implementation
 * 
 */
class IBlockDevice_impl : public IBlockDevice
{
public:

  // IDeviceControl
  //
  status_t init_device(unsigned instance);
  Exokernel::Device * get_device();
  status_t shutdown_device();

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

};


/** 
 * Definition of the component
 * 
 */
class DummyBlockDeviceComponent : public IBlockDevice_impl                    
{
public:  
  DECLARE_COMPONENT_UUID(0x3aa3813c,0xe0cf,0x46cc,0x8ad9,0x1e74,0x08b9,0xb882);

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

};
