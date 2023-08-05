#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "const.h"
#include "log.h"

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main(int argc, char **argv) {
    if (argc < 2) {  // Ensuring that Arguments is available for Client to know which test case to run.
        printf("ERROR: Missing Arguments for determining which Client Test to run.\n");
        return -1;
    }
    int test_number = atoi(argv[1]);  // setting the test case being run.

    struct sockaddr_in client_addr, server_addr;          // sock addresses for client and server.
    int sock_fd;                                          // fd for socket
    socklen_t addr_in_size = sizeof(struct sockaddr_in);  // length of a sockaddr_in
    socklen_t addrlen = sizeof(struct sockaddr_in);       // length of a sockaddr_in
    char buffer[BUFFER_LEN];                              // buffer for putting messages into (because I couldn't bother counting)
    int recv_bytes;                                       // variable to hold length of received message packet
    return_pkt server_pkt;                                // struct to hold return packet from server
    data_pkt client_pkt;                                  // struct for data packet being sent to server
    int send_attempt;                                     // counter for each packet's send attempts
    int poll_ret;                                         // return value for poll (used as timer).

    memset((char *)&server_addr, 0, addr_in_size);
    memset((char *)&client_addr, 0, addr_in_size);

    // Create Client UDP socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        log_fatal("Socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    // Setup the Client Sock Addr
    // Bind it to the Socket and the Selected Port for this communication
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(0);
    if (bind(sock_fd, (struct sockaddr *)&client_addr, addr_in_size) < 0) {
        log_fatal("Binding Failed.\n");
        exit(EXIT_FAILURE);
    }
    // Define the Server Sock Addr that Client needs to connect to for package sending/receiving.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Setting up the Polling socket for doing Time-outs
    //  -- this was recommended when I was complaining to a friend about doing time-outs
    //     so far, it seems to work much better than the timers and forking
    ///   structures I was playing with before.
    struct pollfd acktimer_sockfd;
    acktimer_sockfd.fd = sock_fd;
    acktimer_sockfd.events = POLLIN;  // notes anything coming in on the socket

    // Creating an array of data packets for transmission to the server for testing the socket connection
    // The setup for this was described in the assignment document.
    data_pkt dp_arr[NUM_PACKETS];
    for (int i = 0; i < NUM_PACKETS; i++) {
        // The general data-packet fields that are common to all data-packets sent by the client.
        dp_arr[i].start_id = START_ID;
        dp_arr[i].client_id = CLIENT_ID;
        dp_arr[i].data = DATA;
        dp_arr[i].end_id = END_ID;
        // The more specific details to differentiate each packet (seg-no, payload, length).
        dp_arr[i].seg_num = i;
        log_info(buffer, "Creating packet %d ...\n", i);
        strncpy(dp_arr[i].payload, buffer, LENGTH_MAX);  // used buffer to ensure message fit in the payload
        dp_arr[i].length = sizeof(dp_arr[i].payload);
    }

    // Rather than make separate clients for each test case, I figured I'd just take the base-case
    //(normal operation) and then just use CLI to specify which test case I want the client to run.
    // Thus, by specifying a specific Case, I can force the Server to detect different kinds of errors
    // as described in the problem description.
    // Case 1 - Out of Order Packet Transmission (get a packet early)
    // Case 2 - Mismatch in the Length field of the data packet.
    // Case 3 - The End-of-Packet ID is incorrect.
    // Case 4 - A Duplicated Packet was sent (instead of 0-1-2-3-4 getting 0-1-2-2-4).
    data_pkt tmp;
    int rand_adjust = 0;
    log_info("PA1 Client to Server on Port %d...\n", SERVER_PORT);
    if (test_number == 1) {
        printf("Client Test Case 1: Out-of-Order Packets.\n\n");  // expect 3, get 4
        tmp = dp_arr[3];                                          // just a simple array-element switch
        dp_arr[3] = dp_arr[4];
        dp_arr[4] = tmp;
    } else if (test_number == 2) {
        printf("Client Test Case 2: Mismatch in Length.\n\n");  // mismatch on 3
        while (rand_adjust == 0)
            rand_adjust = (rand() % 10) - 5;  // using a random number adjustment (for fun) to create bad length
        dp_arr[3].length = dp_arr[3].length + rand_adjust;
    } else if (test_number == 3) {
        printf("Client Test Case 3: Incorrect End of Packet ID.\n\n");  // bad eop on 3
        dp_arr[3].end_id = 0x1234;                                      // random value that isn't the specified END_ID value defined in the doc.
    } else if (test_number == 4) {
        printf("Client Test Case 4: Duplicate Packets.\n\n");  // expect 3, get 2
        dp_arr[3] = dp_arr[2];                                 // made packet 3 a copy of packet 2.
    } else {
        printf("Client is Sending Data Normally, Not Testing Error Handling.\n\n");
    }

    // Loop through the packet-array and start sending data packets until we've run out.
    for (int pack_num = 0; pack_num < NUM_PACKETS; pack_num++) {
        client_pkt = dp_arr[pack_num];  // specify which packet in the array we're sending
        send_attempt = 1;               // initialize the attempt number (we try thrice).
        // Send the packe to the server via the set-up socket connections.
        printf("Client is sending Packet %d to Server. Attempt %d\n", pack_num, send_attempt);
        if (sendto(sock_fd, &client_pkt, sizeof(data_pkt), 0, (struct sockaddr *)&server_addr, addrlen) < 0) {
            fprintf(stderr, "Client Experienced Error in Sending Packet %d to Server.\n", pack_num);
            return -1;
        }

        while (send_attempt <= CLIENT_MAX_ATTEMPTS) {
            poll_ret = poll(&acktimer_sockfd, 1, CLIENT_RECV_TIMEOUT);  // The timer waits for three seconds to get an ACK
            if (poll_ret == 0) {                         // Poll Timer ran out and we didn't get the ACK.
                send_attempt++;                          // Increment Send Attempt counter.
                // Try sending the packet again.
                if (send_attempt <= CLIENT_MAX_ATTEMPTS) {
                    printf("No Response from Server to Client. Attempt %d. Retransmitting...\n", send_attempt);
                    if (sendto(sock_fd, &client_pkt, sizeof(data_pkt), 0, (struct sockaddr *)&server_addr, addrlen) < 0) {
                        fprintf(stderr, "Client Experienced Error in Sending Packet %d to Server.\n", pack_num);
                        return -1;
                    }
                }
            } else if (poll_ret == -1) {  // Something's gone wrong with the poll - abort!
                fprintf(stderr, "Client Experienced Error in Polling.\n");
                return -1;
            } else {  // Got something before time-out, need to read it in from the socket.
                // Reading in the return packet from the socket.
                recv_bytes = recvfrom(sock_fd, &server_pkt, sizeof(return_pkt), 0, (struct sockaddr *)&server_addr, &addrlen);
                log_info("Received %d bytes from server", recv_bytes);
                if (recv_bytes == -1) {  // bad packet received. abort due to error in connection.
                    log_fatal("Client Experienced Error in Receiving server_pkt from Server.");
                    return -1;
                }
                log_info("Server packet type = %d", server_pkt.type);
                log_info("Server packet rej_sub = %d", server_pkt.rej_sub);
                // Now, we need to check what kind of packet we got...
                if (server_pkt.type == (short)ACK) {
                    // Return Packet was an ACK. Yay!
                    // We can just skip to the next iteration of the for loop.
                    log_info("Received ACK for Packet %d from Server.\n", pack_num);
                    break;
                } else if (server_pkt.type == (short)REJECT) {
                    // Determine by error code
                    if (server_pkt.rej_sub == (short)REJECT_OUT_OF_SEQUENCE) {
                        printf("Error: REJECT Sub-Code 1. Out-of-Order Packets. Expected %d, Got %d.\n", pack_num, server_pkt.seg_num);
                        return -1;
                    } else if (server_pkt.rej_sub == (short)REJECT_LENGTH_MISMATCH) {
                        printf("Error: REJECT Sub-Code 2. Length Mis-Match in Packet %d.\n", pack_num);
                        return -1;
                    } else if (server_pkt.rej_sub == (short)REJECT_PACKET_MISSING) {
                        printf("Error: REJECT Sub-Code 3. Invalid End-of-Packet ID on Packet %d.\n", pack_num);
                        return -1;
                    } else if (server_pkt.rej_sub == (short)REJECT_DUP_PACKET) {
                        printf("Error: REJECT Sub-Code 4. Duplicate Packets. Expected %d, Got Duplicate %d.\n", pack_num, server_pkt.seg_num);
                        return -1;
                    }
                } else {
                    // I don't expect this to ever be relevant, but just on the safe side...
                    fprintf(stderr, "Client Error -- Received neither ACK or REJECT Packet.\n");
                    return -1;
                }
            }
        }

        // If we've somehow timed out after three attempts at transmission,
        // Then we need to quit sending packets and exit.
        if (send_attempt > 3) {
            log_info("Time-Out Error: Client Attmpted to Send Packet %d thrice. Quit.", pack_num);
            close(sock_fd);
            return -1;
        }
    }

    close(sock_fd);  // need to close the socket now that we're done with it (bad practice if we left it).
    // Wrap things up with a message for the user, and exit with grace.
    log_info("\nClient Received All Packets With No Errors.\nExiting...\n");
    return 0;
}
