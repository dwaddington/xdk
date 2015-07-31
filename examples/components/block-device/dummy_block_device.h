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
  status_t init_device(unsigned instance,  config_t config);
  Exokernel::Device * get_device();
  status_t shutdown_device();

  // IBlockData
  //
  //new sync read
  status_t sync_io(io_request_t io_request,
                           unsigned port,
                           unsigned device=0
                           ) { return S_OK; }

  status_t async_io(io_request_t io_request,
                            notify_t notify,
                            unsigned port,
                            unsigned device=0
                            ) { return S_OK; }

  /* async batch I/O operation*/
  status_t async_io_batch(io_request_t* io_requests,
                          size_t length,
                          notify_t notify,
                          unsigned port,
                          unsigned device=0
                          ) { return S_OK; }


  status_t wait_io_completion(unsigned port,
                      unsigned device=0
                      ) { return S_OK; }

  status_t flush(unsigned nsid,
                 unsigned port,
                 unsigned device=0
                 ) { return S_OK; }


};


/** 
 * Definition of the component
 * 
 */
class DummyBlockDeviceComponent : public IBlockDevice_impl                    
{
public:  
  DECLARE_COMPONENT_UUID(0x3aa3813c,0xe0cf,0x46cc,0x8ad9,0x1e,0x74,0x08,0xb9,0xb8,0x82);

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

  DUMMY_IBASE_CONTROL;

};
