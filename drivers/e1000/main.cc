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
          @author Chen (chen.tian@samsung.com)
          @author Jilong Kuang (jilong.kuang@samsung.com)
          @date: Aug 28, 2012
*/

#include "e1000_card.h"
#include <namespace.h>
#include <ipc_stream>

using namespace OmniOS;
using namespace E1000;

#define PACKET_LEN    80
unsigned char packet[PACKET_LEN] = { // a ping packet
	//0xb8, 0xac, 0x6f, 0x8e, 0xbb, 0x6e, // dst mac, 00:24:54:6e:f6:3a
	0x00, 0x24, 0x54, 0x6e, 0xf6, 0x3a, // dst mac, 00:24:54:6e:f6:3a
	0x00, 0x07, 0xe9, 0x11, 0x3d, 0x3d, // real dst mac, 00:07:e9:11:3d:3d
	0x08, 0x00, // type: ip datagram
	0x45, 0x00, // version
	0x00, 0x1c, // length
	0x00, 0x05, 0x40, 0x00, 0x3f, 0x01, 0xb1, 0x64,//ip header
	0x69, 0x90, 0x1d, 0x6d, // 105.144.29.109
	0xc0, 0xa8, 0x05, 0xff, // 192.168.5.255
	0x08, 0x00, 0x8a, 0x8f, 0x6d, 0x6b, 0x00, 0x05, // ICMP header
	0x00, 0x00 };


void send_pkt_test(E1000_card *dev) {
  uint32_t tmp_v, tmp_p;
  tmp_v = (uint32_t)env()->alloc_pages(1, EAGER_MAPPING, (addr_t *)&tmp_p);
  Address pkt_addr(tmp_v, tmp_p);

  for(int i=0; i<6; i++) {
      packet[6+i] = dev->mac[i];
  }

  memcpy((void *)pkt_addr.v(), (void*)packet, PACKET_LEN);
  printf("\n");
  panic("Press \'g\' to send a packet\n");
  for(int i=0; i<128; i++) {
    dev->send(pkt_addr.p(), PACKET_LEN);
  }
  env()->free((void* )pkt_addr.v());
  printf("%d packets are sent OK..\n", 128);
}



void receive_pkt(E1000_card *dev) {
 int err;
 OmniOS::Irq irq;
 OmniOS::Icu icu;

 int irqno;
 if (dev->use_msi) {
   int msi_irq_num = 0x0; //needs an allocator in app_core
   uint32_t msg;
   icu.msi_info((unsigned) (msi_irq_num |0x80000000), (l4_umword_t*)&msg);
   if (!dev->config_msi(msg)) {
      panic("failed to config msi");
   }
   irqno = msi_irq_num | 0x80000000;
   info("\tMSI irq_no 0x%x \n", msi_irq_num);
 }
 else {
   irqno = dev->irq_num;
   info("irq_no 0x%x is used.\n", irqno);
 }
 err= icu.bind(irqno, irq.cap());
 if (err) {
	  panic("Fail to bind irq num %x through icu [err=%d]\n", irqno, err);
 }

 if (irq.attach(env()->self().cap())!=0) {
	  panic("irq_attach failed\n");
 }

 channel_t p;
 p.open("channel_e1000_nic");
 assert(p.check_magic());
 p.wait_for_consumer_ready();
 dev->register_channel(&p);
 printf("[e1000]: Channel is opened for consumer...\n");

 dev->activate();
 //send_pkt_test(dev);
 printf("[e1000]: Ready for receiving ethernet packets...\n");
 while (1) {
    err= irq.receive(L4_IPC_NEVER);
    if (err!=0) printf("irq_receive error %d\n",err);
    dev->interrupt_handler();
 }
}

void service_request(E1000_card *dev) {
  (void)dev;
  printf("[e1000]: Ready for receiving application requests ...\n");
  
  /* lets use the C++ iostream classes */
  Ipc::Iostream ipc(l4_utcb());

  l4_umword_t label = 0;
  unsigned val;
  unsigned len;

  ipc.wait(&label, L4_IPC_NEVER); // perform open wait

  while (1) {
    ipc >> val;
    if (val == 0xFFFFFFFF) {
      ipc >> dev->ip[0];
      ipc >> dev->ip[1];
      ipc >> dev->ip[2];
      ipc >> dev->ip[3];
      for (int i=0; i<6; i++)
        ipc << dev->mac[i];
      info("[e1000]:IP config request %x:%x:%x:%x:%x:%x->%d.%d.%d.%d\n", 
      dev->mac[0], dev->mac[1], dev->mac[2], 
      dev->mac[3], dev->mac[4], dev->mac[5], 
      dev->ip[0], dev->ip[1], dev->ip[2], dev->ip[3]);
    }
    else {
      ipc >> len;
 //     printf("+ driver received IPC from client (packet p_addr=%x, len %d)!\n",val, len);
      dev->send((uint32_t)val, len);
    }
    ipc.reply_and_wait(&label);
  }
}



int main() {
  E1000_card dev;
  if (!dev.init()) {
    panic("failed to initialize e1000 card");
    return 0;
  }

  thread_t thrd;
  thread_attr_t attr;
  int ret;
 // attr.set_affinity(1);
  for (int i=0; i<1; i++) {
   ret = env()->create_thread(&thrd, &attr, 
	  (entry_func_t)receive_pkt, (entry_arg_t)&dev);
  if (ret)
	  panic("failed to create receiving thread !\n");
  }

  dev.wait_for_activate();

  Name_space_table ns;
  // announce service with name server
  int rc = ns.announce("e1000_service");
  assert(rc==S_OK);
  service_request(&dev);

  panic("Never reach here.");
  return 0;
}

