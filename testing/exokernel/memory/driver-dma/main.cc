#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libexo.h>
#include <common/dump_utils.h>
#include <common/cycles.h>

/* use AHCI device for testing */
static Exokernel::device_vendor_pair_t 
dev_tbl[] = {{0x8086,0x3a22}, // Intel ICH
             {0x8086,0x2829}, // VirtualBox
             {0x8086,0x2922}, // Qemu
             {0,0}};

class Dummy_device : public Exokernel::Pci_device {
public:
  Dummy_device() : 
    Exokernel::Pci_device(0, dev_tbl) { 
  }

};


int main()
{
  Dummy_device dev;
  return 0;
}
