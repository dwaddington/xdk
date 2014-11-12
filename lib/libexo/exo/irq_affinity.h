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

#ifndef __EXOKERNEL_IRQ_AFFINITY_H__
#define __EXOKERNEL_IRQ_AFFINITY_H__

#include <fstream>
#include <sstream>
#include <unistd.h>
#include <set>
#include <iomanip>

namespace Exokernel
{
  /** 
   * Replacement for cpu_set_t but easier to use.
   * 
   */
  class Cpu_mask
  {
  private:
    std::set<core_id_t> _cores;

  public:

    /** 
     * Constructors
     * 
     */
    Cpu_mask() {      
    }

    Cpu_mask(core_id_t c) {
      add_core(c);
    }

    /** 
     * Add a single core to the mask.
     * 
     * @param c Core identifier to add
     */
    void add_core(core_id_t c) {
      _cores.insert(c);
    }

    /** 
     * Add a range of cores to the mask. Range is inclusive
     * 
     * @param c_start Start of range
     * @param c_end End of range
     */
    status_t add_range(core_id_t c_start, core_id_t c_end) {

      if(c_end <= c_start) 
        return Exokernel::E_INVAL;

      for(unsigned i=c_start;i<=c_end;i++) 
        add_core(i);

      return 0;
    }

    /** 
     * Return the largest core identifier in the set
     * 
     * 
     * @return Larget core identifier
     */
    core_id_t max_core() const {
      core_id_t r = 0;
      for(std::set<core_id_t>::iterator i = _cores.begin();
          i!=_cores.end(); i++) {
        if((*i) > r)
          r = *i;
      }
      return r;
    }

    /** 
     * Return the bitmap dword for the mask
     * 
     * @param dword Dword counting from 0
     * 
     * @return 32bit mask
     */
    uint32_t mask_dword(unsigned dword) {
      unsigned range_start = dword * 32;
      uint32_t result = 0;
      for(std::set<core_id_t>::iterator i = _cores.begin();
          i!=_cores.end(); i++) {
        unsigned c = (*i);
        if((c >= range_start) && (c < (range_start+32))) {
          result |= (1 << (c - range_start));
        }
      }
      return result;
    }

    /** 
     * Return the mask string compatible with the /proc/irq/xxx/smp_affinity
     * format
     * 
     * 
     * @return Mask string
     */
    std::string mask_string() {
      
      std::stringstream sstr;
      unsigned max_dword = max_core() / 32;

      /* msb first */
      int i=max_dword;
      while(i > 0) {
        sstr << std::setfill('0') << std::setw(8) << std::hex << mask_dword(i) << ",";
        i--;
      }
      sstr << std::setfill('0') << std::setw(8) << std::hex << mask_dword(i);

      return sstr.str();
    }

  };
  

  /** 
   * Set the mask for an MSI/MSI-X interrupt.
   * 
   * @param irq 
   * @param mask 
   * 
   * @return 
   */
  static status_t route_interrupt_mask(unsigned irq, Cpu_mask& mask)
  {
    try {
      std::stringstream fname;
      fname << "/proc/irq/" << irq << "/smp_affinity";
      std::ofstream ofs;
      ofs.open(fname.str().c_str());
      
      std::string s = mask.mask_string();
      ofs << s << std::endl;

      ofs.close();

      return Exokernel::S_OK;
    }
    catch(...) {
      throw Exokernel::Fatal(__FILE__, __LINE__, "unexpected condition setting IRQ CPU affinity");
    }
  }
  

  /** 
   * Routes an MSI/MSI-X interrupt to a specific *single* core.
   * @param irq 
   * @param core 
   * @return 
   */
  SUPPRESS_NOT_USED_WARN
  static status_t route_interrupt(unsigned irq, core_id_t core) {
    Cpu_mask m(core);
    route_interrupt_mask(irq,m);
    return Exokernel::S_OK;
  }
}


#endif // __EXOKERNEL_IRQ_AFFINITY_H__
