/*
 * network.c
 */

#include "net/socket.h"
#include "net/pop.h"
#include "kernel/assert.h"

#define PORT1 9191
#define PORT2 9192

/*same as in yams.conf but converted to dec*/
#define NADDR 251728436

/*same as in yams2.conf but converted to dec*/
#define NADDR2 251728437

#define LOOPSIZE 10
/*reads hwaddr and mtu of the nic device
 *specified in yams.conf
 *meant to run as a unit test on a single buenos/yams
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

void run_nic_tests() {
    test_nicIOArea();
}

/*write bytes to socket
 * meant to interract with other buenos instances
 */
void network_test() {
    uint16_t port1 = PORT1;
    uint16_t port2 = PORT2;

    int i;
    sock_t s = socket_open(PROTOCOL_POP, port1);
    char msg[] = "aaa";
    for (i = 0; i < LOOPSIZE; i++) {
        kprintf("Socket created to port: %d\n", port1);
        int n = socket_sendto(s, NETWORK_BROADCAST_ADDRESS, port2, msg,
                10);
        msg[0]++;
        kprintf("wrote %d chars to socket\n", n);
        thread_sleep(1000);
    }
}

/*read bytes from socket
 *meant to interract with other buenos instances
 */
void network_test2() {

    char buf[100];
    char expectedmsg[] = "aaa";
    int readed;
    uint16_t port1 = PORT1;
    uint16_t port2 = PORT2;
    network_address_t naddr = NADDR;

    sock_t s = socket_open(PROTOCOL_POP, port2);
    kprintf("Socket created to port: %d\n", port2);

    kprintf("Waiting to receive packets\n");
    int i;
    for (i = 0; i < LOOPSIZE; i++) {

        socket_recvfrom(s, &naddr, &port1, buf, 10, &readed);
        kprintf("Packet received: %s\n", buf);
        KERNEL_ASSERT(buf[0] == expectedmsg[0]);
        KERNEL_ASSERT(buf[1] == expectedmsg[1]);
        KERNEL_ASSERT(buf[2] == expectedmsg[2]);

        expectedmsg[0]++;
    }

}

