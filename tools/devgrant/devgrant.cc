#include <stdio.h>
#include <unistd.h>

#include <boost/program_options.hpp>

void grant_file(std::string& path, uid_t owner);

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
  grant_file(path, uid);

  return 0;
}


void grant_file(std::string& path, uid_t owner) 
{
  std::cout << "Grant path: " << path << std::endl;
  std::cout << "UID: " << owner << std::endl;

  /* config file */
  int rc;
  std::string path_config = path + "/config";
  rc = chown(path_config.c_str(), owner, owner);
  if(rc) {
    std::cout << "Unable to chown (" << path_config << "). Running as root?\n";
  }
  else {
    std::cout << "Chowned " << path_config << " OK." << std::endl;
  }
}
