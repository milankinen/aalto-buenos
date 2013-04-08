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
    real_dev->crecv = condition_create();
    real_dev->csend = condition_create();

    irq_mask = 1 << (desc->irq + 11);
    interrupt_register(irq_mask, nic_interrupt_handle, dev);

    return dev;
}

static void nic_interrupt_handle(device_t *device) {
    nic_real_device_t *real_dev = device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)device->io_address;

    spinlock_acquire(&real_dev->slock);

    /* handle interrupts */
    if (NIC_STATUS_RXIRQ(io->status)) {
        /* frame ready to be tranfered from receive buffer */
        /* clear RXBUSY bit */
    }
    else if (NIC_STATUS_RIRQ(io->status)) {
        /* transfer from receive buffer to memory buffer ready */
    }
    else if (NIC_STATUS_SIRQ(io->status)) {
        /* transfer from memory buffer to send buffer ready */
    }
    else {
        /* interrupt was not for us 
         * do nothing */
        spinlock_release(&real_dev->slock);
        return;
    }
    spinlock_release(&real_dev->slock);
    device = device;
}

int nic_busy(nic_io_area_t *io) {
    return (NIC_STATUS_RXBUSY(io->status) ||
            NIC_STATUS_RBUSY(io->status) ||
            NIC_STATUS_SBUSY(io->status));
}

static int nic_send(struct gnd_struct *gnd, void *frame, network_address_t addr) {
    nic_real_device_t *nic = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)gnd->device->io_address;
    /* acquire nic */
    lock_acquire(nic->lock);
    while (nic_busy(io))
        condition_wait(nic->csend, nic->lock);
    /* interrupt handler has now released this thead to run */
    /* set DMAADDR */
    /* set COMMAND */
    /* wait */
    lock_release(nic->lock);
    /* FIXME */
    gnd = gnd;
    frame = frame;
    addr = addr;
    return 0;
}

static int nic_recv(struct gnd_struct *gnd, void *frame) {
    nic_real_device_t *nic = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t *)gnd->device->io_address;
    /* acquire nic */
    lock_acquire(nic->lock);
    condition_wait(nic->crecv, nic->lock);
    /* FIXME: when set dma and RXIRQ vs RIRQ */
    io = io;
    gnd = gnd;
    frame = frame;
    return 0;
}

static uint32_t nic_frame_size(struct gnd_struct *gnd) {
    /* TODO */
    gnd = gnd;
    return 0;
}

static network_address_t nic_hwaddr(struct gnd_struct *gnd) {
    /* TODO */
    gnd = gnd;
    return 0;
}
