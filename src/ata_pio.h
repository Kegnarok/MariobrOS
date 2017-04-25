#ifndef ATA_PIO_H
#define ATA_PIO_H

#include "error.h"

/* The default and seemingly universal sector size for CD-ROMs. */
#define ATAPI_SECTOR_SIZE 2048
 
/* /\* The default ISA IRQ numbers of the ATA controllers. *\/ */
/* #define ATA_IRQ_PRIMARY     0x0E */
/* #define ATA_IRQ_SECONDARY   0x0F */
 
/* The necessary I/O ports, indexed by "bus". */
#define ATA_DATA(x)         (x)
#define ATA_FEATURES(x)     (x+1)
#define ATA_SECTOR_COUNT(x) (x+2)
#define ATA_ADDRESS1(x)     (x+3) /* LBA low  */
#define ATA_ADDRESS2(x)     (x+4) /* LBA mid  */
#define ATA_ADDRESS3(x)     (x+5) /* LBA high */
#define ATA_DRIVE_SELECT(x) (x+6)
#define ATA_COMMAND(x)      (x+7)
#define ATA_DCR(x)          (x+0x206)   /* device control register */
 
/* valid values for "bus" */
#define ATA_BUS_PRIMARY     0x1F0
#define ATA_BUS_SECONDARY   0x170
/* valid values for "drive" */
#define ATA_DRIVE_MASTER    0xA0
#define ATA_DRIVE_SLAVE     0xB0
 
/* ATA specifies a 400ns delay after drive switching -- often
 * implemented as 4 Alternative Status queries. */
#define ATA_SELECT_DELAY(bus) \
  {inb(ATA_DCR(bus));inb(ATA_DCR(bus));inb(ATA_DCR(bus));inb(ATA_DCR(bus));}

/* Status Byte layout masks */
#define ATA_ERR 0x01 // Error (except DF)
#define ATA_DRQ 0x08 // PIO data transfer
#define ATA_SRV 0x10 // Overlapped Mode Service Request
#define ATA_DF  0x20 // Drive Fault Error
#define ATA_RDY 0x40 // Ready
#define ATA_BSY 0x80 // Busy (if set, disregard the rest)

void identify();

#endif
