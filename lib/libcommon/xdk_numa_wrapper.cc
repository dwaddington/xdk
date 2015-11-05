/* Copyright (C) 2003,2004 Andi Kleen, SuSE Labs.

   libnuma is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; version
   2.1.

   libnuma is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should find a copy of v2.1 of the GNU Lesser General Public License
   somewhere on your Linux system; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

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
  Copyright (C) 2015, Jilong Kuang <jilong.kuang@samsung.com>
*/

#include "common/xdk_numa_wrapper.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/mman.h>
#include <limits.h>

#define array_len(x) (sizeof(x)/sizeof(*(x)))
#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define CPU_BYTES(x) (round_up(x, BITS_PER_LONG)/8)
#define CPU_LONGS(x) (CPU_BYTES(x) / sizeof(long))
enum numa_warn {
        W_nosysfs,
        W_noproc,
        W_badmeminfo,
        W_nosysfs2,
        W_cpumap,
        W_numcpus,
        W_noderunmask,
        W_distance,
        W_memory,
        W_cpuparse,
        W_nodeparse,
        W_blockdev1,
        W_blockdev2,
        W_blockdev3,
        W_blockdev4,
        W_blockdev5,
        W_netlink1,
        W_netlink2,
        W_netlink3,
        W_net1,
        W_net2,
        W_class1,
        W_class2,
        W_pci1,
        W_pci2,
        W_node_parse1,
        W_node_parse2,
        W_nonode,
        W_badchar,
};

#define howmany(x,y) (((x)+((y)-1))/(y))
#define bitsperlong (8 * sizeof(unsigned long))
#define bitsperint (8 * sizeof(unsigned int))
#define longsperbits(n) howmany(n, bitsperlong)
#define bytesperbits(x) ((x+7)/8)


/* these are the old (version 1) masks */
nodemask_t numa_no_nodes;
nodemask_t numa_all_nodes;
/* these are now the default bitmask (pointers to) (version 2) */
struct bitmask *numa_no_nodes_ptr = NULL;
struct bitmask *numa_all_nodes_ptr = NULL;
struct bitmask *numa_possible_nodes_ptr = NULL;
struct bitmask *numa_all_cpus_ptr = NULL;
struct bitmask *numa_possible_cpus_ptr = NULL;
/* I would prefer to use symbol versioning to create v1 and v2 versions
   of numa_no_nodes and numa_all_nodes, but the loader does not correctly
   handle versioning of BSS versus small data items */

struct bitmask *numa_nodes_ptr = NULL;
static struct bitmask *numa_memnode_ptr = NULL;
static unsigned long *node_cpu_mask_v1[NUMA_NUM_NODES];
struct bitmask **node_cpu_mask_v2;

int numa_available(void)
{
  return 0;
}

static __thread int bind_policy = MPOL_BIND;
static __thread unsigned int mbind_flags = 0;
static int sizes_set=0;
static int maxconfigurednode = -1;
static int maxconfiguredcpu = -1;
static int numprocnode = -1;
static int numproccpu = -1;
static int nodemask_sz = 0;
static int cpumask_sz = 0;

int numa_exit_on_error = 0;
int numa_exit_on_warn = 0;
static void set_sizes(void);

/*
 * The following bitmask declarations, bitmask_*() routines, and associated
 * _setbit() and _getbit() routines are:
 * Copyright (c) 2004_2007 Silicon Graphics, Inc. (SGI) All rights reserved.
 * SGI publishes it under the terms of the GNU General Public License, v2,
 * as published by the Free Software Foundation.
 */
static unsigned int
_getbit(const struct bitmask *bmp, unsigned int n)
{
  if (n < bmp->size)
    return (bmp->maskp[n/bitsperlong] >> (n % bitsperlong)) & 1;
  else
    return 0;
}

static void
_setbit(struct bitmask *bmp, unsigned int n, unsigned int v)
{
  if (n < bmp->size) {
    if (v)
      bmp->maskp[n/bitsperlong] |= 1UL << (n % bitsperlong);
    else
      bmp->maskp[n/bitsperlong] &= ~(1UL << (n % bitsperlong));
  }
}

int
numa_bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
  return _getbit(bmp, i);
}

struct bitmask *
numa_bitmask_setall(struct bitmask *bmp)
{
  unsigned int i;
  for (i = 0; i < bmp->size; i++)
    _setbit(bmp, i, 1);

  return bmp;
}

struct bitmask *
numa_bitmask_clearall(struct bitmask *bmp)
{
  unsigned int i;
  for (i = 0; i < bmp->size; i++)
    _setbit(bmp, i, 0);
  return bmp;
}

struct bitmask *
numa_bitmask_setbit(struct bitmask *bmp, unsigned int i)
{
  _setbit(bmp, i, 1);
  return bmp;
}
struct bitmask *
numa_bitmask_clearbit(struct bitmask *bmp, unsigned int i)
{
  _setbit(bmp, i, 0);
  return bmp;
}

unsigned int
numa_bitmask_nbytes(struct bitmask *bmp)
{
  return longsperbits(bmp->size) * sizeof(unsigned long);
}

/* where n is the number of bits in the map */
/* This function should not exit on failure, but right now we cannot really
   recover from this. */
struct bitmask *
numa_bitmask_alloc(unsigned int n)
{
  struct bitmask *bmp;

  if (n < 1) {
    numa_error((char*)"request to allocate mask for invalid number; abort\n");
    exit(1);
  }
  bmp = (bitmask*)malloc(sizeof(*bmp));
  if (bmp == 0)
    goto oom;
  bmp->size = n;
  bmp->maskp = (long unsigned int*)calloc(longsperbits(n), sizeof(unsigned long));
  if (bmp->maskp == 0) {
    free(bmp);
    goto oom;
  }
  return bmp;

oom:
  numa_error((char*)"Out of memory allocating bitmask");
  exit(1);
}

void
numa_bitmask_free(struct bitmask *bmp)
{
  if (bmp == 0)
    return;
  free(bmp->maskp);
  bmp->maskp = (unsigned long *)0xdeadcdef;  /* double free tripwire */
  free(bmp);
  return;
}

/* True if two bitmasks are equal */
int
numa_bitmask_equal(const struct bitmask *bmp1, const struct bitmask *bmp2)
{
  unsigned int i;
  for (i = 0; i < bmp1->size || i < bmp2->size; i++)
    if (_getbit(bmp1, i) != _getbit(bmp2, i))
      return 0;
  return 1;
}

/* Hamming Weight: number of set bits */
unsigned int 
numa_bitmask_weight(const struct bitmask *bmp)
{
  unsigned int i;
  unsigned int w = 0;
  for (i = 0; i < bmp->size; i++)
    if (_getbit(bmp, i))
      w++;
  return w;
}

/* Next two can be overwritten by the application for different error handling */
void 
numa_error(char *where)
{
  int olde = errno;
  perror(where);
  if (numa_exit_on_error)
    exit(1);
  errno = olde;
}

void 
numa_warn(int num, char *fmt, ...)
{
  static unsigned warned;
  va_list ap;
  int olde = errno;

  /* Give each warning only once */
  if ((1<<num) & warned)
    return;
  warned |= (1<<num);

  va_start(ap,fmt);
  fprintf(stderr, "libnuma: Warning: ");
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);

  errno = olde;
}

static void 
setpol(int policy, struct bitmask *bmp)
{
  printf("%s: setpol NOT IMPLEMENTED!\n", __PRETTY_FUNCTION__);
}

static void 
getpol(int *oldpolicy, struct bitmask *bmp)
{
  printf("%s: getpol NOT IMPLEMENTED!\n", __PRETTY_FUNCTION__);
}

static void 
dombind(void *mem, size_t size, int pol, struct bitmask *bmp)
{              
  printf("%s: dombind NOT IMPLEMENTED!\n", __PRETTY_FUNCTION__);
}

/*
 * Find nodes (numa_nodes_ptr), nodes with memory (numa_memnode_ptr)
 * and the highest numbered existing node (maxconfigurednode).
 */
static void
set_configured_nodes(void)
{
  maxconfigurednode = 0;
}

/*
 * Convert the string length of an ascii hex mask to the number
 * of bits represented by that mask.
 */
static int 
s2nbits(const char *s)
{
  return strlen(s) * 32 / 9;
}

/* Is string 'pre' a prefix of string 's'? */
static int 
strprefix(const char *s, const char *pre)
{
  return strncmp(s, pre, strlen(pre)) == 0;
}

static void
set_nodemask_size(void)
{
  nodemask_sz = 1;
}

/*
 * Read a mask consisting of a sequence of hexadecimal longs separated by
 * commas. Order them correctly and return the number of bits set.
 */
static int
read_mask(char *s, struct bitmask *bmp)
{
  char *end = s;
  int tmplen = (bmp->size + bitsperint - 1) / bitsperint;
  unsigned int tmp[tmplen];
  unsigned int *start = tmp;
  unsigned int i, n = 0, m = 0;

  if (!s)
    return 0;       /* shouldn't happen */

  i = strtoul(s, &end, 16);

  /* Skip leading zeros */
  while (!i && *end++ == ',') {
    i = strtoul(end, &end, 16);
  }

  if (!i)
    /* End of string. No mask */
    return -1;

  start[n++] = i;
  /* Read sequence of ints */
  while (*end++ == ',') {
    i = strtoul(end, &end, 16);
    start[n++] = i;

    /* buffer overflow */
    if (n > tmplen)
      return -1;
  }

  /*
   * Invert sequence of ints if necessary since the first int
   * is the highest and we put it first because we read it first.
   */
  while (n) {
    int w;
    unsigned long x = 0;
    /* read into long values in an endian-safe way */
    for (w = 0; n && w < bitsperlong; w += bitsperint)
      x |= ((unsigned long)start[n-- - 1] << w);

    bmp->maskp[m++] = x;
  }
  /*
   * Return the number of bits set
   */
  return numa_bitmask_weight(bmp);
}

/*
 * Read a processes constraints in terms of nodes and cpus from
 * /proc/self/status.
 */
static void
set_task_constraints(void)
{
  printf("%s: set_task_constraints NOT IMPLEMENTED!\n", __PRETTY_FUNCTION__);
}

/*
 * Find the highest cpu number possible (in other words the size
 * of a kernel cpumask_t (in bits) - 1)
 */
static void
set_numa_max_cpu(void)
{
  unsigned ncpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (ncpus % 8 == 0)
    cpumask_sz = ncpus;
  else
    cpumask_sz = (ncpus/8 + 1) * 8;
}

int
numa_node_of_cpu(int cpu)
{
  return 0;
}

/*
 * copy a bitmask map body to another bitmask body
 * fill a larger destination with zeroes
 */
void
copy_bitmask_to_bitmask(struct bitmask *bmpfrom, struct bitmask *bmpto)
{
  int bytes;
 
  if (bmpfrom->size >= bmpto->size) {
    memcpy(bmpto->maskp, bmpfrom->maskp, CPU_BYTES(bmpto->size));
  } else if (bmpfrom->size < bmpto->size) {
    bytes = CPU_BYTES(bmpfrom->size);
    memcpy(bmpto->maskp, bmpfrom->maskp, bytes);
    memset(((char *)bmpto->maskp)+bytes, 0, CPU_BYTES(bmpto->size)-bytes);
  }
}

int
numa_node_to_cpus(int node, struct bitmask *buffer)
{
  if (node != 0) {
    printf("%s: force numa node to 0!\n", __PRETTY_FUNCTION__);
    node = 0;
  }

  struct bitmask *mask;
  mask = numa_allocate_cpumask();
  numa_bitmask_setall(mask);

  numa_bitmask_clearall(buffer);
  copy_bitmask_to_bitmask(mask, buffer);
  numa_bitmask_free(mask);

  return 0;
}

void 
numa_tonode_memory(void *mem, size_t size, int node)
{
  if (node != 0) {
    printf("%s: force numa node to 0!\n", __PRETTY_FUNCTION__);
    node = 0;
  }
  struct bitmask *nodes;

  nodes = numa_allocate_nodemask();
  numa_bitmask_setbit(nodes, node);
  dombind(mem, size, bind_policy, nodes);
  numa_bitmask_free(nodes);
}

int 
numa_num_configured_nodes(void)
{
  return 1;
}

int 
numa_num_configured_cpus(void)
{
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int 
numa_num_possible_nodes(void)
{
  return 1;
}

int 
numa_num_possible_cpus(void)
{
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int 
numa_num_task_nodes(void)
{
  return 1;
}

int 
numa_num_thread_nodes(void)
{
  return 1;
}

int 
numa_num_task_cpus(void)
{
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int 
numa_num_thread_cpus(void)
{
  return sysconf(_SC_NPROCESSORS_ONLN);
}

/**
 * numa_max_node() returns the highest node number available on the current system. 
 * (See the node numbers in /sys/devices/system/node/ ).  
 * Also see numa_num_configured_nodes().
 */
int 
numa_max_node(void)
{
  return 0;
}

/**
 * numa_allocate_cpumask() using size numa_num_possible_cpus().  
 * The set of cpus to record is derived from /proc/self/status, field "Cpus_allowed".  
 * The user should not alter  this  bitmask.
 */
struct bitmask* 
numa_allocate_cpumask()
{
  int ncpus = sysconf(_SC_NPROCESSORS_ONLN);

  return numa_bitmask_alloc(ncpus);
}

/**
 * numa_allocate_nodemask() returns a bitmask of a size equal to the kernel's node mask 
 * (kernel type nodemask_t).  In other words, large enough to represent MAX_NUMNODES nodes.  
 * This  number  of  nodes  can  be  gotten  by calling numa_num_possible_nodes().  
 * The bitmask is zero-filled.
 */
struct bitmask* 
numa_allocate_nodemask(void)
{
  struct bitmask *bmp;
  int nnodes = 1;

  bmp = numa_bitmask_alloc(nnodes);
  return bmp;
}

void *
numa_alloc_onnode(size_t size, int node)
{
  if (node != 0) {
    printf("%s: force numa node to 0!\n", __PRETTY_FUNCTION__);
    node = 0;
  }

  char *mem;
  struct bitmask *bmp;

  bmp = numa_allocate_nodemask();
  numa_bitmask_setbit(bmp, node);
  mem = (char*)mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
             0, 0);
  if (mem == (char *)-1)
    mem = NULL;
  else
    dombind(mem, size, bind_policy, bmp);
  numa_bitmask_free(bmp);
  return mem;
}

void 
numa_free(void *mem, size_t size)
{
  munmap(mem, size);
}

