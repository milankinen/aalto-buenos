/*
 * network.c
 */

#include "net/socket.h"
#include "net/pop.h"
#include "kernel/assert.h"
#include "test_network.h"

/*same as in yams.conf but converted to dec*/
#define NADDR 251728436

/*same as in yams2.conf but converted to dec*/
#define NADDR2 251728437

#define LOOPSIZE 10

uint16_t port1 = 9191;
uint16_t port2 = 9192;

/*acts as a counter, if >0 tests are still running and we don't terminate*/
int finished = 0;

/*reads hwaddr and mtu of the nic device
 *specified in yams.conf
 *meant to run as a unit test on a single buenos/yams
 */
static void test_nicIOArea() {
    device_t *dev = device_get(YAMS_TYPECODE_NIC, 0);
    gnd_t *gnd = dev->generic_device;
    int mac = gnd->hwaddr(gnd);
    int mtu = gnd->frame_size(gnd);
    KERNEL_ASSERT(mac == 251728436);
    KERNEL_ASSERT(mtu == 1324);

}

static void write_bytes_thread(uint32_t dummy) {
    dummy = dummy;
    write_to_network_test(port1, port2);
}

static void receive_bytes_thread(uint32_t dummy) {
    finished++;
    dummy = dummy;
    receive_from_network_test(port1, port2);
    finished--;

}

/*write bytes to socket
 * meant to interract with other buenos instances
 */
void write_to_network_test(uint16_t port1, uint16_t port2) {

    int i;
    sock_t s = socket_open(PROTOCOL_POP, port1);
    kprintf("Socket created to port: %d\n", port1);
    char msg[] = "aaa";
    for (i = 0; i < LOOPSIZE; i++) {
        int n = socket_sendto(s, NETWORK_BROADCAST_ADDRESS, port2, msg, 10);
        msg[0]++;
        kprintf("wrote max %d chars to socket\n", n);
        thread_sleep(1000);
    }
}

/*read bytes from socket
 *meant to interract with other buenos instances
 */
void receive_from_network_test(uint16_t port1, uint16_t port2) {

    char buf[100];
    char expectedmsg[] = "aaa";
    int readed;

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
    /*the test is now finished*/
}

void test_send_receive() {
    /*two threads inside the same buenos send and receive packets*/
    TID_t tid = thread_create(&receive_bytes_thread, 0);
    thread_run(tid);
    TID_t tid2 = thread_create(&write_bytes_thread, 0);
    thread_run(tid2);
}

static void test_multiple_send_receive() {
   port1 = 7071;
    port2 = 7072;
    test_send_receive();
 /*   TID_t tid1 = thread_create(&write_bytes_thread, 0);
    thread_run(tid1);
    */
    thread_sleep(2000);
    port1 = 7171;
    port2 = 7172;


    test_send_receive();


  //  test_send_receive();
  /*    TID_t tid = thread_create(&receive_bytes_thread, 0);
    thread_run(tid);
    TID_t tid2 = thread_create(&write_bytes_thread, 0);
    thread_run(tid2);*/
}

void run_nic_tests() {



    test_nicIOArea();
    kprintf("nicIOArea test finished without errors\n");

    /*two threads inside the same buenos
     * one sends and one receives packets*/
    kprintf("starting test_send_receive\n");
    test_send_receive();
    /*sleep until test is finished*/
    while (finished > 0) {
        thread_sleep(1000);
    }
    kprintf("test_send_receive finished without errors\n");

    /*more complex test where multiple thread sends
     and received to multiple ports
     total 2 senders and 2 receivers */
    kprintf("starting test_multiple_send_receive\n");
    test_multiple_send_receive();

    /*sleep until test is finished*/
    while (finished > 0) {
        thread_sleep(1000);
    }
    kprintf("test_multiple_send_receive finished without errors\n");


}
