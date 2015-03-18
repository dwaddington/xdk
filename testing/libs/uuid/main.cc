#include <common/logging.h>
#include <component/base.h>

class MyComponent
{
public:
  /* test UUID declaration macros */
  DECLARE_INTERFACE_UUID(0x12345678,0x1122,0x3344,0x5566,0x77,0x88,0x99,0x01,0x02,0x03);
  DECLARE_COMPONENT_UUID(0x2aed073f,0x8696,0x4766,0xbc44,0xe6,0x76,0x43,0x9e,0x20,0xd6);
};

int main()
{
  MyComponent a;

  Component::uuid_t i = MyComponent::iid();
  
  PINF("Testing...");
  std::string s = i.toString();
  PINF("iid to string:[%s]", s.c_str());

  printf("len=%ld\n",strlen("12345678-1122-3344-5566-778899010203"));
  Component::uuid_t j;
  j.fromString(s);
  PINF("iid to string:[%s]", j.toString().c_str());

  PINF("Test OK.");
  return 0;
}

