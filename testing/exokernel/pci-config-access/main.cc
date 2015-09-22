#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libexo.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <exo/pagemap.h>

using namespace Exokernel;

static Exokernel::device_vendor_pair_t 
dev_tbl[] = {{0x8086,0x5845}, // QEMU NVME SSD
             {0,0}};

class Dummy_device : public Exokernel::Pci_device {

public:
  Dummy_device() : 
    Exokernel::Pci_device(0, dev_tbl) 
  {
    pci_config()->dump_pci_device_info();
  }


};


int main()
{
  Dummy_device dev;

  // open /proc/parasite/pk0/pci_config and do some read?

  //  printf("Press return to continue....\n");
  // getchar();

  //  pm.dump();
  printf("OK!\n");

  return 0;
}
