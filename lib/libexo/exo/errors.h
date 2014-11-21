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

#ifndef __EXO_ERRORS_H__
#define __EXO_ERRORS_H__

#include <string>
#include <iostream>

#define ERROR_ENUMS   enum {                                            \
    S_OK = 0,                                                           \
    E_FAIL = -1,                                                        \
    E_INVALID_REQUEST = -2,                                             \
    E_INVAL = -2,                                                       \
    E_INSUFFICIENT_QUOTA = -3,                                          \
    E_NOT_FOUND = -4,                                                   \
    E_INSUFFICIENT_RESOURCES = -5,                                      \
    E_NO_RESOURCES = -6,                                                \
    E_INSUFFICIENT_SPACE = -7,                                          \
    E_USER_ERROR = -8, /*used as the base for in-class defined errors*/ \
    E_BUSY = -9,                                                        \
    E_TAKEN = -10,                                                      \
    E_LENGTH_EXCEEDED = -11,                                            \
    E_BAD_OFFSET = -12,                                                 \
    E_BAD_PARAM = -13,                                                  \
    E_NO_MEM = -14,                                                     \
    E_NOT_SUPPORTED = -15,                                              \
    E_OUT_OF_BOUNDS = -16,                                              \
    E_NOT_INITIALIZED = -17,                                            \
    E_NOT_IMPL = -18,                                                   \
    E_NOT_ENABLED = -19,                                                \
    E_SEND_TIMEOUT = -20,                                               \
    E_RECV_TIMEOUT = -21,                                               \
    E_BAD_FILE = -22,                                                   \
    E_FULL = -23,                                                       \
    E_EMPTY = -24,                                                      \
    E_INVALID_ARG = -25,                                                \
    E_ERROR_BASE = -50,                                                 \
  };

ERROR_ENUMS; /* add to global namespace also */

namespace Exokernel
{
  ERROR_ENUMS;

  class Exception
  {
  private:
    char _cause[256];
  public:
    Exception() {
    }
    Exception(const char * cause) {
      __builtin_strncpy(_cause, cause, 256);
    }
    const char * cause() const {
      return _cause;
    }
      
  };

  class Constructor_error : public Exception
  {
  private:
    int _err_code;

  public:
    Constructor_error() : Exception("Ctor failed"), _err_code(E_FAIL) {
    }

    Constructor_error(int err) : Exception("Ctor failed"), _err_code(err) {
    }

  };

  class Error_no_device : public Exception
  {
  public:
    Error_no_device() : Exception("no device") {}
  };

  class Fatal
  {
  private:
    std::string _file,_cause;
    int _line;

  public:
    Fatal() {
    }

    Fatal(const char * file, int line, const char * cause) :
      _file(file), _cause(cause), _line(line) {
    }

    const char * cause() const {
      return _cause.c_str();
    }

    void abort() {
      std::cerr << "FATAL-ERROR: [" << _file << ":" << _line << "] " << _cause << std::endl;
    }

  };
}


#endif
