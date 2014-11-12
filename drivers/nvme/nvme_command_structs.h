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






#ifndef __NVME_COMMAND_STRUCTS_H__
#define __NVME_COMMAND_STRUCTS_H__

#include <stdint.h>

struct nvme_sub_command_common {
  volatile uint8_t	opcode;
  volatile uint8_t  fuse; //unsigned fuse:2;
  //  volatile unsigned reserved:6;
  volatile uint16_t command_id;
  volatile uint32_t nsid;
  volatile uint32_t cdw2[2];
  volatile uint64_t metadata;
  volatile uint64_t prp1;
  volatile uint64_t prp2;
  volatile uint32_t cdw10;
  volatile uint32_t cdw11;
  volatile uint32_t cdw12;
  volatile uint32_t cdw13;
  volatile uint32_t cdw14;
  volatile uint32_t cdw15;
} __attribute__((packed));

struct nvme_comp_command_common {
  volatile uint32_t dw0;
  volatile uint32_t dw1;
  volatile uint16_t sq_hdptr;
  volatile uint16_t sq_id;
  volatile uint16_t command_id;
  volatile unsigned phase_tag : 1;
  volatile unsigned status : 15;  
} __attribute__((packed));


struct nvme_id_power_state {
	uint16_t			max_power;	/* centiwatts */
	uint16_t			rsvd2;
	uint32_t			entry_lat;	/* microseconds */
	uint32_t			exit_lat;	/* microseconds */
	uint8_t			read_tput;
	uint8_t			read_lat;
	uint8_t			write_tput;
	uint8_t			write_lat;
	uint8_t			rsvd16[16];
};

#define NVME_VS(major, minor)	(major << 16 | minor)

struct nvme_id_ctrl {
	uint16_t	vid;
	uint16_t	ssvid;
	char			sn[20];
	char			mn[40];
	char			fr[8];
	uint8_t		rab;
	uint8_t		ieee[3];
	uint8_t		mic;
	uint8_t		mdts;
	uint8_t		rsvd78[178];
	uint16_t	oacs;
	uint8_t		acl;
	uint8_t		aerl;
	uint8_t		frmw;
	uint8_t		lpa;
	uint8_t		elpe;
	uint8_t		npss;
	uint8_t		rsvd264[248];
	uint8_t		sqes;
	uint8_t		cqes;
	uint8_t		rsvd514[2];
	uint32_t	nn;
	uint16_t  oncs;
	uint16_t	fuses;
	uint8_t		fna;
	uint8_t		vwc;
	uint16_t	awun;
	uint16_t	awupf;
	uint8_t		rsvd530[1518];
	struct nvme_id_power_state	psd[32];
	uint8_t		vs[1024];
};

enum {
	NVME_CTRL_ONCS_COMPARE			= 1 << 0,
	NVME_CTRL_ONCS_WRITE_UNCORRECTABLE	= 1 << 1,
	NVME_CTRL_ONCS_DSM			= 1 << 2,
};


struct nvme_lbaf {
	uint16_t			ms;
	uint8_t			ds;
	uint8_t			rp;
};

struct nvme_id_ns {
	uint64_t			nsze;
	uint64_t			ncap;
	uint64_t			nuse;
	uint8_t			nsfeat;
	uint8_t			nlbaf;
	uint8_t			flbas;
	uint8_t			mc;
	uint8_t			dpc;
	uint8_t			dps;
	uint8_t			rsvd30[98];
	struct nvme_lbaf	lbaf[16];
	uint8_t			rsvd192[192];
	uint8_t			vs[3712];
};

enum {
	NVME_NS_FEAT_THIN	= 1 << 0,
	NVME_LBAF_RP_BEST	= 0,
	NVME_LBAF_RP_BETTER	= 1,
	NVME_LBAF_RP_GOOD	= 2,
	NVME_LBAF_RP_DEGRADED	= 3,
};

struct nvme_smart_log {
	uint8_t			critical_warning;
	uint8_t			temperature[2];
	uint8_t			avail_spare;
	uint8_t			spare_thresh;
	uint8_t			percent_used;
	uint8_t			rsvd6[26];
	uint8_t			data_units_read[16];
	uint8_t			data_units_written[16];
	uint8_t			host_reads[16];
	uint8_t			host_writes[16];
	uint8_t			ctrl_busy_time[16];
	uint8_t			power_cycles[16];
	uint8_t			power_on_hours[16];
	uint8_t			unsafe_shutdowns[16];
	uint8_t			media_errors[16];
	uint8_t			num_err_log_entries[16];
	uint8_t			rsvd192[320];
};

enum {
	NVME_SMART_CRIT_SPARE		= 1 << 0,
	NVME_SMART_CRIT_TEMPERATURE	= 1 << 1,
	NVME_SMART_CRIT_RELIABILITY	= 1 << 2,
	NVME_SMART_CRIT_MEDIA		= 1 << 3,
	NVME_SMART_CRIT_VOLATILE_MEMORY	= 1 << 4,
};

struct nvme_lba_range_type {
	uint8_t			type;
	uint8_t			attributes;
	uint8_t			rsvd2[14];
	uint64_t			slba;
	uint64_t			nlb;
	uint8_t			guid[16];
	uint8_t			rsvd48[16];
};

enum {
	NVME_LBART_TYPE_FS	= 0x01,
	NVME_LBART_TYPE_RAID	= 0x02,
	NVME_LBART_TYPE_CACHE	= 0x03,
	NVME_LBART_TYPE_SWAP	= 0x04,

	NVME_LBART_ATTRIB_TEMP	= 1 << 0,
	NVME_LBART_ATTRIB_HIDE	= 1 << 1,
};

/* I/O commands */

enum nvme_opcode {
	nvme_cmd_flush		= 0x00,
	nvme_cmd_write		= 0x01,
	nvme_cmd_read		= 0x02,
	nvme_cmd_write_uncor	= 0x04,
	nvme_cmd_compare	= 0x05,
	nvme_cmd_dsm		= 0x09,
};


struct nvme_rw_command {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			nsid;
	uint64_t			rsvd2;
	uint64_t			metadata;
	uint64_t			prp1;
	uint64_t			prp2;
	uint64_t			slba;
	uint16_t			length;
	uint16_t			control;
	uint32_t			dsmgmt;
	uint32_t			reftag;
	uint16_t			apptag;
	uint16_t			appmask;
};

struct nvme_io_dsm {
  unsigned access_freq    : 4;
  unsigned access_lat     : 2;
  unsigned seq_request    : 1;
  unsigned incompressible : 1;
} __attribute__((packed));

enum {
	NVME_RW_LR			= 1 << 15,
	NVME_RW_FUA			= 1 << 14,
	NVME_RW_DSM_FREQ_UNSPEC		= 0,
	NVME_RW_DSM_FREQ_TYPICAL	= 1,
	NVME_RW_DSM_FREQ_RARE		= 2,
	NVME_RW_DSM_FREQ_READS		= 3,
	NVME_RW_DSM_FREQ_WRITES		= 4,
	NVME_RW_DSM_FREQ_RW		= 5,
	NVME_RW_DSM_FREQ_ONCE		= 6,
	NVME_RW_DSM_FREQ_PREFETCH	= 7,
	NVME_RW_DSM_FREQ_TEMP		= 8,
	NVME_RW_DSM_LATENCY_NONE	= 0 << 4,
	NVME_RW_DSM_LATENCY_IDLE	= 1 << 4,
	NVME_RW_DSM_LATENCY_NORM	= 2 << 4,
	NVME_RW_DSM_LATENCY_LOW		= 3 << 4,
	NVME_RW_DSM_SEQ_REQ		= 1 << 6,
	NVME_RW_DSM_COMPRESSED		= 1 << 7,
};

struct nvme_dsm_cmd {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			nsid;
	uint64_t			rsvd2[2];
	uint64_t			prp1;
	uint64_t			prp2;
	uint32_t			nr;
	uint32_t			attributes;
	uint32_t			rsvd12[4];
};

enum {
	NVME_DSMGMT_IDR		= 1 << 0,
	NVME_DSMGMT_IDW		= 1 << 1,
	NVME_DSMGMT_AD		= 1 << 2,
};

struct nvme_dsm_range {
	uint32_t			cattr;
	uint32_t			nlb;
	uint64_t			slba;
};

/* Admin commands */

enum nvme_admin_opcode {
	nvme_admin_delete_sq		= 0x00,
	nvme_admin_create_sq		= 0x01,
	nvme_admin_get_log_page		= 0x02,
	nvme_admin_delete_cq		= 0x04,
	nvme_admin_create_cq		= 0x05,
	nvme_admin_identify		= 0x06,
	nvme_admin_abort_cmd		= 0x08,
	nvme_admin_set_features		= 0x09,
	nvme_admin_get_features		= 0x0a,
	nvme_admin_async_event		= 0x0c,
	nvme_admin_activate_fw		= 0x10,
	nvme_admin_download_fw		= 0x11,
	nvme_admin_format_nvm		= 0x80,
	nvme_admin_security_send	= 0x81,
	nvme_admin_security_recv	= 0x82,
};

enum {
	NVME_QUEUE_PHYS_CONTIG	= (1 << 0),
	NVME_CQ_IRQ_ENABLED	= (1 << 1),
	NVME_SQ_PRIO_URGENT	= (0 << 1),
	NVME_SQ_PRIO_HIGH	= (1 << 1),
	NVME_SQ_PRIO_MEDIUM	= (2 << 1),
	NVME_SQ_PRIO_LOW	= (3 << 1),
	NVME_FWACT_REPL		= (0 << 3),
	NVME_FWACT_REPL_ACTV	= (1 << 3),
	NVME_FWACT_ACTV		= (2 << 3),
};

struct nvme_identify {
	uint8_t	opcode;
	uint8_t	flags;
	uint16_t command_id;
	uint32_t nsid;
	uint64_t rsvd2[2];
	uint64_t prp1;
	uint64_t prp2;
	uint32_t cns;
	uint32_t rsvd11[5];
};

struct nvme_features {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			nsid;
	uint64_t			rsvd2[2];
	uint64_t			prp1;
	uint64_t			prp2;
	uint32_t			fid;
	uint32_t			dword11;
	uint32_t			rsvd12[4];
};

struct nvme_create_cq {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			rsvd1[5];
	uint64_t			prp1;
	uint64_t			rsvd8;
	uint16_t			cqid;
	uint16_t			qsize;
	uint16_t			cq_flags;
	uint16_t			irq_vector;
	uint32_t			rsvd12[4];
};

struct nvme_create_sq {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			rsvd1[5];
	uint64_t			prp1;
	uint64_t			rsvd8;
	uint16_t			sqid;
	uint16_t			qsize;
	uint16_t			sq_flags;
	uint16_t			cqid;
	uint32_t			rsvd12[4];
};

struct nvme_delete_queue {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			rsvd1[9];
	uint16_t			qid;
	uint16_t			rsvd10;
	uint32_t			rsvd11[5];
};

struct nvme_download_firmware {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			rsvd1[5];
	uint64_t			prp1;
	uint64_t			prp2;
	uint32_t			numd;
	uint32_t			offset;
	uint32_t			rsvd12[4];
};

struct nvme_format_cmd {
	uint8_t			opcode;
	uint8_t			flags;
	uint16_t			command_id;
	uint32_t			nsid;
	uint64_t			rsvd2[4];
	uint32_t			cdw10;
	uint32_t			rsvd11[5];
};


enum {
	NVME_SC_SUCCESS			= 0x0,
	NVME_SC_INVALID_OPCODE		= 0x1,
	NVME_SC_INVALID_FIELD		= 0x2,
	NVME_SC_CMDID_CONFLICT		= 0x3,
	NVME_SC_DATA_XFER_ERROR		= 0x4,
	NVME_SC_POWER_LOSS		= 0x5,
	NVME_SC_INTERNAL		= 0x6,
	NVME_SC_ABORT_REQ		= 0x7,
	NVME_SC_ABORT_QUEUE		= 0x8,
	NVME_SC_FUSED_FAIL		= 0x9,
	NVME_SC_FUSED_MISSING		= 0xa,
	NVME_SC_INVALID_NS		= 0xb,
	NVME_SC_CMD_SEQ_ERROR		= 0xc,
	NVME_SC_LBA_RANGE		= 0x80,
	NVME_SC_CAP_EXCEEDED		= 0x81,
	NVME_SC_NS_NOT_READY		= 0x82,
	NVME_SC_CQ_INVALID		= 0x100,
	NVME_SC_QID_INVALID		= 0x101,
	NVME_SC_QUEUE_SIZE		= 0x102,
	NVME_SC_ABORT_LIMIT		= 0x103,
	NVME_SC_ABORT_MISSING		= 0x104,
	NVME_SC_ASYNC_LIMIT		= 0x105,
	NVME_SC_FIRMWARE_SLOT		= 0x106,
	NVME_SC_FIRMWARE_IMAGE		= 0x107,
	NVME_SC_INVALID_VECTOR		= 0x108,
	NVME_SC_INVALID_LOG_PAGE	= 0x109,
	NVME_SC_INVALID_FORMAT		= 0x10a,
	NVME_SC_BAD_ATTRIBUTES		= 0x180,
	NVME_SC_WRITE_FAULT		= 0x280,
	NVME_SC_READ_ERROR		= 0x281,
	NVME_SC_GUARD_CHECK		= 0x282,
	NVME_SC_APPTAG_CHECK		= 0x283,
	NVME_SC_REFTAG_CHECK		= 0x284,
	NVME_SC_COMPARE_FAILED		= 0x285,
	NVME_SC_ACCESS_DENIED		= 0x286,
};

struct nvme_completion {
	uint32_t	result;		/* Used by admin commands to return data */
	uint32_t	rsvd;
	uint16_t	sq_head;	/* how much of this queue may be reclaimed */
	uint16_t	sq_id;		/* submission queue that generated this entry */
	uint16_t	command_id;	/* of the command which completed */
	uint16_t	status;		/* did the command fail, and if so, why? */
};

struct nvme_user_io {
	uint8_t	opcode;
	uint8_t	flags;
	uint16_t	control;
	uint16_t	nblocks;
	uint16_t	rsvd;
	uint64_t	metadata;
	uint64_t	addr;
	uint64_t	slba;
	uint32_t	dsmgmt;
	uint32_t	reftag;
	uint16_t	apptag;
	uint16_t	appmask;
};

struct nvme_admin_cmd {
	uint8_t	opcode;
	uint8_t	flags;
	uint16_t	rsvd1;
	uint32_t	nsid;
	uint32_t	cdw2;
	uint32_t	cdw3;
	uint64_t	metadata;
	uint64_t	addr;
	uint32_t	metadata_len;
	uint32_t	data_len;
	uint32_t	cdw10;
	uint32_t	cdw11;
	uint32_t	cdw12;
	uint32_t	cdw13;
	uint32_t	cdw14;
	uint32_t	cdw15;
	uint32_t	timeout_ms;
	uint32_t	result;
};

#define NVME_IOCTL_ID		_IO('N', 0x40)
#define NVME_IOCTL_ADMIN_CMD	_IOWR('N', 0x41, struct nvme_admin_cmd)
#define NVME_IOCTL_SUBMIT_IO	_IOW('N', 0x42, struct nvme_user_io)


// #ifdef __KERNEL__
// #include <linux/pci.h>
// #include <linux/miscdevice.h>
// #include <linux/kref.h>

// #define NVME_IO_TIMEOUT	(5 * HZ)

// /*
//  * Represents an NVM Express device.  Each nvme_dev is a PCI function.
//  */
// struct nvme_dev {
// 	struct list_head node;
// 	struct nvme_queue **queues;
// 	u32 __iomem *dbs;
// 	struct pci_dev *pci_dev;
// 	struct dma_pool *prp_page_pool;
// 	struct dma_pool *prp_small_pool;
// 	int instance;
// 	int queue_count;
// 	int db_stride;
// 	u32 ctrl_config;
// 	struct msix_entry *entry;
// 	struct nvme_bar __iomem *bar;
// 	struct list_head namespaces;
// 	struct kref kref;
// 	struct miscdevice miscdev;
// 	char name[12];
// 	char serial[20];
// 	char model[40];
// 	char firmware_rev[8];
// 	u32 max_hw_sectors;
// 	u32 stripe_size;
// 	u16 oncs;
// };

// /*
//  * An NVM Express namespace is equivalent to a SCSI LUN
//  */
// struct nvme_ns {
// 	struct list_head list;

// 	struct nvme_dev *dev;
// 	struct request_queue *queue;
// 	struct gendisk *disk;

// 	int ns_id;
// 	int lba_shift;
// 	int ms;
// 	u64 mode_select_num_blocks;
// 	u32 mode_select_block_len;
// };

// /*
//  * The nvme_iod describes the data in an I/O, including the list of PRP
//  * entries.  You can't see it in this data structure because C doesn't let
//  * me express that.  Use nvme_alloc_iod to ensure there's enough space
//  * allocated to store the PRP list.
//  */
// struct nvme_iod {
// 	void *private;		/* For the use of the submitter of the I/O */
// 	int npages;		/* In the PRP list. 0 means small pool in use */
// 	int offset;		/* Of PRP list */
// 	int nents;		/* Used in scatterlist */
// 	int length;		/* Of data, in bytes */
// 	dma_addr_t first_dma;
// 	struct scatterlist sg[0];
// };

// static inline u64 nvme_block_nr(struct nvme_ns *ns, sector_t sector)
// {
// 	return (sector >> (ns->lba_shift - 9));
// }

struct nvme_command {
  union {
    struct nvme_sub_command_common common;
    struct nvme_rw_command rw;
    struct nvme_identify identify;
    struct nvme_features features;
    struct nvme_create_cq create_cq;
    struct nvme_create_sq create_sq;
    struct nvme_delete_queue delete_queue;
    struct nvme_download_firmware dlfw;
    struct nvme_format_cmd format;
    struct nvme_dsm_cmd dsm;
  };
};

#endif // __NVME_COMMAND_STRUCTS_H__
