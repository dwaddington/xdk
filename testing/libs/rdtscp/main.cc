#include <unistd.h>

#include <common/types.h>
#include <common/cycles.h>
#include <common/logging.h>

/** 
 * Test program for rdtscp functions in common/cycles.h.
 *
 * To verify that the returned values of aux, socket_id and cpu_id are correct,
 * you can use the 'tasklet' tool as follows:
 * + $> tasklet <COREMASK> <EXECUTABLE>
 *
 * For example:
 * + $> tasklet 0x2 ./rdtscp-test.
 *
 * To install taskset on Debian, Ubuntu or Linux Mint:
 * $> sudo apt-get install util-linux
 *
 * To install taskset on Fedora, CentOS or RHEL:
 * $> sudo yum install util-linux 
 */
int main() {

  PINF("Testing...");

  uint32_t aux = 0;
  cpu_time_t t0 = rdtscp(aux);
  PINF("t0 = %lu, aux = %d", t0, aux);


  usleep(10*1000);

  uint32_t socket_id = 0; 
  uint32_t cpu_id = 0; 
  cpu_time_t t1 = rdtscp(socket_id, cpu_id);
  PINF("t1 = %lu, socket_id = %d, cpu_id = %d", t1, socket_id, cpu_id);

  PINF("Test OK.");
  return 0;
}

