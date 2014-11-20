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

#include <stdio.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/predicate.hpp>

void grant_files(std::string& path, uid_t owner);

int main(int argc, char * argv[])
{

  namespace po = boost::program_options; 
  po::options_description desc("Options"); 
  desc.add_options() 
    ("help", "Print help messages") 
    ("list", "List PCI devices")
    ("pciaddr", po::value<std::string>(), "PCI device address (e.g., 00:1f.3)")
    ("uid", po::value< int >(), "User id")
    ;
    
  po::variables_map vm; 
  po::store(po::parse_command_line(argc, argv, desc), vm); // can throw 

  if(vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  if(vm.count("list")) {
    system("lspci");
    return 1;
  }

  if(!vm.count("uid")) {
    std::cout << "Must specify user id with --uid option.\n";
    std::cout << desc << "\n";
    return 1;
  }

  if(!vm.count("pciaddr")) {
    std::cout << "Must specifiy pciaddr\n";
    std::cout << desc << "\n";
    return 1;
  }

  int uid = vm["uid"].as<int>();

  std::string path = "/sys/bus/pci/devices/0000:";
  path += vm["pciaddr"].as<std::string>();
  grant_files(path, uid);

  return 0;
}


void grant_files(std::string& path, uid_t owner) 
{
  std::cout << "Grant path: " << path << std::endl;
  std::cout << "UID: " << owner << std::endl;

  /* config file */
  int rc;
  std::string path_config = path + "/config";
  rc = chown(path_config.c_str(), owner, owner);
  if(rc) {
    std::cout << "Unable to chown (" << path_config << "). Running as root?\n";
    return;
  }

  /* resource files */
  boost::filesystem::path fsDirectoryPath =
    boost::filesystem::system_complete(boost::filesystem::path(path));
  boost::filesystem::directory_iterator end_iter;

  for (boost::filesystem::directory_iterator dir_itr(fsDirectoryPath);
       dir_itr != end_iter; ++dir_itr) {
    if (boost::contains((*dir_itr).path().native(), "resource")) {
      const char * filename = (*dir_itr).path().c_str();
      
      rc = chown(filename, owner, owner);
      if(rc) {
        std::cout << "Unable to chown (" << path_config << "). Running as root?\n";
        return;
      }

    }
  } 
  



}
