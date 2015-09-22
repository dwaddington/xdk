#ifndef __EXO_PCI_CONFIG_H__
#define __EXO_PCI_CONFIG_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vector>
#include <map>

#include <common/utils.h>
#include <common/types.h>
#include <common/logging.h>

#include "../exo/pci.h"

namespace Exokernel
{
    class Pci_config_space {
    private:
      std::string  _root_fs;
      int          _fd;

    public:
      enum bus_speed {
        BUS_SPEED_UNKNOWN = 0,
        BUS_SPEED_33      = 33,
        BUS_SPEED_66      = 66,
        BUS_SPEED_100     = 100,
        BUS_SPEED_120     = 120,
        BUS_SPEED_133     = 133,
        BUS_SPEED_2500    = 2500,
        BUS_SPEED_5000    = 5000,
        BUS_SPEED_8000    = 8000,
        BUS_SPEED_RESERVED
      };

      /* PCI bus widths */
      enum bus_width {
        BUS_WIDTH_UNKNOWN = 0,
        BUS_WIDTH_PCIE_X1 = 1,
        BUS_WIDTH_PCIE_X2 = 2,
        BUS_WIDTH_PCIE_X4 = 4,
        BUS_WIDTH_PCIE_X8 = 8,
        BUS_WIDTH_32      = 32,
        BUS_WIDTH_64      = 64,
        BUS_WIDTH_RESERVED
      };

    public:
      Pci_config_space(std::string& root_fs) 
      {
        assert(!root_fs.empty());

        // TOFIX
        PLOG("root_fs is:(%s)",root_fs.c_str());
        _root_fs = "/proc/parasite/pk0/pci_config";

        _fd = open(_root_fs.c_str(), O_RDWR|O_SYNC);
        if(!_fd) {
          PWRN("unable to open file (%s)", _root_fs.c_str());
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to open PCI config space");
        }

        interrogate_bar_regions();
      }

      ~Pci_config_space() {
        if(_fd)
          close(_fd);
      }

      /** 
       * Accessor to PCI configuration block
       * 
       */
      uint16_t vendor()             { return read16(PCI_VENDOR_ID);    }
      uint16_t device()             { return read16(PCI_DEVICE_ID);    }
      uint16_t command()            { return read16(PCI_COMMAND);      }
      uint16_t status()             { return read16(PCI_STATUS);       }
      uint8_t  revision()           { return read8(PCI_REVISION);     }
      uint8_t cacheline_size()      { return read8(PCI_CACHE_LINE_SIZE);  }
      uint8_t latency_timer()       { return read8(PCI_LATENCY_TIMER);    }
      uint8_t header_type()         { return read8(PCI_HEADER_TYPE);      }
      uint8_t bist()                { return read8(PCI_BIST);             }
      uint32_t cardbus_cis()        { return read32(PCI_CARDBUS_CIS_PTR); }
      uint16_t subsys_vendor()      { return read16(PCI_SUBSYSTEM_VENDOR_ID); }
      uint16_t subsys()             { return read16(PCI_SUBSYSTEM_ID);        }
      uint32_t expansion_rom_addr() { return read32(PCI_EXPANSION_ROM); }
      uint8_t cap_pointer()         { return read8(PCI_CAP_PTR); }
      uint8_t irq_pin()             { return read8(PCI_INTERRUPT_PIN); }
      uint8_t irq_line()            { return read8(PCI_INTERRUPT_LINE); }
      uint8_t min_gnt()             { return read8(PCI_MIN_GNT); }
      uint8_t max_lat()             { return read8(PCI_MAX_LAT); }
      uint32_t bar(int i)           { return read32(PCI_BAR_0+i*4); }

      uint32_t classcode() {  
        uint32_t cc=0;
        cc |= read8(PCI_CLASSCODE_2) << 16;
        cc |= read8(PCI_CLASSCODE_1) << 8;
        cc |= read8(PCI_CLASSCODE_0);
        return cc;
      }

      void dump_pci_device_info() {
        PINF("PCI Device:");
        PINF("  Vendor ID:           %04x",vendor());
        PINF("  Device ID:           %04x",device());
        PINF("  Command:             %04x",command());
        PINF("  Status:              %04x",status());
        PINF("  Revision:            %02x",revision());
        PINF("  ClassCode:           %06x",classcode());
        PINF("  CacheLineSize:       %02x",cacheline_size());
        PINF("  LatencyTimer:        %02x",latency_timer());
        PINF("  HeaderType:          %02x",header_type());
        PINF("  BIST:                %02x",bist());
        PINF("  BAR[0]:              %08x",bar(0));
        PINF("  BAR[1]:              %08x",bar(1));
        PINF("  BAR[2]:              %08x",bar(2));
        PINF("  BAR[3]:              %08x",bar(3));
        PINF("  BAR[4]:              %08x",bar(4));
        PINF("  BAR[5]:              %08x",bar(5));
        PINF("  Cardbus CIS:         %08x",cardbus_cis());
        PINF("  Subsystem Vendor ID: %04x",subsys_vendor());
        PINF("  Subsystem ID:        %04x",subsys());
        PINF("  Expansion ROM:       %08x",expansion_rom_addr());
        PINF("  Cap Pointer:         %02x",cap_pointer());
        PINF("  IRQ_PIN:             %02x",irq_pin());
        PINF("  IRQ_LINE:            %02x",irq_line());
        PINF("  MIN_GNT:             %02x",min_gnt());
        PINF("  MAX_LAT:             %02x",max_lat());
      }

      uint8_t read8(unsigned offset) {
        uint8_t val;        
        if(pread(_fd,&val,1,offset) != 1) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to read8 PCI config space");
        return val;        
      }
      
      uint16_t read16(unsigned offset) {
        uint16_t val;        
        if(pread(_fd,&val,2,offset) != 2) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to read16 PCI config space");
        return val;        
      }

      uint32_t read32(unsigned offset) {
        uint32_t val;        
        if(pread(_fd,&val,4,offset) != 4) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to read16 PCI config space");
        return val;        
      }

      void write8(unsigned offset, uint32_t val) 
      {
        if(pwrite(_fd,&val,1,offset) != 1) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to write32 PCI config space");
      }

      void write16(unsigned offset, uint16_t val) 
      {
        if(pwrite(_fd,&val,2,offset) != 2) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to write32 PCI config space");
      }

      void write32(unsigned offset, uint32_t val) 
      {
        if(pwrite(_fd,&val,4,offset) != 4) 
          throw Exokernel::Fatal(__FILE__,__LINE__,"unable to write32 PCI config space");
      }

      size_t bar_region_size(unsigned i) const {
        if (i > 5) return -1;
        return _bar_region_sizes[i];
      }

    private:
      size_t _bar_region_sizes[6];

      void interrogate_bar_regions() {
        for(unsigned i=0;i<6;i++) {

          PDBG("accessing PCI bar (%u)", i);

          uint32_t bar = read32(PCI_BAR_0+i*4);
          if (bar > 0) {

            write32(PCI_BAR_0+i*4,~(0U));
            uint32_t ss_value = read32(PCI_BAR_0+i*4);
            ss_value &= ~0xFU;
            _bar_region_sizes[i] = ss_value & ~( ss_value - 1 );
            PLOG("PCI bar region %u is %ld bytes",i, _bar_region_sizes[i]);
      
            write32(PCI_BAR_0+i*4,bar);  // replace original
          } 
          else {
            _bar_region_sizes[i] = 0;
          }
        }
      }
    };
}

#endif
