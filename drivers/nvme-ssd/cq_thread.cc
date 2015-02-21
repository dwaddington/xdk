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

#include <signal.h>
#include <libexo.h>

#include "cq_thread.h"
#include "nvme_queue.h"
#include "nvme_common.h"
#include "nvme_device.h"

/* enable or disable the callback manager */
//#define CONFIG_DISABLE_CALLBACK

std::vector<CQ_thread *> cq_objs;

CQ_thread::CQ_thread(NVME_IO_queue * qbase, unsigned core, unsigned vector, unsigned qid) 
  : Exokernel::Base_thread(NULL, core), /* order important */
    g_times_woken(0),
    g_entries_cleared(0),
    _qid(qid),
    _core(core)
{
  _irq = vector;
  _queues = qbase;
  PLOG("### CQ_thread: core=%u, qbase=%p",core,(void*)qbase);
  cq_objs.push_back(this);
}

void sig_handler(int sig) {
  /* dump stats */
  printf("\n");
  for(std::vector<CQ_thread*>::iterator i=cq_objs.begin(); i!=cq_objs.end(); i++) {
    printf("CQ thread (irq=%d): times woken = %lu (woken)/%lu (cleared)\n",
           (*i)->irq(),
           (*i)->g_times_woken,
           (*i)->g_entries_cleared);
  }
  panic("ctrl-c handled.");
}

void* CQ_thread::entry(void* qb) {

  assert(_queues);
  assert(_irq > 0);

  PLOG("CQ thread started");

  /* register Ctrl-C signal handler */
  struct sigaction sh;
  sh.sa_handler = sig_handler;
  sigemptyset(&sh.sa_mask);
  sh.sa_flags = 0;
  sigaction(SIGINT, &sh, NULL);

#if 0
  /* set higher priority */
  {
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);   
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
  }
#endif

  std::stringstream str;
  str << "cq-thread-" << _irq;
  Exokernel::set_thread_name(str.str().c_str());

  Completion_command_slot * ccs;
  status_t s;

  //  _queues->_pending_reader.wake_all();
  
  /* hmm, should be some other condition instead of while(1) !! */
  while(1) {

    /* wait for interrupt */
    if(_queues->device()->wait_for_msix_irq(_irq) != Exokernel::S_OK) {
      panic("unexpected");
      break;
    }

    // _queues->_wake_cq_thread.wait();
    // _queues->_wake_cq_thread.reset();
    
    unsigned found_completion = false;

    g_times_woken++;
    PLOG("IRQ(%u) !!! WOKEN !!! (core=%u) (times=%lu)",_irq, _core, g_times_woken);

  retry:

    /* iterate through the completed completion slots (looking at phase tag) */
    while((ccs = _queues->get_next_completion())!=NULL) {

#ifndef CONFIG_DISABLE_CALLBACK
      /* perform call back */
      PLOG("IRQ handler calling callback ccs->command_id=%d queue-id=%u",ccs->command_id, _qid);
      //s = _queues->callback_manager()->call(ccs->command_id);
      //assert(s == Exokernel::S_OK);
#endif

      /* update batch info */
      s = _queues->update_batch_manager(ccs->command_id);
      assert(s == Exokernel::S_OK);

      /* free slot */
      _queues->release_slot(ccs->command_id-1);

      found_completion = true;
      // if(!woke_reader) {
      //   woke_reader = true;
      //   _queues->_pending_reader.wake_one();
      // }
      g_entries_cleared++;
      PLOG("cleared = %lu (Q:%u)", g_entries_cleared, _qid);

      // if(g_entries_cleared % 1024 == 0) 
      //   PLOG("CQ thread cleared %lu entries",g_entries_cleared);
    }

    /* we may not have woken readers yet */
    // if(!woke_reader)
    //   goto retry;
    PLOG("found completion --> %s",found_completion ? "yes" : "no");

    /* ring completion doorbell for new _cq_head */

    if(found_completion) {
      PLOG("ringing completion doorbell");
      _queues->ring_completion_doorbell();
    }
    // else {
    //    _queues->dump_queue_info();
    // }
 
  }

  assert(0=="CQ exited?");
}


