#ifndef __TESTS_H__
#define __TESTS_H__

#include <common/dump_utils.h>
#include "nvme_device.h"

class TestBlockWriter
{
private:
  NVME_device * _dev;
  Notify_object _nobj;
  void * _vbuff;
  addr_t _pbuff;
  unsigned _qid;

public:
  TestBlockWriter(NVME_device * dev, unsigned qid = 1) :
    _dev(dev), _qid(qid)
  {
    assert(dev);

    /* set up call back */
    dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                              (void*)&_nobj);

    _pbuff = 0;
    _vbuff = dev->alloc_dma_pages(1,&_pbuff);
    
    assert(_pbuff);
    assert(_vbuff);
  }

  ~TestBlockWriter() {
    _dev->free_dma_pages(_vbuff);
  }


  void write_and_verify(off_t offset, uint8_t value) {

    memset(_vbuff,value,PAGE_SIZE);

    uint16_t cid = _dev->block_async_write(_qid,
                                           _pbuff,
                                           offset, /* LBA */
                                           1); /* num blocks */
    
    _nobj.set_when(cid);
    _nobj.wait();

    memset(_vbuff,0,PAGE_SIZE);

    cid = _dev->block_async_read(_qid,
                                 _pbuff,
                                 offset,
                                 1);
    
    _nobj.set_when(cid);
    _nobj.wait();
    
    uint8_t * p = (uint8_t *) _vbuff;

    bool good = true;
    for(unsigned i=0;i<512 /* block size */;i++) {
      if(p[i] != value) {
        PLOG("p[i] is %d not %d (i=%u)",p[i],value,i);
        hexdump(p,512);
        good = false;
        break;
      }
    }
    PLOG("FINISHED: status = %s", good ? "*** GOOD ***" : "BAD");
  }

};
#endif
