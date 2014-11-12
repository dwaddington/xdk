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







#include <mcd_binprot.h>

/**
 *  Dump memcached header
 *  Changhui Lin
 **/

///////////////////////
//Helper functions
////////////////////////
inline
uint64_t htonll(uint64_t _val) {
	uint64_t val= _val;
	char* tmp = (char*)&val;

	int i, len = sizeof(uint64_t)/sizeof(char);
	for(i=0; i<len/2; i++)
	{
		char c = *(tmp+i);
		*(tmp+i) = *(tmp+len-i-1);
		*(tmp+len-i-1) = c;
	}
	return val;
}

inline
uint64_t ntohll(uint64_t _val) {
	return htonll(_val);
}

inline
uint64_t htonll_ptr(uint64_t *buf) {
	return htonll(*buf);
}

inline
uint64_t ntohll_ptr(uint64_t *buf) {
	return ntohll(*buf);
}

/**
 *  Dump memcached header
 **/

void dump_mcd_pkt(mcd_binprot_header_t *h){
	int idx=1, i;
	char *pchar = (char *)h;

	printf("\n===== MEMCACHED HEADER =====\n");

	printf("%2d: magic      = 0x%x\n", idx++, h->magic);
	printf("%2d: opcode     = 0x%02x : %s\n", idx++, h->opcode, mcd_cmd2str(h->opcode));
	printf("%2d: key_len    = %d\n", idx++, ntohs(h->key_len));
	printf("%2d: extras_len = %d\n", idx++, h->extras_len);
	printf("%2d: data_type  = %d\n", idx++, h->data_type);
	printf("%2d: status     = %d\n", idx++, ntohs(h->status) );
	printf("%2d: total_body_len 	= %d\n", idx++, ntohl(h->total_body_len) );
	printf("%2d: opaque     = %d\n", idx++, ntohl(h->opaque) );
	uint64_t cas_val = ntohll(h->cas);
	printf("%2d: cas        = %lu\n", idx++, cas_val );


	/*Dump body*/
	printf("\n===== MEMCACHED BODY =====\n");

	pchar += sizeof(mcd_binprot_header_t);
	printf("%2d: extras (length=%d)\n", idx++, h->extras_len);
	if(h->extras_len>0) {
		for(i=0; i<h->extras_len; i++)
			printf("%c ", *pchar++);
		printf("\n");
	}

	printf("%2d: key (length=%d)\n", idx++, ntohs(h->key_len));
	if(ntohs(h->key_len)>0) {
		for(i=0; i<ntohs(h->key_len); i++)
			printf("%c ", *pchar++);
		printf("\n");
	}

	int val_len = ntohl(h->total_body_len) - h->extras_len - ntohs(h->key_len);
	printf("%2d: value (length=%d)\n", idx++, val_len);
	if(val_len>0) {
		for(i=0; i<val_len; i++)
			printf("%c ", *pchar++);
		printf("\n");
	}

	printf("\n===== MEMCACHED END  =====\n\n");
}

void dump_mcd_pkt_offset(void *data, int offset) {
	mcd_binprot_header_t *h =(mcd_binprot_header_t *) ( (char *)data + offset);
	dump_mcd_pkt(h);
}

