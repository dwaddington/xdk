#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libexo.h>
#include <common/dump_utils.h>
#include <common/cycles.h>

using namespace Exokernel;

/* use AHCI device for testing - remember to remove the stock AHCI module! */
static Exokernel::device_vendor_pair_t 
dev_tbl[] = {{0x8086,0x2922}, // Qemu
             {0x8086,0x3a22}, // Intel ICH
             {0x8086,0x2829}, // VirtualBox
             {0,0}};

class Dummy_device : public Exokernel::Pci_device {
public:
  Dummy_device() : 
    Exokernel::Pci_device(0, dev_tbl) 
  {
  }

  void test_huge_dma_allocation()
  {
    addr_t phys_addr;
    void * p;
    int num_pages = 2; /* note Linux off-the-shelf is limited to 4MB contiguous memory */
    p = alloc_dma_huge_pages(num_pages, &phys_addr, 0, MAP_HUGETLB);

    PINF("Allocated %d pages: v=%p p=%p", num_pages, p, (void*) phys_addr);

    memset(p,0,HUGE_PAGE_SIZE * num_pages);

    free_dma_huge_pages(p);
  }

  void test_dma_allocation()
  {
    addr_t phys_addr;
    void * p;
    int num_pages = 24;
    p = alloc_dma_pages(num_pages, &phys_addr);

    PINF("Allocated %d pages: v=%p p=%p", num_pages, p, (void*) phys_addr);

    memset(p,0,PAGE_SIZE * num_pages);

    free_dma_pages(p);
  }


};


int main()
{
  Pagemap pm;
  Dummy_device dev;
  dev.test_dma_allocation();
  dev.test_huge_dma_allocation();

  printf("Press return to continue....\n");
  getchar();

  //  pm.dump();
  printf("OK!\n");

  return 0;
}
