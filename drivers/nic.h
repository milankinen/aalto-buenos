/*
 * Network interface card (NIC) driver
 */

#ifndef DRIVERS_NIC_H
#define DRIVERS_NIC_H

#include "kernel/lock_cond.h"
#include "kernel/spinlock.h"
#include "kernel/semaphore.h"
#include "drivers/device.h"
#include "drivers/yams.h"

/* Receive and send will start a DMA tranfer
 * between receive/send buffers and memory buffer
 * addressed by DMAADDR. Bit commands will clear the
 * bit.
 */
#define NIC_COMMAND_RECEIVE         0x1
#define NIC_COMMAND_SEND            0x2 
#define NIC_COMMAND_RXIRQ           0x3
#define NIC_COMMAND_RIRQ            0x4
#define NIC_COMMAND_SIRQ            0x5  
#define NIC_COMMAND_RXBUSY          0x6
#define NIC_COMMAND_ENTERPRO        0x7
#define NIC_COMMAND_EXITPRO         0x8

#define NIC_STATUS_RXBUSY(status)   ((status) & 0x00000001)
#define NIC_STATUS_RBUSY(status)    ((status) & 0x00000002)
#define NIC_STATUS_SBUSY(status)    ((status) & 0x00000004)
#define NIC_STATUS_RXIRQ(status)    ((status) & 0x00000008)
#define NIC_STATUS_RIRQ(status)     ((status) & 0x00000010)
#define NIC_STATUS_SIRQ(status)     ((status) & 0x00000020) 
#define NIC_STATUS_PROMISC(status)  ((status) & 0x00000040)

#define NIC_STATUS_NOFRAME(status)  ((status) & 0x08000000) 
#define NIC_STATUS_IADDR(status)    ((status) & 0x10000000) 
#define NIC_STATUS_ICOMM(status)    ((status) & 0x20000000) 
#define NIC_STATUS_EBUSY(status)    ((status) & 0x40000000) 
#define NIC_STATUS_ERROR(status)    ((status) & 0x80000000) 

/* mask for the all above errors */
#define NIC_STATUS_ERRORS(status)   ((status) & 0xf8000000)

typedef struct {
    volatile uint32_t status;
    volatile uint32_t command;
    volatile uint32_t hwaddr;
    volatile uint32_t mtu;
    volatile uint32_t dmaaddr;
} nic_io_area_t;

/* spinlock for synchronization in interrupt handler
 * lock for synchronization in send/receive
 * ccndition variables for waiting in send/receive
 */
typedef struct {
    spinlock_t slock;
    lock_t *lock;
    cond_t *crecv;
    cond_t *csend;
} nic_real_device_t;

device_t *nic_init(io_descriptor_t *desc);

#endif /* DRIVERS_NIC_H */
