#include <common/logging.h>
#include <common/cycles.h>

int main()
{
  PINF("Clock Frequency MHz: %g", get_tsc_frequency_in_mhz());
  return 0;
}

