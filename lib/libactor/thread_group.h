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
  Copyright (C) 2015, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __THREAD_GROUP_H__
#define __THREAD_GROUP_H__

#include <functional>
#include <vector>
#include <boost/thread.hpp>


/** 
 * Thread-safe class for managing groups of boost threads threads
 * 
 */
class ThreadGroup
{
private:
  boost::mutex                 mutex_;
  std::vector<boost::thread *> vector_;

public:

  /** 
   * Destructor which clears the group and deletes each thread instance.
   * 
   */
  ~ThreadGroup() {
    clear();
  }
  
  /** 
   * Add a thread to the group
   * 
   * @param thread Thread to add
   */
  void add(boost::thread* thread) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    vector_.push_back(thread);
  }


  /** 
   * Apply lambda function to each thread in the group while holding lock
   * 
   * @param func Lambda function or function pointer to apply to each member.  List is locked throughout.
   */
  void for_each(std::function<void (boost::thread *)> func) {
    boost::lock_guard<boost::mutex> lock(mutex_);

    for(std::vector<boost::thread*>::iterator i = vector_.begin();
        i != vector_.end(); i++) {
      func((*i));
    }
  }

  /** 
   * Apply lambda function to each thread in the group without holding lock
   * 
   * @param func Lambda function or function pointer to apply to each member.  List is locked throughout.
   */
  void for_each_unsafe(std::function<void (boost::thread *)> func) {
    for(std::vector<boost::thread*>::iterator i = vector_.begin();
        i != vector_.end(); i++) {
      func((*i));
    }
  }

  /** 
   * Delete each thread in the group and clear the list.  Holds lock throughout.
   * 
   */
  void clear() {
    boost::lock_guard<boost::mutex> lock(mutex_);

    for(std::vector<boost::thread*>::iterator i = vector_.begin();
        i != vector_.end(); i++) {
      if((*i)!=NULL)
        delete (*i);
    }
    vector_.clear();
  }
};

#endif // __THREAD_GROUP_H__
