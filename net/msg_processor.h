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
  @author Jilong Kuang (jilong.kuang@samsung.com)
*/

#ifndef __EXO_MSG_PROCESSOR_H__
#define __EXO_MSG_PROCESSOR_H__

#include "../lib/libexo/libexo.h"
#include <network/memory_itf.h>
 
using namespace Exokernel;

namespace Exokernel
{

  typedef void* obj_handle_t;

  class Msg_processor
  {

  public:
    IMem * _imem;
    IStack * _istack;

    Msg_processor(IMem * imem, IStack * istack) : _imem(imem), _istack(istack) {}

    /** 
     * To process network packet. The actual implementation of the method should be in 
     * inherited class for server side and client side.
     *
     * @param data The packet data struct pointer.
     * @param nic_id The NIC id.
     * @param tid The local thread/channel id.
     * @param core The global cpu id.
     * @param flag The packet reuse flag.
     * @return The return status.
     */
    virtual status_t process(void * data, unsigned nic_id, unsigned tid, unsigned core, bool& flag) = 0;

    virtual void create_tx_threads(void * ptr) = 0;
  };

}

#endif
