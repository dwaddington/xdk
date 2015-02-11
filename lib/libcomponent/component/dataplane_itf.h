#ifndef __DATAPLANE_ITF_H__
#define __DATAPLANE_ITF_H__

#include <common/types.h>

enum ACTION {
  E_READ = 0, 
  E_WRITE
};

typedef struct {
  ACTION action;
  void *buffer_virt;
  void *buffer_phys;
  off_t offset;
  size_t num_blocks;
  unsigned port;
}__attribute__((aligned(64))) io_descriptor_t;


class IBlockData {
public:

  /** 
   * Synchronously read a block of data
   * 
   * @param buffer_virt Virtual address of client allocated buffer. Must be 512 byte aligned.
   * @param buffer_phys Physical address of client allcated buffer.
   * @param offset Block offset (in units of 512 byte blocks)
   * @param num_blocks Number of blocks to read
   * @param port Device port to read from
   * 
   * @return S_OK on success.
   */
  virtual status_t sync_read_block(void * buffer_virt,
                                   addr_t buffer_phys, 
                                   off_t offset,      
                                   size_t num_blocks, 
                                   unsigned port      
                                   ) = 0;
  
  /** 
   * Synchronously write a block of data
   * 
   * @param buffer_virt Virtual address of client allocated buffer. Must be 512 byte aligned.
   * @param buffer_phys Physical address of client allcated buffer.
   * @param offset Block offset (in units of 512 byte blocks)
   * @param num_blocks Number of blocks to read
   * @param port Device port to write to
   * 
   * @return S_OK on success.
   */
  virtual status_t sync_write_block(void * buffer_virt, /* must be 512 byte aligned */
                                    addr_t buffer_phys, 
                                    off_t offset,       /* store offset */
                                    size_t num_blocks,  /* each block is 512 bytes */
                                    unsigned port       /* device port */
                                    ) = 0;

  virtual status_t async_read_block(void * buffer_virt,
                                   addr_t buffer_phys, 
                                   off_t offset,      
                                   size_t num_blocks, 
                                   unsigned port,      
                                   uint16_t *cid
                                   ) = 0;

  virtual status_t async_write_block(void * buffer_virt, /* must be 512 byte aligned */
                                    addr_t buffer_phys, 
                                    off_t offset,       /* store offset */
                                    size_t num_blocks,  /* each block is 512 bytes */
                                    unsigned port,      /* device port */
                                    uint16_t *cid
                                    ) = 0;


  /* async batch io operation*/
  virtual status_t async_io_batch(io_descriptor_t* io_desc,
                                  uint64_t length
                                  ) = 0;

  /* TODO: add some nice async equivalents */

};

#endif // __DATAPLANE_ITF_H__
