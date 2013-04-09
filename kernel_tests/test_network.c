/*
 * network.c
 */

#include "net/socket.h"
#include "net/pop.h"
#include "kernel/assert.h"

#define PORT1 9191
#define PORT2 9192
#define NADDR 123

/*reads hwaddr and mtu of the nic device
 *specified in yams.conf
 *meant to run as a unit test on a single buenos
 */
static void test_nicIOArea() {
    kprintf("nicIOArea test\n");
    device_t *dev = device_get(YAMS_TYPECODE_NIC, 0);
    gnd_t *gnd = dev->generic_device;
    int mac = gnd->hwaddr(gnd);
    int mtu = gnd->frame_size(gnd);
    KERNEL_ASSERT(mac == 251728436);
    KERNEL_ASSERT(mtu == 1324);

}

void run_nic_tests(){
    test_nicIOArea();
}

/*write bytes to socket
 * meant to interract with other buenos instances
 */
void network_test() {
    uint16_t port1 = PORT1;
    uint16_t port2 = PORT2;

    char *buf = "TEXTEXTEXT";
    sock_t s = socket_open(PROTOCOL_POP, port1);
    int n = socket_sendto(s, (uint16_t) NADDR, port2, buf, 4);
    kprintf("wrote %d chars to socket\n", n);
}

/*read bytes from socket
 *meant to interract with other buenos instances
 */
void network_test2() {

    char buf[100];
    int readed;
    uint16_t port1 = PORT1;
    uint16_t port2 = PORT2;
    network_address_t naddr = NADDR;

    kprintf("Opening socket to port: %d\n", port2);
    sock_t s = socket_open(PROTOCOL_POP, port2);
    kprintf("Waiting to receive packets\n");
    socket_recvfrom(s, &naddr, &port1, buf, 100, &readed);
    kprintf("Packet received: %s", buf);

}

