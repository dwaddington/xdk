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
  Authors:
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __EXO_ACPI_TABLES_H__
#define __EXO_ACPI_TABLES_H__

#include <stdint.h>
#include <vector>

namespace Exokernel
{

  class FADT_table;

  namespace Acpi_table_layouts
  {
    /* Root System Descriptor Table */
    struct DESCRIPTION_HEADER
    {
      struct {
        char _signature[4];
        uint32_t _length;
        uint8_t _revision;
        uint8_t _chk_sum;
        char _oem_id[6];
        char _oem_tid[8];
        uint32_t _oem_rev;
        uint32_t _creator_id;
        uint32_t _creator_rev;
      } __attribute__((packed));

      bool checksum_ok() 
      {
        uint8_t sum = 0;
        for (unsigned i = 0; i < _length; ++i)
          sum += *((uint8_t *)this + i);
      
        return !sum;
      }

      bool is_sig(const char * sig) {
        return 
          (_signature[0] == sig[0] &&
           _signature[1] == sig[1] &&
           _signature[2] == sig[2] &&
           _signature[3] == sig[3]);

      }
    } __attribute__((packed));
  
    struct SRAT_resource_entry
    {
      byte _type;
      byte _length;
    } __attribute__((packed));

    struct SRAT_resource_processor : public SRAT_resource_entry
    {
      byte     _proximity_domain_lo;      // Bit[7:0] of the proximity domain to which the processor belongs
      byte     _apic_id;                  // The processor local APIC ID.
      uint32_t _flags;
      byte     _local_sapic_eid;
      byte     _proximity_domain_hi[3];   // Bit[31:8] of the proximity domain to which the processor belongs.
      uint32_t _clock_domain;    
    
      void dump() {
        PLOG("SRAT:PROCESSOR PROX(%u) APIC(0x%x) FLAGS(%u) SAPIC(%u)",
             ((uint32_t)_proximity_domain_lo) | 
             ((uint32_t) _proximity_domain_hi[0]) << 8 | 
             ((uint32_t) _proximity_domain_hi[1]) << 16 | 
             ((uint32_t) _proximity_domain_hi[2]) << 24,
             _apic_id, _flags, _local_sapic_eid);
      }
    } __attribute__((packed));

    enum {
      SRAT_FLAGS_PROCESS_ENABLES = 0x1,
      SRAT_FLAGS_MEMORY_ENABLED = 0x1,
      SRAT_FLAGS_MEMORY_HOTPLUG = 0x2,
      SRAT_FLAGS_MEMORY_NONVOLATILE = 0x4,
    };

    struct SRAT_resource_memory : public SRAT_resource_entry
    {
      uint32_t _proximity_domain;
      byte     _reserved[2];
      uint32_t _base_address_lo;
      uint32_t _base_address_hi;
      uint32_t _length_lo;
      uint32_t _length_hi;
      uint32_t _reserved2;
      uint32_t _flags;
      byte     _reserved3[8];

      void dump() {
        uint64_t base = (uint64_t)_base_address_lo | ((uint64_t)_base_address_hi << 32);
        uint64_t len = (uint64_t)_length_lo | ((uint64_t)_length_hi << 32);
        PINF("SRAT:MEMORY (0x%lx-0x%lx) %lu GB PROX(%u) FLAGS(%u)",
             base, base+len, REDUCE_GB(len), _proximity_domain,_flags);
      }
    } __attribute__((packed));


    struct SRAT_resource_x2apic : public SRAT_resource_entry
    {
      uint16_t _reserved;
      uint32_t _proximity_domain;
      uint32_t _x2_apic_id;
      uint32_t _flags;
      uint32_t _clock_domain;
      uint32_t _reserved2;

      void dump() {
        PLOG("SRAT:X2_APIC ID(%u) PROX(%u) FLAGS(%u) CLOCK(%u)",
             _x2_apic_id, _proximity_domain, _flags, _clock_domain);
      }

    } __attribute__((packed));

    struct SRAT : public DESCRIPTION_HEADER
    {
      byte _reserved[12];
      SRAT_resource_entry _res_entries[0];
    } __attribute__((packed));

    struct FADT : public DESCRIPTION_HEADER
    {
      addr32_t _firmward_ctrl_addr;
      addr32_t _dsdt_phys;
      byte _reserved;
      byte _preferred_pm_profile;
      uint16_t _sci_int;
      addr32_t _smi_cmd;
      byte _acpi_enable;
      byte _acpi_disable;
      byte _s4bios_req;
      byte _pstate_cnt;
      addr32_t _pm1a_evt_blk;
      addr32_t _pm1b_evt_blk;
      addr32_t _pm1a_cnt_blk;
      addr32_t _pm1b_cnt_blk;
      addr32_t _pm2_cnt_blk;
      addr32_t _pm_tmr_blk;
      addr32_t _gpe0_block;
      addr32_t _gpe1_block;
      byte _pm1_evt_len;
      byte _pm1_cnt_len;

    } __attribute__((packed));


    struct SLIT : public DESCRIPTION_HEADER
    {
      uint64_t _num_sys_localities;
      byte _entries[0];
    } __attribute__((packed));
  }

  class FADT_table
  {
  public:
    // cached information
    addr32_t _smi_cmd;
    addr32_t _pm1a_cnt;
    addr32_t _pm1b_cnt;
    byte     _acpi_enable;
    byte     _acpi_disable;
    byte     _pm1_cnt_len;

  public:
    FADT_table() : _smi_cmd(0),
                   _pm1a_cnt(0),
                   _pm1b_cnt(0),
                   _acpi_enable(0),
                   _acpi_disable(0),
                   _pm1_cnt_len(0)
    {
    }
      
    void parse_fadt(void * base) {      
      assert(base);

      Acpi_table_layouts::FADT * f = (Acpi_table_layouts::FADT *) base;
      /* store off some useful values */
      _smi_cmd = f->_smi_cmd;
      _pm1a_cnt = f->_pm1a_cnt_blk;
      _pm1b_cnt = f->_pm1b_cnt_blk;
      _acpi_enable = f->_acpi_enable;
      _acpi_disable = f->_acpi_disable;
      _pm1_cnt_len = f->_pm1_cnt_len;

      dump();
    }

    void dump() {
      PINF("== ACPI FADT =========");
      PINF("acpi_enable  = 0x%x",_acpi_enable);
      PINF("acpi_disable = 0x%x",_acpi_disable);
      PINF("smi_cmd      = 0x%x",_smi_cmd);
      PINF("pm1a_cnt     = 0x%x",_pm1a_cnt);
      PINF("pm1_cnt_len  = 0x%x",_pm1_cnt_len);
      PINF("======================");
    }

  };

  class Table_base
  {
  protected:
    void * _base;
    size_t _base_map_length;
 
  public:
    Table_base() : _base(NULL), _base_map_length(0) {
    }

    ~Table_base() {
      if(_base)
        free(_base);
    }

    void allocate_base(size_t len) { 
      _base = malloc(len);
      assert(_base);
      _base_map_length = len;
    
    }

    void * base() const { 
      return _base; 
    }
  };

  /** 
   * System Resource Affinity Table (SRAT) - normally only found in NUMA capable systems.
   * 
   */
  class SRAT_table : public Table_base
  {
  private:
    unsigned                                                   _num_records;
    unsigned                                                   _num_processors;
    unsigned                                                   _num_memory;
    unsigned                                                   _num_x2apic;
    std::vector<Acpi_table_layouts::SRAT_resource_processor *> _processors;
    std::vector<Acpi_table_layouts::SRAT_resource_memory *>    _memory;
    std::vector<Acpi_table_layouts::SRAT_resource_x2apic *>    _x2apic;

    struct srat_info_t
    {
      uint8_t type;
      union {
        struct 
        {
          unsigned proximity;
          unsigned apic;
          uint32_t flags;
        } processor;

        struct
        {
          unsigned proximity;
          addr_t base;
          size_t size;
          uint32_t flags;
        } memory;

        struct 
        {
          unsigned proximity;
          unsigned x2apic_id;
          uint32_t flags;
        } x2apic;
      };

      void dump() {
        if(type == 0) {
          PLOG("SRAT:PROCESSOR proximity=%u apic=%u flags=%u",
               processor.proximity, processor.apic, processor.flags);
        }
        else if(type == 1){
          PLOG("SRAT:MEMORY proximity=%u base=0x%lx size=%lu flags=%u",
               memory.proximity, memory.base, memory.size, memory.flags);
        }
        else {
          PLOG("SRAT:X2APIC proximity=%u id=%u flags=%u",
               x2apic.proximity, x2apic.x2apic_id, x2apic.flags);
        }
      }
    };

  public:
    enum { 
      verbose = false 
    };

  public:
    /** 
     * Constructor.
     * 
     */
    SRAT_table() : _num_records(0), 
                   _num_processors(0), 
                   _num_memory(0)
    {
    }

    ~SRAT_table() {
      for(std::vector<Acpi_table_layouts::SRAT_resource_memory *>::iterator i=_memory.begin();
          i!=_memory.end(); i++) {
          delete (*i);
      }
      for(std::vector<Acpi_table_layouts::SRAT_resource_processor *>::iterator i=_processors.begin();
          i!=_processors.end(); i++) {
          delete (*i);
      }
      for(std::vector<Acpi_table_layouts::SRAT_resource_x2apic *>::iterator i=_x2apic.begin();
          i!=_x2apic.end(); i++) {
          delete (*i);
      }

    }

    void dump() {
      //  std::vector<Acpi_table_layouts::SRAT_resource_memory *>    _memory;
      printf("== MEMORY ==\n");
      for(std::vector<Acpi_table_layouts::SRAT_resource_memory *>::iterator i=_memory.begin();
          i!=_memory.end(); i++) {
        (*i)->dump();
      }

    }

    /** 
     * Parse and store state for SRAT table
     * 
     * @param base Base pointer to table.
     */
    void parse(void * base) {

      using namespace Acpi_table_layouts;

      if(verbose)
        PLOG("[ACPIX]: Parsing SRAT table.");

      assert(base);
      SRAT * t = (SRAT *) base;
    
      unsigned records_length = t->_length - sizeof(Acpi_table_layouts::SRAT);
    
      SRAT_resource_entry * res = t->_res_entries;
      while(records_length > 0) {

        if(res->_type == 0) {
          //        ((SRAT_resource_processor *) res)->dump();        
          /* copy of record since memory will be unmapped. */
          _processors.push_back(new SRAT_resource_processor(*((SRAT_resource_processor *) res)));
          _num_processors++;
        }
        else if(res->_type == 1) {
          //        ((SRAT_resource_memory *) res)->dump();
          /* copy of record since memory will be unmapped. */
          _memory.push_back(new SRAT_resource_memory(*((SRAT_resource_memory *) res)));
          _num_memory++;
        }
        else if(res->_type == 2) {
          //((SRAT_resource_x2apic *) res)->dump();
          /* copy of record since memory will be unmapped. */
          _x2apic.push_back(new SRAT_resource_x2apic(*((SRAT_resource_x2apic *) res)));
          _num_x2apic++;
        }
        else assert(0);

        _num_records++;
        records_length -= res->_length;
        res = (SRAT_resource_entry *) (((byte *)res) + res->_length);
      }
    }



    /** 
     * Return the total number of resource entries in the SRAT table
     * 
     * @return Number of resources. 0 if no SRAT.
     */
    unsigned srat_num_resources() const {
      return _num_records;
    }


    /** 
     * Fetch an SRAT record from the stored list of records.
     * 
     * @param idx Record index.
     * @param info Record content.
     * 
     * @return S_OK or E_OUT_OF_BOUNDS for invalid index.
     */
    status_t srat_get_record(unsigned idx, srat_info_t * info) const {

      using namespace Acpi_table_layouts;

      if(idx > _num_records) return Exokernel::E_OUT_OF_BOUNDS;

      if(idx < _num_processors) {

        SRAT_resource_processor * res = _processors[idx];

        info->type = res->_type;
        info->processor.flags = res->_flags;
        info->processor.apic = res->_apic_id;

        unsigned proximity = ((uint32_t)res->_proximity_domain_lo) | 
          ((uint32_t) res->_proximity_domain_hi[0]) << 8 | 
          ((uint32_t) res->_proximity_domain_hi[1]) << 16 | 
          ((uint32_t) res->_proximity_domain_hi[2]) << 24;

        info->processor.proximity = proximity;
      }
      else if(idx < (_num_processors + _num_memory)) {

        SRAT_resource_memory * res = _memory[idx-_num_processors];

        info->type = res->_type;
        uint64_t base = (uint64_t)res->_base_address_lo | ((uint64_t)res->_base_address_hi << 32);
        uint64_t len = (uint64_t)res->_length_lo | ((uint64_t)res->_length_hi << 32);

        info->memory.base = base;
        info->memory.size = len;
        info->memory.flags = res->_flags;
        info->memory.proximity = res->_proximity_domain;
      }
      else {
        SRAT_resource_x2apic * res = _x2apic[idx-_num_processors-_num_memory];

        info->x2apic.proximity = res->_proximity_domain;
        info->x2apic.x2apic_id = res->_x2_apic_id;
        info->x2apic.flags = res->_flags;
      }
    
      return Exokernel::S_OK;
    }
  
  };


  /** 
   * System Locality Distance Information Table (SLIT) - normally only found in NUMA capable systems.
   * 
   * @param base 
   */
  class SLIT_table
  {
  private:

    enum { verbose = false };
  
    byte * _matrix;
    size_t _matrix_size;
    bool   _initialized;

    struct Localities 
    {
      enum { MAX_SIZE = 128 };
      size_t _matrix_len;
      byte   _matrix[MAX_SIZE];

      Localities() : _matrix_len(0) {
      }
    
      static size_t max_len() { return MAX_SIZE; }

      void dump() {
        if(_matrix_len == 0) return;

        unsigned sqrt = 1;
        while(sqrt * sqrt < _matrix_len) sqrt++; // avoid libm
        printf("= SLIT matrix ======================\n");

        printf("\t");
        for(unsigned i=0;i<sqrt;i++) 
          printf("[%u]\t",i);
        printf("\n");

        for(unsigned i=0;i<sqrt;i++) {
          printf("[%u]\t",i);
          for(unsigned j=0;j<sqrt;j++) {
            printf("%u\t",_matrix[i*sqrt+j]);
          }
          printf("\n");
        }
        printf("====================================\n");
      }
    };

  public:

    /** 
     * Constructor.
     * 
     */
    SLIT_table() : _matrix(0), 
                   _initialized(false) 
    {
    }

    ~SLIT_table() {
      if(_matrix)
        free(_matrix);
    }


    /** 
     * Main SLIT parse routine
     * 
     * @param base table base pointer
     */
    void parse(void * base) {
      using namespace Acpi_table_layouts;

      if(verbose)
        PLOG("[ACPIX]: Parsing SLIT table.");

      assert(base);
      SLIT * t = (SLIT *) base;

      _matrix_size = t->_num_sys_localities * t->_num_sys_localities;
      assert(_matrix_size < 256);

      if(verbose)
        PLOG("[ACPIX]: SLIT matrix is %lu bytes",_matrix_size);
    
      assert(_matrix==NULL);
      _matrix = new byte[_matrix_size];

      for(unsigned i=0;i<_matrix_size;i++) {
        // if(verbose)
        //   PLOG("SLIT: entry[%u]=%u",i,t->_entries[i]);
        _matrix[i] = t->_entries[i];
      }

      _initialized = true;
    }

    /** 
     * Retrieve the SLIT localities
     * 
     * @param sz          [out] number of localities (each one byte)
     * @param localities  [out] locality data
     * 
     * @return S_OK
     */
    status_t slit_get_localities(size_t * sz, SLIT_table::Localities * localities) const {

      if(!_initialized) {
        PWRN("WARNING: calling slit_get_localities before initialization is complete");
        return Exokernel::E_FAIL;
      }

      if(_matrix_size > SLIT_table::Localities::max_len()) {
        assert(0);
        return Exokernel::E_LENGTH_EXCEEDED;
      }

      localities->_matrix_len = _matrix_size;
      memcpy(localities->_matrix, _matrix, _matrix_size);
      *sz = _matrix_size;

      return Exokernel::S_OK;
    }

  };


  class Acpi_tables : public FADT_table,
                      public SRAT_table,
                      public SLIT_table
  {  
  };
}

#endif
