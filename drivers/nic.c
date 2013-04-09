/*
 * NIC driver
 */

#include "drivers/device.h"
#include "drivers/gnd.h"
#include "drivers/nic.h"
#include "drivers/yams.h"
#include "kernel/kmalloc.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/lock_cond.h"
#include "kernel/spinlock.h"
#include "kernel/interrupt.h"
#include "lib/libc.h"

/* functions of this file */

static void nic_interrupt_handle(device_t *device);
static int nic_send(struct gnd_struct *gnd, void *frame, network_address_t addr);
static int nic_recv(struct gnd_struct *gnd, void *frame);
static uint32_t nic_frame_size(struct gnd_struct *gnd);
static network_address_t nic_hwaddr(struct gnd_struct *gnd);

/* initialization */
device_t *nic_init(io_descriptor_t *desc) 
{
    device_t *dev;
    gnd_t    *gnd;
    nic_real_device_t *real_dev;
    uint32_t irq_mask;

    dev = kmalloc(sizeof(device_t));
    gnd = kmalloc(sizeof(gnd_t));
    real_dev = kmalloc(sizeof(nic_real_device_t));
    if (dev == NULL || gnd == NULL || real_dev == NULL) 
	KERNEL_PANIC("Could not allocate memory for nic driver.");
    
    dev->generic_device = gnd;
    dev->real_device = real_dev;
    dev->descriptor = desc;
    dev->io_address = desc->io_area_base;
    dev->type = desc->type;

    gnd->device = dev;
    gnd->send = nic_send;
    gnd->recv = nic_recv;
    gnd->frame_size = nic_frame_size;
    gnd->hwaddr = nic_hwaddr;

    spinlock_reset(&real_dev->slock);
    real_dev->lock = lock_create();
    real_dev->dmalock = lock_create();
    real_dev->crxirq = condition_create();
    real_dev->crirq = condition_create();
    real_dev->csirq = condition_create();
    real_dev->csdma = condition_create();

    irq_mask = 1 << (desc->irq + 11);
    interrupt_register(irq_mask, nic_interrupt_handle, dev);

    return dev;
}

int nic_dmabusy(nic_real_device_t *nic, nic_io_area_t *io) {
    spinlock_acquire(&nic->slock);
    int b = (NIC_STATUS_RBUSY(io->status) ||
             NIC_STATUS_SBUSY(io->status));
    spinlock_release(&nic->slock);
    return b;
}

int nic_dmabusy_without_slock(nic_io_area_t *io) {
    return (NIC_STATUS_RBUSY(io->status) ||
            NIC_STATUS_SBUSY(io->status));
}

static void nic_interrupt_handle(device_t *device) {
    nic_real_device_t *real_dev = device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)device->io_address;

    spinlock_acquire(&real_dev->slock);

    /* handle interrupts:
     *  No else if statements since we will handle possibly more than
     *  one interrupt. This is used for delaying RXIRQ handling when
     *  DMA transfer is used by send.
     */
    if (NIC_STATUS_RXIRQ(io->status)) {
        /* frame ready to be tranfered from receive buffer */
        /* if dma tranfer is in use, do nothing
         * the interrupt will be handled later
         */
        if (!nic_dmabusy_without_slock(io)) { /* slock is ours */
            /* transfer to memory ready:
             *     the command will be issued by nic_recv
             *
             * clear RXIRQ bit: this interrupt is now handled */
            io->command = NIC_COMMAND_RXIRQ;
            /* wake up waiting receivers */
            condition_signal(real_dev->crxirq, real_dev->lock);
            /* we must return not to let SIRQ to signal csdma */
        }
    }
    if (NIC_STATUS_RIRQ(io->status)) {
        /* transfer from receive buffer to memory buffer ready */
        /* clear RIRQ and RXBUSY bit */
        io->command = NIC_COMMAND_RIRQ;
        io->command = NIC_COMMAND_RXBUSY;
        /* wake up receivers and send dma waiters*/
        condition_signal(real_dev->crirq, real_dev->lock);
        condition_signal(real_dev->csdma, real_dev->lock);
    }
    if (NIC_STATUS_SIRQ(io->status)) {
        /* transfer from memory buffer to send buffer ready */
        /* clear SRIRQ bit */
        io->command = NIC_COMMAND_SIRQ;
        /* wake up senders and send dma waiters*/
        condition_signal(real_dev->csirq, real_dev->lock);
        condition_signal(real_dev->csdma, real_dev->lock);
    }
    else {
        /* interrupt was not for us 
         * do nothing */
        spinlock_release(&real_dev->slock);
        return;
    }
    spinlock_release(&real_dev->slock);
    return;
}

static int nic_send(struct gnd_struct *gnd, void *frame, network_address_t addr) {
    int retval = 0;
    nic_real_device_t *nic = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)gnd->device->io_address;

    /* acquire nic */
    lock_acquire(nic->lock);
    while (nic_dmabusy(nic,io))
        condition_wait(nic->csdma, nic->lock);

    /* interrupt handler has now released this thead to run */
    /* set up the frame:
     *  first word destination address
     *  secend word sender address
     */
    ((uint32_t *)frame)[0] = addr;
    ((uint32_t *)frame)[1] = nic_hwaddr(gnd);

    /* acquire dmalock */
    lock_acquire(nic->dmalock);
    /* synchronize memory mapped io with the spinlock */
    spinlock_acquire(&nic->slock);
    /* set DMAADDR */
    io->dmaaddr = (uint32_t)frame;
    /* set COMMAND */
    io->command = NIC_COMMAND_SEND;
    /* check errors */
    if (NIC_STATUS_IADDR(io->status))
        retval = NIC_ERROR_IADDR;
    else if (NIC_STATUS_ERRORS(io->status)) {
        kprintf("nic error: 0x%8.8x\n", DISK_STATUS_ERRORS(io->status));
        KERNEL_PANIC("nic error occured");
    }
    spinlock_release(&nic->slock);

    /* wait */
    condition_wait(nic->csirq, nic->lock);
    lock_release(nic->dmalock);
    lock_release(nic->lock);
    return retval;
}

static int nic_recv(struct gnd_struct *gnd, void *frame) {
    nic_real_device_t *nic = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)gnd->device->io_address;

    /* acquire nic */
    lock_acquire(nic->lock);
    while (nic_dmabusy(nic,io)) {
        condition_wait(nic->crxirq, nic->lock);
    }

    /* acquire dma_lock to prevent send overwriting
     * dmaaddr in the case of simultaneous RXIRQ and SIRQ
     */
    lock_acquire(nic->dmalock);

    /* acquire spinlock to alter io field */
    spinlock_acquire(&nic->slock);
    io->dmaaddr = (uint32_t)frame;
    io->command = NIC_COMMAND_RECEIVE;
    if (NIC_STATUS_ERRORS(io->status)) {
        kprintf("nic error: 0x%8.8x\n", DISK_STATUS_ERRORS(io->status));
        KERNEL_PANIC("nic error occured");
    }
    spinlock_release(&nic->slock);

    /* wait for the message to be tranfered to frame */
    condition_wait(nic->crirq, nic->lock);
    lock_release(nic->dmalock);
    lock_release(nic->lock);
    return 0;
}

static uint32_t nic_frame_size(struct gnd_struct *gnd) {
    nic_real_device_t *nic = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)gnd->device->io_address;
    
    /* io area is synchronized by the spinlock */
    spinlock_acquire(&nic->slock);
    uint32_t n = io->mtu;
    spinlock_release(&nic->slock);

    return n;
}

static network_address_t nic_hwaddr(struct gnd_struct *gnd) {
    nic_real_device_t *nic = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)gnd->device->io_address;

    /* io area is synchronized by the spinlock */
    spinlock_acquire(&nic->slock);
    network_address_t addr = io->hwaddr;
    spinlock_release(&nic->slock);

    return addr;
}
