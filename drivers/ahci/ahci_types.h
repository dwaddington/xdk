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
  @author Daniel Waddington (d.waddington@samsung.com)
  @date: June 26, 2012
*/

#ifndef __AHCI_TYPES_H__
#define __AHCI_TYPES_H__


#define HOST_IRQ_STAT 0x44

#define NR_PORTS 32		/* maximum number of ports */
#define NR_CMDS 32		/* maximum number of queued commands */

/* Time values that can be set with options. */
#define SPINUP_TIMEOUT		5000	/* initial spin-up time (ms) */
#define SIG_TIMEOUT		250	/* time between signature checks (ms) */
#define NR_SIG_CHECKS		60	/* maximum number of times to check */
#define COMMAND_TIMEOUT		10000	/* time to wait for non-I/O cmd (ms) */
#define TRANSFER_TIMEOUT	30000	/* time to wait for I/O cmd (ms) */
#define FLUSH_TIMEOUT		60000	/* time to wait for flush cmd (ms) */

/* Time values that are defined by the standards. */
#define SPINUP_DELAY		1	/* time to assert spin-up flag (ms) */
#define RESET_DELAY		1000	/* maximum HBA reset time (ms) */
#define PORTREG_DELAY		500	/* maximum port register update (ms) */

/* Generic FIS layout. */
#define ATA_FIS_TYPE			0	/* FIS Type */
#define 	ATA_FIS_TYPE_H2D	0x27	/* Register - Host to Device */

/* Register - Host to Device FIS layout. */
#define ATA_H2D_SIZE			20	/* byte size of H2D FIS */
#define ATA_H2D_FLAGS			1	/* C and PM Port */
#define ATA_H2D_FLAGS_C		0x80	/* update command register */
#define ATA_H2D_CMD			2	/* Command */
#define ATA_CMD_READ_DMA_EXT	0x25	/* READ DMA EXT */
#define ATA_CMD_WRITE_DMA_EXT	0x35	/* WRITE DMA EXT */
#define ATA_CMD_READ_FPDMA_QUEUED	0x60	/* READ FPDMA QUEUED */
#define ATA_CMD_WRITE_FPDMA_QUEUED	0x61	/* WRITE FPDMA QUEUED */
#define ATA_CMD_WRITE_DMA_FUA_EXT	0x3D	/* WRITE DMA FUA EXT */
#define ATA_CMD_PACKET		0xA0	/* PACKET */
#define ATA_CMD_IDENTIFY_PACKET	0xA1	/* IDENTIFY PACKET DEVICE */
#define ATA_CMD_FLUSH_CACHE	0xE7	/* FLUSH CACHE */
#define ATA_CMD_IDENTIFY	0xEC	/* IDENTIFY DEVICE */
#define ATA_CMD_SET_FEATURES	0xEF	/* SET FEATURES */
#define ATA_H2D_FEAT			3	/* Features */
#define ATA_FEAT_PACKET_DMA	0x01	/* use DMA */
#define ATA_FEAT_PACKET_DMADIR	0x03	/* DMA is inbound */
#define ATA_H2D_LBA_LOW			4	/* LBA Low */
#define ATA_H2D_LBA_MID			5	/* LBA Mid */
#define ATA_H2D_LBA_HIGH		6	/* LBA High */
#define ATA_H2D_DEV			7	/* Device */
#define ATA_DEV_LBA		0x40	/* use LBA addressing */
#define ATA_DEV_FUA		0x80	/* Force Unit Access (FPDMA) */
#define ATA_H2D_LBA_LOW_EXP		8	/* LBA Low (exp) */
#define ATA_H2D_LBA_MID_EXP		9	/* LBA Mid (exp) */
#define ATA_H2D_LBA_HIGH_EXP		10	/* LBA High (exp) */
#define ATA_H2D_FEAT_EXP		11	/* Features (exp) */
#define ATA_H2D_SEC			12	/* Sector Count */
#define ATA_SEC_TAG_SHIFT	3	/* NCQ command tag */
#define ATA_H2D_SEC_EXP			13	/* Sector Count (exp) */
#define ATA_H2D_CTL			15	/* Control */

#define ATA_IS_FPDMA_CMD(c)                     \
	((c) == ATA_CMD_READ_FPDMA_QUEUED ||          \
	 (c) == ATA_CMD_WRITE_FPDMA_QUEUED)

/* ATA constants. */
#define ATA_SECTOR_SIZE		512		/* default sector size */
#define ATA_MAX_SECTORS		0x10000		/* max sectors per transfer */

#define ATA_ID_SIZE	(256 * sizeof(u16_t))	/* IDENTIFY result size */

#define ATA_ID_GCAP		0		/* General capabililties */
#define ATA_ID_GCAP_ATAPI_MASK	0xC000		/* ATAPI device mask */
#define ATA_ID_GCAP_ATAPI	0x8000		/* ATAPI device */
#define ATA_ID_GCAP_ATA_MASK	0x8000		/* ATA device mask */
#define ATA_ID_GCAP_ATA		0x0000		/* ATA device */
#define ATA_ID_GCAP_TYPE_MASK	0x1F00		/* ATAPI device type */
#define ATA_ID_GCAP_TYPE_SHIFT	8		/* (one of ATAPI_TYPE_*) */
#define ATA_ID_GCAP_REMOVABLE	0x0080		/* Removable media device */
#define ATA_ID_GCAP_INCOMPLETE	0x0004		/* Incomplete response */
#define ATA_ID_CAP		49		/* Capabilities */
#define ATA_ID_CAP_DMA		0x0100		/* DMA supported (no DMADIR) */
#define ATA_ID_CAP_LBA		0x0200		/* LBA supported */
#define ATA_ID_DMADIR		62		/* DMADIR */
#define ATA_ID_DMADIR_DMADIR	0x8000		/* DMADIR required */
#define ATA_ID_DMADIR_DMA	0x0400		/* DMA supported (DMADIR) */
#define ATA_ID_QDEPTH		75		/* NCQ queue depth */
#define ATA_ID_QDEPTH_MASK	0x000F		/* NCQ queue depth mask */
#define ATA_ID_SATA_CAP		76		/* SATA capabilities */
#define ATA_ID_SATA_CAP_NCQ	0x0100		/* NCQ support */
#define ATA_ID_SUP0		82		/* Features supported (1/3) */
#define ATA_ID_SUP0_WCACHE	0x0020		/* Write cache supported */
#define ATA_ID_SUP1		83		/* Features supported (2/3) */
#define ATA_ID_SUP1_VALID_MASK	0xC000		/* Word validity mask */
#define ATA_ID_SUP1_VALID	0x4000		/* Word contents are valid */
#define ATA_ID_SUP1_FLUSH	0x1000		/* FLUSH CACHE supported */
#define ATA_ID_SUP1_LBA48	0x0400		/* 48-bit LBA supported */
#define ATA_ID_ENA0		85		/* Features enabled (1/3) */
#define ATA_ID_ENA0_WCACHE	0x0020		/* Write cache enabled */
#define ATA_ID_ENA2		87		/* Features enabled (3/3) */
#define ATA_ID_ENA2_VALID_MASK	0xC000		/* Word validity mask */
#define ATA_ID_ENA2_VALID	0x4000		/* Word contents are valid */
#define ATA_ID_ENA2_FUA		0x0040		/* Forced Unit Access sup. */
#define ATA_ID_LBA0		100		/* Max. LBA48 address (LSW) */
#define ATA_ID_LBA1		101		/* Max. LBA48 address */
#define ATA_ID_LBA2		102		/* Max. LBA48 address */
#define ATA_ID_LBA3		103		/* Max. LBA48 address (MSW) */
#define ATA_ID_PLSS		106		/* Phys./logical sector size */
#define ATA_ID_PLSS_VALID_MASK	0xC000		/* Word validity mask */
#define ATA_ID_PLSS_VALID	0x4000		/* Word contents are valid */
#define ATA_ID_PLSS_LLS		0x1000		/* Long logical sectors */
#define ATA_ID_LSS0		118		/* Logical sector size (LSW) */
#define ATA_ID_LSS1		119		/* Logical sector size (MSW) */

#define ATA_SF_EN_WCACHE	0x02		/* Enable write cache */
#define ATA_SF_DI_WCACHE	0x82		/* Disable write cache */

/* ATAPI constants. */
#define ATAPI_PACKET_SIZE	16		/* ATAPI packet size */

#define ATAPI_TYPE_CDROM	5		/* CD-ROM device */

#define ATAPI_CMD_TEST_UNIT	0x00		/* Test Unit Ready */
#define ATAPI_CMD_REQUEST_SENSE	0x03		/* Request Sense */
#define ATAPI_REQUEST_SENSE_LEN	18		/* result length */
#define ATAPI_SENSE_UNIT_ATT	6		/* Unit Attention */
#define ATAPI_CMD_START_STOP	0x1B		/* Start/Stop Unit */
#define ATAPI_START_STOP_EJECT	0x02		/* eject the medium */
#define ATAPI_START_STOP_LOAD	0x03		/* load the medium */
#define ATAPI_CMD_READ_CAPACITY	0x25		/* Read Capacity */
#define ATAPI_READ_CAPACITY_LEN	8		/* result length */
#define ATAPI_CMD_READ		0xA8		/* Read (12) */
#define ATAPI_CMD_WRITE		0xAA		/* Write (12) */

/* Command List constants. */
#define AHCI_CL_ENTRY_SIZE	32		/* Command List header size */
#define AHCI_CL_ENTRY_DWORDS	(AHCI_CL_ENTRY_SIZE / sizeof(uint32_t))

#define AHCI_CL_PRDTL_SHIFT	16		/* PRD Table Length */
#define AHCI_CL_PREFETCHABLE	(1L << 7)	/* Prefetchable */
#define AHCI_CL_WRITE		(1L << 6)	/* Write */
#define AHCI_CL_ATAPI		(1L << 5)	/* ATAPI */
#define AHCI_CL_CFL_SHIFT	0		/* Command FIS Length */

/* Command Table offsets. */
#define AHCI_CT_PACKET_OFF		0x40	/* CT offset to ATAPI packet */
#define AHCI_CT_PRDT_OFF		0x80	/* CT offset to PRD table */


#define AHCI_PRDT_DBA 0x0
#define AHCI_PRDT_DBAU 0x4
#define AHCI_PRDT_DBC 0xC

/* Host Bus Adapter (HBA) constants. */
#define AHCI_HBA_CAP	0		/* Host Capabilities */
#define AHCI_HBA_CAP_SNCQ	(1L << 30)	/* Native Cmd Queuing */
#define AHCI_HBA_CAP_SCLO	(1L << 24)	/* Cmd List Override */
#define AHCI_HBA_CAP_NCS_SHIFT	8		/* Nr of Cmd Slots */
#define AHCI_HBA_CAP_NCS_MASK	0x1FL
#define AHCI_HBA_CAP_NP_SHIFT	0		/* Nr of Ports */
#define AHCI_HBA_CAP_NP_MASK	0x1FL
#define AHCI_HBA_GHC	1		/* Global Host Control */
#define AHCI_HBA_GHC_AE		(1L << 31)	/* AHCI Enable */
#define AHCI_HBA_GHC_IE		(1L <<  1)	/* Interrupt Enable */
#define AHCI_HBA_GHC_HR		(1L <<  0)	/* HBA Reset */
#define AHCI_HBA_IS	2		/* Interrupt Status */
#define AHCI_HBA_PI	3		/* Ports Implemented */
#define AHCI_HBA_VS	4		/* Version */
#define AHCI_HBA_CAP2	9		/* Host Capabilities Extended */

/* Port constants. */
#define AHCI_PORT_CLB	0		/* Command List Base */
#define AHCI_PORT_CLBU	1		/* Command List Base, Upper 32 bits */
#define AHCI_PORT_FB	2		/* FIS Base */
#define AHCI_PORT_FBU	3		/* FIS Base, Upper 32 bits */
#define AHCI_PORT_IS	4		/* Interrupt Status */
#define AHCI_PORT_IS_TFES	(1L << 30)	/* Task File Error */
#define AHCI_PORT_IS_HBFS	(1L << 29)	/* Host Bus Fatal */
#define AHCI_PORT_IS_HBDS	(1L << 28)	/* Host Bus Data */
#define AHCI_PORT_IS_IFS	(1L << 27)	/* Interface Fatal */
#define AHCI_PORT_IS_PRCS	(1L << 22)	/* PhyRdy Change */
#define AHCI_PORT_IS_PCS	(1L <<  6)	/* Port Conn Change */
#define AHCI_PORT_IS_SDBS	(1L <<  3)	/* Set Device Bits FIS */
#define AHCI_PORT_IS_PSS	(1L <<  1)	/* PIO Setup FIS */
#define AHCI_PORT_IS_DHRS	(1L <<  0)	/* D2H Register FIS */
#define AHCI_PORT_IS_RESTART                                    \
	(AHCI_PORT_IS_TFES | AHCI_PORT_IS_HBFS | AHCI_PORT_IS_HBDS |  \
	 AHCI_PORT_IS_IFS)
#define AHCI_PORT_IS_MASK                                         \
	(AHCI_PORT_IS_RESTART | AHCI_PORT_IS_PRCS | AHCI_PORT_IS_PCS |  \
	 AHCI_PORT_IS_DHRS | AHCI_PORT_IS_PSS | AHCI_PORT_IS_SDBS)
#define AHCI_PORT_IE	5		/* Interrupt Enable */
#define AHCI_PORT_IE_MASK	AHCI_PORT_IS_MASK
#define AHCI_PORT_IE_PRCE	AHCI_PORT_IS_PRCS
#define AHCI_PORT_IE_PCE	AHCI_PORT_IS_PCS
#define AHCI_PORT_IE_NONE	0L
#define AHCI_PORT_CMD	6		/* Command and Status */

#define AHCI_PORT_CMD_ASP	(1L << 27)	
#define AHCI_PORT_CMD_ALPE	(1L << 26)	
#define AHCI_PORT_CMD_PMA	(1L << 17)	/* Cmd List Running */
#define AHCI_PORT_CMD_CR	(1L << 15)	/* Cmd List Running */
#define AHCI_PORT_CMD_FR	(1L << 14)	/* FIS Recv Running */
#define AHCI_PORT_CMD_FRE	(1L <<  4)	/* FIS Recv Enabled */
#define AHCI_PORT_CMD_POD	(1L << 2)
#define AHCI_PORT_CMD_SUD	(1L <<  1)	/* Spin-Up Device */
#define AHCI_PORT_CMD_ST	(1L <<  0)	/* Start */
#define AHCI_PORT_TFD	8		/* Task File Data */
#define AHCI_PORT_TFD_STS_BSY	(1L << 7)	/* Busy */
#define AHCI_PORT_TFD_STS_DF	(1L << 5)	/* Device Fault */
#define AHCI_PORT_TFD_STS_DRQ	(1L << 3)	/* Data Xfer Req'd */
#define AHCI_PORT_TFD_STS_ERR	(1L << 0)	/* Error */
#define AHCI_PORT_TFD_STS_INIT	0x7F		/* Initial state */
#define AHCI_PORT_SIG	9		/* Signature */
#define ATA_SIG_ATA		0x00000101L	/* ATA interface */
#define ATA_SIG_ATAPI		0xEB140101L	/* ATAPI interface */
#define AHCI_PORT_SSTS	10		/* Serial ATA Status */
#define AHCI_PORT_SSTS_DET_MASK	0x00000007L	/* Detection Mask */
#define AHCI_PORT_SSTS_DET_PHY	0x00000003L	/* PHY Comm Establ */
#define AHCI_PORT_SCTL	11		/* Serial ATA Control */
#define AHCI_PORT_SCTL_DET_INIT	0x00000001L	/* Perform Init Seq */
#define AHCI_PORT_SCTL_DET_NONE	0x00000000L	/* No Action Req'd */
#define AHCI_PORT_SERR	12		/* Serial ATA Error */
#define AHCI_PORT_SERR_DIAG_N	(1L << 16)	/* PhyRdy Change */
#define AHCI_PORT_SACT	13		/* Serial ATA Active */
#define AHCI_PORT_CI	14		/* Command Issue */

/* Number of Physical Region Descriptors (PRDs). Must be at least NR_IOREQS+2,
 * and at most 1024. There is currently no reason to use more than the minimum.
 */
#define NR_PRDS		(NR_IOREQS + 2)

/* Various size constants. */
#define AHCI_MEM_BASE_SIZE	0x100	/* memory-mapped base region size */
#define AHCI_MEM_PORT_SIZE	0x80	/* memory-mapped port region size */

#define AHCI_FIS_SIZE	256		/* size of FIS receive buffer */
#define AHCI_CL_SIZE	1024		/* size of command list buffer */
#define AHCI_TMP_SIZE	ATA_ID_SIZE	/* size of temporary storage buffer */
#define AHCI_TMP_ALIGN	2		/* required alignment for temp buf */
#define AHCI_CT_SIZE	(128 + NR_PRDS * sizeof(uint32_t) * 4)
/* size of command table buffer */
#define AHCI_CT_ALIGN	128		/* required alignment for CT buffer */

#define MAX_PRD_BYTES	(1L << 22)	/* maximum number of bytes per PRD */
#define MAX_TRANSFER	MAX_PRD_BYTES	/* maximum size of a single transfer */

//  /* Command Frame Information Structure (FIS). For internal use only;
//  * the contents of this structure are later converted to an actual FIS.
//  */
// typedef struct {
// 	uint8_t cf_cmd;		/* Command */
// 	uint8_t cf_feat;		/* Features */
// 	uint32_t cf_lba;		/* LBA (24-bit) */
// 	uint8_t cf_dev;		/* Device */
// 	uint32_t cf_lba_exp;	/* LBA (exp) (24-bit) */
// 	uint8_t cf_feat_exp;	/* Features (exp) */
// 	uint8_t cf_sec;		/* Sector Count */
// 	uint8_t cf_sec_exp;	/* Sector Count (exp) */
// 	uint8_t cf_ctl;		/* Control */
// } cmd_fis_t;

/* Physical Region Descriptor (PRD). For internal and sys_vumap() use only;
 * the contents of this structure are later converted to an actual PRD.
 */
typedef struct vumap_phys prd_t;

/* These are from at_wini, as this driver is a drop-in replacement for at_wini.
 * Practically speaking this is already the upper limit with 256 minor device
 * numbers per driver, even though it means we can only ever expose 8 devices
 * out of potentially 32.
 */
#define MAX_DRIVES		8
#define NR_MINORS		(MAX_DRIVES * DEV_PER_DRIVE)
#define SUB_PER_DRIVE		(NR_PARTITIONS * NR_PARTITIONS)
#define NR_SUBDEVS		(MAX_DRIVES * SUB_PER_DRIVE)

/* Port states. */
enum {
	STATE_NO_PORT,		/* this port is not present */
	STATE_SPIN_UP,		/* waiting for device or timeout after reset */
	STATE_NO_DEV,		/* no device has been detected on this port */
	STATE_WAIT_SIG,		/* waiting for device signature to appear */
	STATE_WAIT_ID,		/* waiting for device identification */
	STATE_BAD_DEV,		/* an unusable device has been detected */
	STATE_GOOD_DEV		/* a usable device has been detected */
};

/* Command results. */
enum {
	RESULT_FAILURE,
	RESULT_SUCCESS
};

/* Port flags. */
#define FLAG_ATAPI		0x00000001	/* is this an ATAPI device? */
#define FLAG_HAS_MEDIUM		0x00000002	/* is a medium present? */
#define FLAG_USE_DMADIR		0x00000004	/* use ATAPI DMADIR flag? */
#define FLAG_READONLY		0x00000008	/* is the device read-only? */
#define FLAG_BUSY		0x00000010	/* is an operation ongoing? */
#define FLAG_FAILURE		0x00000020	/* did the operation fail? */
#define FLAG_BARRIER		0x00000040	/* no access until unset */
#define FLAG_HAS_WCACHE		0x00000080	/* is a write cache present? */
#define FLAG_HAS_FLUSH		0x00000100	/* is FLUSH CACHE supported? */
#define FLAG_SUSPENDED		0x00000200	/* is the thread suspended? */
#define FLAG_HAS_FUA		0x00000400	/* is WRITE DMA FUA EX sup.? */
#define FLAG_HAS_NCQ		0x00000800	/* is NCQ supported? */
#define FLAG_NCQ_MODE		0x00001000	/* issuing NCQ commands? */

/* Mapping between devices and ports. */
#define NO_PORT		-1	/* this device maps to no port */
#define NO_DEVICE	-1	/* this port maps to no device */

/* Output verbosity levels. */
enum {
	V_NONE,		/* no output at all; keep silent */
	V_ERR,		/* important error information only (the default) */
	V_INFO,		/* general information about the driver and devices */
	V_DEV,		/* device details, to help with detection problems */
	V_REQ		/* detailed information about requests */
};


typedef struct _IDENTIFY_DEVICE_DATA {
  struct {
    uint16_t Reserved1  :1;
    uint16_t Retired3  :1;
    uint16_t ResponseIncomplete  :1;
    uint16_t Retired2  :3;
    uint16_t FixedDevice  :1;
    uint16_t RemovableMedia  :1;
    uint16_t Retired1  :7;
    uint16_t DeviceType  :1;
  } GeneralConfiguration;
  uint16_t NumCylinders;
  uint16_t ReservedWord2;
  uint16_t NumHeads;
  uint16_t Retired1[2];
  uint16_t NumSectorsPerTrack;
  uint16_t VendorUnique1[3];
  uint8_t  SerialNumber[20];
  uint16_t Retired2[2];
  uint16_t Obsolete1;
  uint8_t  FirmwareRevision[8];
  uint8_t  ModelNumber[40];
  uint8_t  MaximumBlockTransfer;
  uint8_t  VendorUnique2;
  uint16_t ReservedWord48;
  struct {
    uint8_t  ReservedByte49;
    uint8_t  DmaSupported  :1;
    uint8_t  LbaSupported  :1;
    uint8_t  IordyDisable  :1;
    uint8_t  IordySupported  :1;
    uint8_t  Reserved1  :1;
    uint8_t  StandybyTimerSupport  :1;
    uint8_t  Reserved2  :2;
    uint16_t ReservedWord50;
  } Capabilities;
  uint16_t ObsoleteWords51[2];
  uint16_t TranslationFieldsValid  :3;
  uint16_t Reserved3  :13;
  uint16_t NumberOfCurrentCylinders;
  uint16_t NumberOfCurrentHeads;
  uint16_t CurrentSectorsPerTrack;
  uint32_t  CurrentSectorCapacity;
  uint8_t  CurrentMultiSectorSetting;
  uint8_t  MultiSectorSettingValid  :1;
  uint8_t  ReservedByte59  :7;
  uint32_t  UserAddressableSectors;
  uint16_t ObsoleteWord62;
  uint16_t MultiWordDMASupport  :8;
  uint16_t MultiWordDMAActive  :8;
  uint16_t AdvancedPIOModes  :8;
  uint16_t ReservedByte64  :8;
  uint16_t MinimumMWXferCycleTime;
  uint16_t RecommendedMWXferCycleTime;
  uint16_t MinimumPIOCycleTime;
  uint16_t MinimumPIOCycleTimeIORDY;
  uint16_t ReservedWords69[6];
  uint16_t QueueDepth  :5;
  uint16_t ReservedWord75  :11;
  uint16_t ReservedWords76[4];
  uint16_t MajorRevision;
  uint16_t MinorRevision;
  struct {
    uint16_t SmartCommands  :1;
    uint16_t SecurityMode  :1;
    uint16_t RemovableMediaFeature  :1;
    uint16_t PowerManagement  :1;
    uint16_t Reserved1  :1;
    uint16_t WriteCache  :1;
    uint16_t LookAhead  :1;
    uint16_t ReleaseInterrupt  :1;
    uint16_t ServiceInterrupt  :1;
    uint16_t DeviceReset  :1;
    uint16_t HostProtectedArea  :1;
    uint16_t Obsolete1  :1;
    uint16_t WriteBuffer  :1;
    uint16_t ReadBuffer  :1;
    uint16_t Nop  :1;
    uint16_t Obsolete2  :1;
    uint16_t DownloadMicrocode  :1;
    uint16_t DmaQueued  :1;
    uint16_t Cfa  :1;
    uint16_t AdvancedPm  :1;
    uint16_t Msn  :1;
    uint16_t PowerUpInStandby  :1;
    uint16_t ManualPowerUp  :1;
    uint16_t Reserved2  :1;
    uint16_t SetMax  :1;
    uint16_t Acoustics  :1;
    uint16_t BigLba  :1;
    uint16_t DeviceConfigOverlay  :1;
    uint16_t FlushCache  :1;
    uint16_t FlushCacheExt  :1;
    uint16_t Resrved3  :2;
    uint16_t SmartErrorLog  :1;
    uint16_t SmartSelfTest  :1;
    uint16_t MediaSerialNumber  :1;
    uint16_t MediaCardPassThrough  :1;
    uint16_t StreamingFeature  :1;
    uint16_t GpLogging  :1;
    uint16_t WriteFua  :1;
    uint16_t WriteQueuedFua  :1;
    uint16_t WWN64Bit  :1;
    uint16_t URGReadStream  :1;
    uint16_t URGWriteStream  :1;
    uint16_t ReservedForTechReport  :2;
    uint16_t IdleWithUnloadFeature  :1;
    uint16_t Reserved4  :2;
  } CommandSetSupport;
  struct {
    uint16_t SmartCommands  :1;
    uint16_t SecurityMode  :1;
    uint16_t RemovableMediaFeature  :1;
    uint16_t PowerManagement  :1;
    uint16_t Reserved1  :1;
    uint16_t WriteCache  :1;
    uint16_t LookAhead  :1;
    uint16_t ReleaseInterrupt  :1;
    uint16_t ServiceInterrupt  :1;
    uint16_t DeviceReset  :1;
    uint16_t HostProtectedArea  :1;
    uint16_t Obsolete1  :1;
    uint16_t WriteBuffer  :1;
    uint16_t ReadBuffer  :1;
    uint16_t Nop  :1;
    uint16_t Obsolete2  :1;
    uint16_t DownloadMicrocode  :1;
    uint16_t DmaQueued  :1;
    uint16_t Cfa  :1;
    uint16_t AdvancedPm  :1;
    uint16_t Msn  :1;
    uint16_t PowerUpInStandby  :1;
    uint16_t ManualPowerUp  :1;
    uint16_t Reserved2  :1;
    uint16_t SetMax  :1;
    uint16_t Acoustics  :1;
    uint16_t BigLba  :1;
    uint16_t DeviceConfigOverlay  :1;
    uint16_t FlushCache  :1;
    uint16_t FlushCacheExt  :1;
    uint16_t Resrved3  :2;
    uint16_t SmartErrorLog  :1;
    uint16_t SmartSelfTest  :1;
    uint16_t MediaSerialNumber  :1;
    uint16_t MediaCardPassThrough  :1;
    uint16_t StreamingFeature  :1;
    uint16_t GpLogging  :1;
    uint16_t WriteFua  :1;
    uint16_t WriteQueuedFua  :1;
    uint16_t WWN64Bit  :1;
    uint16_t URGReadStream  :1;
    uint16_t URGWriteStream  :1;
    uint16_t ReservedForTechReport  :2;
    uint16_t IdleWithUnloadFeature  :1;
    uint16_t Reserved4  :2;
  } CommandSetActive;

  uint16_t UltraDMASupport  :8;
  uint16_t UltraDMAActive  :8;
  uint16_t ReservedWord89[4];
  uint16_t HardwareResetResult;
  uint16_t CurrentAcousticValue  :8;
  uint16_t RecommendedAcousticValue  :8;
  uint16_t ReservedWord95[5];
  uint32_t Max48BitLBA[2];
  uint16_t StreamingTransferTime;
  uint16_t ReservedWord105;

  struct {
    uint16_t LogicalSectorsPerPhysicalSector  :4;
    uint16_t Reserved0  :8;
    uint16_t LogicalSectorLongerThan256Words  :1;
    uint16_t MultipleLogicalSectorsPerPhysicalSector  :1;
    uint16_t Reserved1  :2;
  } PhysicalLogicalSectorSize;

  uint16_t InterSeekDelay;
  uint16_t WorldWideName[4];
  uint16_t ReservedForWorldWideName128[4];
  uint16_t ReservedForTlcTechnicalReport;
  uint16_t WordsPerLogicalSector[2];

  struct {
    uint16_t ReservedForDrqTechnicalReport  :1;
    uint16_t WriteReadVerifySupported  :1;
    uint16_t Reserved01  :11;
    uint16_t Reserved1  :2;
  } CommandSetSupportExt;

  struct {
    uint16_t ReservedForDrqTechnicalReport  :1;
    uint16_t WriteReadVerifyEnabled  :1;
    uint16_t Reserved01  :11;
    uint16_t Reserved1  :2;
  } CommandSetActiveExt;

  uint16_t ReservedForExpandedSupportandActive[6];
  uint16_t MsnSupport  :2;
  uint16_t ReservedWord1274  :14;

  struct {
    uint16_t SecuritySupported  :1;
    uint16_t SecurityEnabled  :1;
    uint16_t SecurityLocked  :1;
    uint16_t SecurityFrozen  :1;
    uint16_t SecurityCountExpired  :1;
    uint16_t EnhancedSecurityEraseSupported  :1;
    uint16_t Reserved0  :2;
    uint16_t SecurityLevel  :1;
    uint16_t Reserved1  :7;
  } SecurityStatus;

  uint16_t ReservedWord129[31];
  struct {
    uint16_t MaximumCurrentInMA2  :12;
    uint16_t CfaPowerMode1Disabled  :1;
    uint16_t CfaPowerMode1Required  :1;
    uint16_t Reserved0  :1;
    uint16_t Word160Supported  :1;
  } CfaPowerModel;

  uint16_t ReservedForCfaWord161[8];

  struct {
    uint16_t SupportsTrim  :1;
    uint16_t Reserved0  :15;
  } DataSetManagementFeature;

  uint16_t ReservedForCfaWord170[6];
  uint16_t CurrentMediaSerialNumber[30];
  uint16_t ReservedWord206;
  uint16_t ReservedWord207[2];

  struct {
    uint16_t AlignmentOfLogicalWithinPhysical  :14;
    uint16_t Word209Supported  :1;
    uint16_t Reserved0  :1;
  } BlockAlignment;

  uint16_t WriteReadVerifySectorCountMode3Only[2];
  uint16_t WriteReadVerifySectorCountMode2Only[2];

  struct {
    uint16_t NVCachePowerModeEnabled  :1;
    uint16_t Reserved0  :3;
    uint16_t NVCacheFeatureSetEnabled  :1;
    uint16_t Reserved1  :3;
    uint16_t NVCachePowerModeVersion  :4;
    uint16_t NVCacheFeatureSetVersion  :4;
  } NVCacheCapabilities;

  uint16_t NVCacheSizeLSW;
  uint16_t NVCacheSizeMSW;
  uint16_t NominalMediaRotationRate;
  uint16_t ReservedWord218;

  struct {
    uint8_t NVCacheEstimatedTimeToSpinUpInSeconds;
    uint8_t Reserved;
  } NVCacheOptions;

  uint16_t ReservedWord220[35];
  uint16_t Signature  :8;
  uint16_t CheckSum  :8;
} 
IDENTIFY_DEVICE_DATA;

#endif /* _AHCI_H */
