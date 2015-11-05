#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libexo.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <exo/pagemap.h>

using namespace Exokernel;

/* must allocate huge pages */
/* use AHCI or E1000 device for testing - remember to remove the stock AHCI module! */
static Exokernel::device_vendor_pair_t 
dev_tbl[] = {{0x8086,0x2922}, // Qemu
             {0x8086,0x3a22}, // Intel ICH
             {0x8086,0x2829}, // VirtualBox
	     {0x8086,0x1528}, // X540 NIC card
             {0,0}};

static Exokernel::Pagemap pm;

class Dummy_device : public Exokernel::Pci_device {

public:
  Dummy_device() : 
    Exokernel::Pci_device(0, dev_tbl) 
  {
    pci_config()->dump_pci_device_info();
  }

  void test_huge_dma_allocation()
  {
    addr_t phys_addr;
    void * p;
    int num_pages = 2; /* note Linux off-the-shelf is limited to 4MB contiguous memory */
    p = alloc_dma_huge_pages(num_pages, &phys_addr, 0, MAP_HUGETLB);

    PINF("Allocated %d pages: v=%p p=%p", num_pages, p, (void*) phys_addr);

    PINF("BEFORE TOUCH: Page map thinks p=%p is mapped to p=%p",
         (void*) phys_addr,
         (void*) pm.virt_to_phys(p));

    memset(p,0,HUGE_PAGE_SIZE * num_pages);

    // test making shared
    grant_dma_access(phys_addr);

    // dump page map
    PINF("AFTER TOUCH: alloc_dma_huge_pages returned p=%p; pagemap gives p=%p (delta=%ld bytes)",
         (void*) phys_addr,
         (void*) pm.virt_to_phys(p),
         ((unsigned long) pm.virt_to_phys(p) - (unsigned long) phys_addr)
         );

    free_dma_huge_pages(p);
  }

  void test_dma_allocation()
  {
    addr_t phys_addr;
    void * p;
    int num_pages = 1;
    p = alloc_dma_pages(num_pages, &phys_addr);
    //    p = alloc_dma_pages(num_pages, &phys_addr, (void*) 0x7fff00000000);

    PINF("allocations:\n%s",debug_fetch_dma_allocations().c_str());

    PINF("* Allocated %d pages: v=%p p=%p", num_pages, p, (void*) phys_addr);

    printf("!!!! %lx\n",*((unsigned long*)p));
    memset(p,0,PAGE_SIZE * num_pages);

    // pm.dump_self_region_info();

    // PINF("page flags =>");
    // pm.dump_page_flags(pm.page_flags(p));
    // PINF("end of page flags.");

    // test making shared
    grant_dma_access(phys_addr);

    // dump page map
    PINF("AFTER TOUCH: alloc_dma_pages returned v=%p p=%p; pagemap gives p=%p (delta=%ld bytes)",
         p,
         (void*) phys_addr,
         (void*) pm.virt_to_phys(p),
         ((unsigned long) pm.virt_to_phys(p) - (unsigned long) phys_addr)
         );

    free_dma_pages(p);
  }


};


int main()
{
  Pagemap pm;
  Dummy_device dev;
  dev.test_dma_allocation();
  //  dev.test_huge_dma_allocation();

  printf("Press return to continue....\n");
  getchar();

  //  pm.dump();
  printf("OK!\n");

  return 0;
}
