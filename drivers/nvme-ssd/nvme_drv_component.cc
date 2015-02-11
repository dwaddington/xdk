
#include <string.h>
#include <stdio.h>
#include <common/logging.h>
#include <component/base.h>

#include "nvme_drv_component.h"

//////////////////////////////////////////////////////////////////////
// IDeviceControl interface
//
status_t NVME_driver_component::init_device(unsigned instance) {
  PLOG("init_device(instance=%u)",instance);
  
  try {
    _dev = new NVME_device("config.xml");
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
  const unsigned qid = port; /* do we want to change IBlockDevice? */

  Notify_object nobj;

  /* set up call back */
  _dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                             (void*)&nobj);

  uint16_t cid = _dev->block_async_read(qid,
                                        buffer_phys,
                                        lba,
                                        num_blocks); /* num blocks */
  nobj.set_when(cid);
  nobj.wait();

  return S_OK;
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
  const unsigned qid = port; /* do we want to change IBlockDevice? */

  Notify_object nobj;

  /* set up call back */
  _dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                             (void*)&nobj);

  uint16_t cid = _dev->block_async_write(qid,
                                         buffer_phys,
                                         lba,
                                         num_blocks); /* num blocks */
  nobj.set_when(cid);
  nobj.wait();

  return S_OK;
}

/*async apis*/

status_t
NVME_driver_component::
async_read_block(void * buffer_virt, /* must be 512 byte aligned */
                addr_t buffer_phys,
                off_t lba,          /* store offset */
                size_t num_blocks,  /* each block is 512 bytes */
                unsigned port,      /* device port */
                uint16_t *cid
                )
{
  const unsigned qid = port; /* do we want to change IBlockDevice? */

  Notify_object nobj;

  *cid = _dev->block_async_read(qid,
                                        buffer_phys,
                                        lba,
                                        num_blocks); /* num blocks */
  return S_OK;
}

status_t
NVME_driver_component::
async_write_block(void * buffer_virt, /* must be 512 byte aligned */
                 addr_t buffer_phys,
                 off_t lba,          /* logical block offset */
                 size_t num_blocks,  /* each block is 512 bytes */
                 unsigned port,       /* device port */
                 uint16_t *cid
                 )
{
  const unsigned qid = port; /* do we want to change IBlockDevice? */

  Notify_object nobj;

  *cid = _dev->block_async_write(qid,
                                         buffer_phys,
                                         lba,
                                         num_blocks); /* num blocks */
  return S_OK;
}



extern "C" void * factory_createInstance(Component::uuid_t& component_id)
{
  if(component_id == NVME_driver_component::component_id()) {
    printf("Creating 'NVME_driver_component'.\n");
    return static_cast<void*>(new NVME_driver_component());
  }
  else return NULL;
}



