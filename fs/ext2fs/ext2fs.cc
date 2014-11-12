/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  
*/







/*
  Author(s):
  @author Daniel Waddington (d.waddington@samsung.com)
  @date: Oct 1, 2012
*/

#include <printf.h>
#include <stdlib.h>
#include <types.h>
#include <utils.h>
#include <range_tree.h>

#include "block_device.h"
#include "ext2fs.h"
#include "ext2fs_file.h"
#include "ext2_superblock.h"
#include "ext2_inode.h"
#include "ext2_addr_block.h"

using namespace OmniOS;
using namespace Ext2fs;

#define MB(X) (X * 1024 * 1024)
#define AHCI_CLIENT_BUFFER MB(8)

#ifdef VERBOSE
void EXT2FS_INFO(const char *fmt, ...) {
  OmniOS::printf(MAGENTA);
  va_list list;
  va_start(list, fmt);
  OmniOS::printf("[EXT2FS]:");
  vprintf(fmt, list);
  va_end(list);
  OmniOS::printf(RESET);
}
#endif


/** 
 * Read the Inode for the root directory
 * 
 * 
 * @return Pointer to Inode which should be freed by callee
 */
Inode * Ext2fs::Ext2fs_core::get_root_directory() {
  /* get root directory inode */
  Inode * inode = _super_block->get_inode_ref(EXT2_INODE_ROOT_INDEX);
  assert(inode);
  return inode;
}

void Ext2fs::Ext2fs_core::read_inode(uint32_t inode_index, Inode * inode)
{
  _super_block->read_inode(inode_index,inode);
}

static void dump_directory_entries(Directory_entry * entry, size_t size_of_entries)
{
  info("--dir entries (size=%u)----\n",size_of_entries);
  unsigned s = size_of_entries;
  
  while(s > 0) {
    entry->dump();
    s -= entry->get_record_length();
    entry = entry->next();
  } 
  info("---------------------------\n");
}

void Ext2fs::Ext2fs_core::dump_directory_entries(Inode * i_dir)
{
  //  assert(i_dir->is_dir());
  size_t dir_size;
  Directory_entry * d_entries = get_directory_entries(i_dir, dir_size);
  ::dump_directory_entries(d_entries, dir_size);
}



/** 
 * From a given directory Inode read the directory entries.
 * 
 * @param dir_inode Directory inode
 * @param size_of_content Number of entries in the block
 * 
 * @return A copy of the directory entry data which must be freed by the callee
 */
Directory_entry * Ext2fs::Ext2fs_core::get_directory_entries(Inode * dir_inode, size_t& size_of_content)
{
  /* get blocks for directory inode */
  Block_address_collective * c = get_blocks_for_inode(dir_inode);
  assert(c);

  size_of_content = dir_inode->get_size();
  size_t remaining_size = size_of_content;

  Directory_entry * entry = (Directory_entry *) malloc(size_of_content);
  assert(entry);
  byte * p = (byte *) entry;

  /* iterate through block address lists */
  for(unsigned i = 0;i < c->list_count();i++) {
    Block_address_list * bal = c->get_block_list(i);
    assert(bal);
    unsigned bal_len = bal->len();
    for(unsigned j = 0; j < bal_len; j++) {
      block_address_t baddr = bal->get_block_address(j);

      if(baddr==0) break; /* bal_len is just the upper bound */

      size_t read_this_time = remaining_size;
      if(read_this_time > _fs_block_size) 
        read_this_time = _fs_block_size;
      
      _block_cache_session->read(block_to_abs_offset(baddr),
                                 read_this_time,
                                 p);  
      p += read_this_time;
      remaining_size -= read_this_time;
    }
  }
  /* release collective memory */
  delete c;

  // if(dir_inode->get_direct_data_block_ptr(1)) {
    
  //   ::dump_directory_entries(entry,size_of_content);
  //   panic("check");
  // }

  return entry;
}


Inode * Ext2fs::Ext2fs_core::find_local_directory(BString dirname, Inode * current_dir)
{
  EXT2FS_INFO("find_local_directory: [%s]\n",dirname.c_str());
  
  //  dump_directory_entries(current_dir);

  Directory_entry * fst;
  size_t size_of_content;
  Directory_entry * entry = fst = get_directory_entries(current_dir,size_of_content);

  delete current_dir;

  while(size_of_content > 0) {
    //    entry->dump();

    //    EXT2FS_INFO("checking against [%s]==[%s]\n",dirname.c_str(),entry->get_name());
    /* check entry */
    if(dirname == entry->get_name()) {

      assert(entry->get_file_type() == 0x2); /* should have matched a directory entry */

      unsigned inode_idx = entry->get_inode();
      Inode * result = _super_block->get_inode_ref(inode_idx);
      
      /* free directory entries memory */
      free(fst);

      //      result->dump();

      return result;
    }
    
    entry = entry->next();
    size_of_content -= entry->get_record_length();
  }

  panic("find_local_directory could not find dir [%s].",dirname.c_str());
  return NULL;
}

/** 
 * 
 * 
 * @param pathname Path starting with '/' 
 * 
 * @return 
 */
Inode * Ext2fs::Ext2fs_core::locate_directory(BString pathname, Inode * current_dir)
{
  EXT2FS_INFO("locate_directory: [%s]\n",pathname.c_str());
  if(pathname[0] != '/') return NULL;

  if(pathname[1] == '\0') {
    return current_dir;
  }

  /* strip off next subdirectory */
  char * p = (char *) &pathname[1];
  char next_dirname[255];

  char * q = next_dirname;
  while(*p != '/') {
     *q++ = *p++;
  }
  *q = '\0';

  Inode * next_dir = NULL;
  if(!(next_dir = find_local_directory(BString(next_dirname),current_dir))) {
    panic("could not find path component.");
  }

  delete current_dir; // no longer need this

  return locate_directory(BString(p),next_dir);      
}

/** 
 * Locate a specific file with a given directory
 * 
 * @param filename 
 * @param dir 
 * 
 * @return Inode of the locate file or NULL on failure
 */
Inode * Ext2fs::Ext2fs_core::locate_file(const char * filename, Inode * dir)
{
  assert(dir);
  assert(filename);

  assert(!(dir->flags & FS_INDEX_FL));

  /* iterate through the directory entries */
  size_t dir_size;
  Directory_entry * fph;
  Directory_entry * d_entries = fph = get_directory_entries(dir, dir_size);

  unsigned s = dir_size;

  //  dump_directory_entries(dir);
  
  while(s > 0) {
    
    // inode());
    // d_entries->dump();

    if(d_entries->get_inode() > 0 ) { /* inode == 0 means entry is unused */
      if(strcmp(d_entries->get_name(),filename)==0) {
        unsigned inode_idx = d_entries->get_inode();
        assert(inode_idx > 0);

        Inode * result = _super_block->get_inode_ref(inode_idx);
        free(fph);
        assert(result->is_file()); // should be a file
        return result;
      }
    }

    s -= d_entries->get_record_length();
    d_entries = d_entries->next();
  } 

  panic("locate_file: unable to locate file [%s]\n",filename);
  return 0;
}


aoff64_t Ext2fs::Ext2fs_core::get_offset_for_file(Inode * inode, unsigned dbp_index)
{
  assert(inode->is_file());

  return block_to_abs_offset(inode->get_direct_data_block_ptr(dbp_index));
}

Inode * Ext2fs::Ext2fs_core::get_inode(unsigned block)
{
  return _super_block->get_inode_from_block(block);
}

Block_address_list * 
Ext2fs::Ext2fs_core::get_block_addresses(unsigned block)
{
  return _super_block->get_block_address_list(block);
}

Block_address_collective * 
Ext2fs::Ext2fs_core::get_singly_indirect_block_addresses(Inode * i_file)
{
  if(i_file->get_singly_indirect() == 0) return NULL;

  Block_address_list * bal = get_block_addresses(i_file->get_singly_indirect());
  Block_address_collective * r = new Block_address_collective;
  r->insert(bal);
  return r;
}


Block_address_collective *
Ext2fs::Ext2fs_core::get_doubly_indirect_block_addresses(Inode * i_file)
{
  if(i_file->get_doubly_indirect() == 0) return NULL;

  Block_address_list * bal = get_block_addresses(i_file->get_doubly_indirect());
  Block_address_collective * r = new Block_address_collective;
  assert(r);

  block_address_t * block_ids = bal->list();
  // panic("count = %u\n",bal->count_blocks());

  // {
  //   unsigned j=0,c=0;
  //   for(;j<256;j++) {
  //     if(block_ids[j])
  //       c++;
  //   }
  //   panic("c=%u\n",c);
  // }

      
  unsigned len = bal->len();
  for(unsigned b=0;b<len;b++) {

    if(block_ids[b] > 0) {
      Block_address_list * bal = get_block_addresses(block_ids[b]);
      assert(bal);
      r->insert(bal); /* add to the collective */
    }
  }
  return r;
}

Block_address_collective *
Ext2fs::Ext2fs_core::get_triply_indirect_block_addresses(Inode * i_file)
{
  if(i_file->get_triply_indirect() == 0) return NULL;

  Block_address_list * bal = get_block_addresses(i_file->get_triply_indirect());
  Block_address_collective * r = new Block_address_collective;
  assert(r);

  block_address_t * block_ids = bal->list();
  unsigned len = bal->len();
  for(unsigned b=0;b<len;b++) {

    if(block_ids[b] > 0) {
      Block_address_list * bal_2l = get_block_addresses(block_ids[b]);
      block_address_t * block_ids_2l = bal_2l->list();

      unsigned len = bal_2l->len();
      //      info("(%u) 2-level len = %u\n",b,len);
      for(unsigned c=0;c<len;c++) {
        if(block_ids_2l[c] > 0) {
          Block_address_list * bal_3l = get_block_addresses(block_ids_2l[c]);
          r->insert(bal_3l);
          //          info("  (%u) 3-level entries = %u\n",c, bal_3l->len());
          //          bal_3l->dump();
        }
      }

      delete bal_2l;
    }
  }

  return r;
}


class Block_range_aggregator
{
private:
  unsigned _growth_index;
  enum { MAX_ALLOCATION = L4_PAGESIZE };

  struct {
    unsigned _start_block;
    unsigned _end_block;
  } range_t;

  
public:
};

Block_address_collective * 
Ext2fs::Ext2fs_core::get_blocks_for_inode(Inode * i_file)
{
  assert(i_file);
  Block_address_collective * collective = new Block_address_collective;
  assert(collective);

  /* insert direct blocks */
  collective->insert(i_file->get_direct_blocks());

  /* insert single indirect blocks */
  Block_address_collective * c;

  /* insert doubly indirect blocks */
  c = get_singly_indirect_block_addresses(i_file);
  if(c) {
    collective->append(c);
  }

  /* insert doubly indirect blocks */
  c = get_doubly_indirect_block_addresses(i_file);
  if(c) {
    collective->append(c);
  }

  /* insert triply indirect blocks */
  c = get_triply_indirect_block_addresses(i_file);
  if(c) {
    collective->append(c);
  }

  /* we may be able to short-circuit early ? */
  return collective;
}

/** 
 * 
 * 
 * @param pathname Full path name starting with '/'
 * 
 * @return 
 */
status_t Ext2fs::Ext2fs_core::get_blocks_for_file(const char * pathname, 
                                                  Block_address_collective *& out_collective, 
                                                  Inode *& out_inode)
{
  /* tokenize pathname into directory and file name */
  char * p = (char *) pathname;
  unsigned last_slash = 0, len = 0;
  char dir[MAX_PATH_SIZE];
  while(*p > 0) {
    if(*p=='/') last_slash = len;
    dir[len] = *p;
    len++;
    p++;
    assert(len < MAX_PATH_SIZE);
  }
  dir[last_slash+1] = '\0';

  /* locate directory */
  Inode * i_dir = locate_directory(BString(dir),get_root_directory());
  if(!i_dir) {
    assert(i_dir);
    return E_NOT_FOUND;
  }

  /* locate file */
  Inode * i_file = locate_file(&pathname[last_slash+1],i_dir);
  if(!i_file) {
    assert(i_file);
    return E_NOT_FOUND;
  }

  /* collect block information */
  Block_address_collective * c = get_blocks_for_inode(i_file);
  assert(c);

  out_inode = i_file;
  delete i_dir;
  out_collective = c;

  return S_OK;
}

Ext2fs::File * 
Ext2fs::Ext2fs_core::open(const char * pathname) {

  File * r = new File(this);
  /* for the moment, all we have is files */
  if(r->open_file(pathname)==S_OK) return r;
  else {
    warn("[EXT2FS]: Unable to open file (%s)\n",pathname);
    delete r;
    assert(0);
    return NULL;
  }
}

status_t Ext2fs::Ext2fs_core::close(Ext2fs::File * handle) {
  
  /* flush etc. */
  delete handle;
  return S_OK;
}


#define RECORDS_PER_CHUNK 1024*32  /* translates to 256K */

void test_read_random_blocks(File * fp, uint64_t max_bytes);

void 
Ext2fs::Ext2fs_core::test2()
{
  File * fp = open("/data/hexfile.1");
  assert(fp);
  
  printf("Opened file: %u bytes\n",fp->size_in_bytes());
  //  dump_directory_entries(get_root_directory());

  fp->md5sum();
  panic("test2 OK.");
}

void 
Ext2fs::Ext2fs_core::test()
{

  File * fp = open("/data/hexfile.2");
  assert(fp);
  
  printf("Opened file: %u bytes\n",fp->size_in_bytes());

  //  test_read_random_blocks(fp,fp->size_in_bytes());

  unsigned long records_to_read = fp->size_in_bytes() / 8;
  filepos_t pos = 0;
  unsigned print_every = 0;

#define VERIFY_DATA
#ifdef VERIFY_DATA
  uint64_t last_record;
#endif
  
  uint64_t * data = (uint64_t*) malloc(RECORDS_PER_CHUNK * 8);
  while(records_to_read > 0) {
    
    unsigned records_this_cycle = MIN<filepos_t>(records_to_read,RECORDS_PER_CHUNK);
    //    info("Reading chunk of %u records.\n",records_this_cycle);

    fp->read(pos,records_this_cycle*8,data);
    assert(data);

#ifdef VERIFY_DATA
    for(unsigned r = 0; r < records_this_cycle; r++) {
      if(data[r] > 0) {
        if(data[r] != last_record + 1) { // make sure they're consequetive
          panic("Mismatch (record=%u) %llu != %llu\n",r,data[r],last_record);
        }       
      }      
      last_record = data[r];
    }
#endif
    records_to_read -= records_this_cycle;
    
    if(print_every == 100) {
      printf("%lu records left to read ..\n",records_to_read);
      print_every = 0;
    }
    else print_every++;

    pos += records_this_cycle * 8;
  }
  free((void*)data);
    


#if 0
  Block_address_collective * c;
  Inode * file_inode;
  get_blocks_for_file("/data_dir/video.raw",c,file_inode);
  assert(file_inode->get_size() == c->total_count() * _fs_block_size);
  info("Total blocks = %u\n",c->total_count());
#endif

#if 0
  Block_address_collective * c = get_blocks_for_inode(i_file);
  info("Total blocks = %u\n",c->total_count());
#endif
 
#if 0 
  unsigned total_count = 0;
  {
    Block_address_list * bal = i_file->get_direct_blocks();
    info("There are %u direct blocks.\n", bal->count_blocks());
    total_count+=bal->count_blocks();
    //    bal->collect_ranges(rt);
    delete bal;
  }
  {
    Block_address_list * bal = get_block_addresses(i_file->get_singly_indirect());
    //    bal->dump();
    unsigned n = bal->count_blocks();
    total_count += n;
    info("Singly added %u blocks\n",n);
    //    bal->collect_ranges(rt);
    delete bal;    
  }
  {
    Block_address_collective * c = get_doubly_indirect_block_addresses(i_file);
    unsigned n = c->total_count();
    total_count += n;
    info("Doubly added %u blocks\n",n);
    //    c->collect_ranges(rt);
    delete c;
  }
  {
    Block_address_collective * c = get_triply_indirect_block_addresses(i_file);
    unsigned n = c->total_count();
    total_count += n;
    info("Triply added %u blocks\n",n);
    //    c->collect_ranges(rt);
    delete c;
  }
#endif
  
  panic("TEST OK.");

}




int main()
{
  EXT2FS_INFO("entered OK.\n");

  //turn_malloc_trace_on();

  /* create instance (proxy) of block device */
  block_device_t<Module::Ahci::Block_device_proxy,
                 Module::Ahci::Block_device_session_proxy,
                 AHCI_CLIENT_BUFFER> ahci("ahci_driver");

  EXT2FS_INFO("created proxy to AHCI OK.\n");
  
  /* create file system and hook in block device */
  Ext2fs::Ext2fs_core core(&ahci,0 /* 1st ext2 partition */);
  
  EXT2FS_INFO("created file system state and hooked into AHCI OK.\n");

  Ext2fs::Filesystem_ipc_service _bootstrap_interface(&core); /* bootstrap interface */

  // open up hex file and test the file integrity
  //core.test();
  //  core.test2();

  sleep_forever();
  return S_OK;
}
