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
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __COMPONENT_INTERFACE_H__
#define __COMPONENT_INTERFACE_H__

#include "types.h"

namespace Component
{


  /**
   * The component type.
   */
  typedef enum {
    NIC_COMPONENT = 0,    /**< The NIC component type. */
    STACK_COMPONENT = 1,  /**< The Stack component type. */
    MEM_COMPONENT = 2,    /**< The Memory component type. */
  } component_t;

  enum { MAX_INSTANCE = 10 };

  /** 
   * Base class. All interfaces must inherit from this class.
   */

  class Interface_base
  {
    component_t _comp_type;
    state_t     _comp_state[MAX_INSTANCE];

  public:
    /** 
     * To initialize the component interface.
     *
     * @param arg A struct pointer for configuration arguments.
     * @return The return status.
     */
    virtual status_t init(arg_t arg) = 0;

    /** 
     * To bind another interface pointed by itf.
     *
     * @param itf An interface pointer.
     * @return The return status.
     */
    virtual status_t bind(interface_t itf) = 0;

    /** 
     * To specify the cpu allocation for this interface.
     *
     * @param mask The CPU mask to indicate which CPUs are used.
     * @return The return status.
     */
    virtual status_t cpu_allocation(cpu_mask_t mask) = 0;
 
    /** 
     * To start the interface.
     */
    virtual void run() = 0;

    /** 
     * To obtain the component type.
     *
     * @return The type of this component.
     */
    component_t get_comp_type() {
      return _comp_type;
    }

    /** 
     * To set the component type.
     *
     * @param t The component type to be set.
     */
    void set_comp_type(component_t t) {
      _comp_type = t;
    }

    /** 
     * To obtain the state of component.
     *
     * @return The runtime state of current component.
     */
    state_t get_comp_state(unsigned idx) {
      return _comp_state[idx];
    }
    
    /** 
     * To set the component state.
     *
     * @param s The component state to be set.
     */
    void set_comp_state(state_t s, unsigned idx) {
      _comp_state[idx] = s;
    }
  } __attribute__((deprecated));

}
#endif
