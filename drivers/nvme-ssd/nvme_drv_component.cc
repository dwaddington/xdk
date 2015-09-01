
#include <string.h>
#include <stdio.h>
#include <common/logging.h>
#include <component/base.h>
#include <boost/tokenizer.hpp>

#include "nvme_drv_component.h"

static void split_config_string(std::string op, std::map<std::string, std::string>& result)
{
  using namespace boost;
  using namespace std;

  char_separator<char> sep("&");
  tokenizer<char_separator<char>> tokens(op, sep);

  vector<string> pairs;
  for(const auto& t : tokens) {

    char_separator<char> sep("=");
    tokenizer<char_separator<char>> inner_tokens(t, sep);    
    tokenizer<char_separator<char>>::iterator iter = inner_tokens.begin();
    std::string left = *iter;
    ++iter;
    result[left] = *iter;
  }
}


//////////////////////////////////////////////////////////////////////
// IDeviceControl interface
//
status_t NVME_driver_component::init_device(unsigned instance, const char * config) {

  PLOG("init_device(instance=%u)",instance);

  std::string filename = "config.xml";
  if(config) {
    std::map<std::string, std::string> params;
    split_config_string(std::string(config), params);
    if(!params["file"].empty()) {
      filename = params["file"];
      PLOG("opening config file (%s)", filename.c_str());
    }
  }
  
  try {
    PLOG("instantiating new NVME_device instance");
    _dev = new NVME_device(filename.c_str(), instance); //compatibility
  }
  catch(Exokernel::Exception e) {
    NVME_INFO("EXCEPTION: error in NVME device initialization (%s) \n",e.cause());
    asm("int3");
  }
  catch(Exokernel::Fatal e) {
    NVME_INFO("FATAL: error in NVME device initialization (%s) \n",e.cause());
    asm("int3");
  }
  catch(...) {
    NVME_INFO("EXCEPTION: error in NVME device initialization (unknown exception) \n");
    asm("int3");
  }
  

#ifdef TESTING_ONLY
  /* IRQ will be masked and will need unmasking */
  NVME_INFO("MASKING IRQ!!");
  _dev->irq_set_masking_mode();
#endif



  return S_OK;
}

Exokernel::Device * NVME_driver_component::get_device() {
  return (Exokernel::Device *) _dev;
}

status_t NVME_driver_component::shutdown_device() {

  if(_dev) {
    _dev->destroy();
    delete _dev;
    _dev = NULL;
  }

  return S_OK;
}


status_t 
NVME_driver_component::
sync_read_block(void * buffer_virt, /* must be 512 byte aligned */
                addr_t buffer_phys, 
                off_t lba,          /* store offset */
                size_t num_blocks,  /* each block is 512 bytes */
                unsigned port       /* device port */
                )
{
  assert(0);
  // const unsigned qid = port; /* do we want to change IBlockDevice? */

  // /* set up call back */
  // _dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
  //                                                            (void*)&nobj);

  // uint16_t cid = _dev->block_async_read(qid,
  //                                       buffer_phys,
  //                                       lba,
  //                                       num_blocks); /* num blocks */
  // nobj.set_when(cid);
  // nobj.wait();

  return E_NOT_IMPL;
}


status_t 
NVME_driver_component::
sync_write_block(void * buffer_virt, /* must be 512 byte aligned */
                 addr_t buffer_phys, 
                 off_t lba,          /* logical block offset */
                 size_t num_blocks,  /* each block is 512 bytes */
                 unsigned port       /* device port */
                 )
{
  assert(0);
  // const unsigned qid = port; /* do we want to change IBlockDevice? */

  // Notify_object nobj;

  // /* set up call back */
  // _dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
  //                                                            (void*)&nobj);

  // uint16_t cid = _dev->block_async_write(qid,
  //                                        buffer_phys,
  //                                        lba,
  //                                        num_blocks); /* num blocks */
  // nobj.set_when(cid);
  // nobj.wait();

  return E_NOT_IMPL;
}


/* new sync I/O */
status_t
NVME_driver_component::
sync_io(io_request_t io_request,
        unsigned port)
{
  Notify_Sync nobj;
  async_io(io_request, port);

  nobj.wait();

  return S_OK;
}


/* async I/O */
status_t
NVME_driver_component::
async_io(io_request_t io_request,
         unsigned port)
{
  _dev->async_io_batch(port /* same as queue */,
                       &io_request,
                       1);

  return S_OK;
}

/* async I/O batch */
status_t
NVME_driver_component::
async_io_batch(io_request_t* io_requests,
               size_t length,
               unsigned port)
{
  _dev->async_io_batch(port /* same as queue */,
                       io_requests, 
                       length);

  return S_OK;
}


status_t
NVME_driver_component::
wait_io_completion(unsigned port)
{
  _dev->wait_io_completion(port /* same as queue */);
  return S_OK;
}



status_t
NVME_driver_component::
flush(unsigned nsid, unsigned port)
{
  _dev->flush(nsid, port /* queue */);
  return S_OK;
}



extern "C" void * factory_createInstance(Component::uuid_t& component_id)
{
  if(component_id == NVME_driver_component::component_id()) {
    printf("Creating 'NVME_driver_component'.\n");
    return static_cast<void*>(static_cast<Component::IBase *>(new NVME_driver_component()));
  }
  else return NULL;
}



