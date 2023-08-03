#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <math.h>
#include <arpa/inet.h>
#include "const.h"
#include "log.h"

int main()
{
    struct sockaddr_in server_addr, client_addr; // sock addresses for client and server.
    int server_fd;                               // fd for socket
    socklen_t addr_len = sizeof(client_addr);    // length of a sockaddr_in
    int recv_len;                                // variable to hold length of received message packet
    data_pkt client_pkt;                         // struct to hold data packet being sent to server
    return_pkt server_pkt;                       // struct for return packet from server
    int poll_ret;                                // return value for poll (used as timer).
    int exp_pkt = 0;                             // packet-segment-num expected
    int client_active = 0;                       // flag for determining if a Client is connected to the Server (C has no bool)

    // Creating a UDP Socket for the Client
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        log_fatal("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    // Setup the Server Sock Addr
    // Bind it to the Socket and the Selected Port for this communication
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        log_fatal("Sever Error -- Failed to Bind Socket to Local Address and Selected Port.\n");
        exit(EXIT_FAILURE);
    }

    // Setting up the Polling socket for doing Time-outs
    //  --- In contrast to the Timer used in the Client (which wait for ACK/REJECT from Server)
    //      This Timer is used to determine if the Server needs to drop connection to a Client
    //      and isntead ready itself for a new client to connect.
    struct pollfd newclientpoll_sockfd;
    newclientpoll_sockfd.fd = server_fd;
    newclientpoll_sockfd.events = POLLIN; // notes anything coming in on the socket.

    printf("COEN 233 Fall 2020 Assignment 1\nInitializing Server.\nWaiting for Client on Port %d.\n---\n\n", SERVER_PORT);

    // The Server will run ad-infinitum to service clients.
    while (1)
    {
        if (client_active)
        {
            // The Server wll only Wait 2.5 seconds between each received packet.
            // If the Server receives no packets from Client in 2.5 sec, Server will assume Client has
            // no more packets to send and will reset itself, waiting for next Client.
            poll_ret = poll(&newclientpoll_sockfd, 1, 2500); // Timer waits 2.5 seconds
            if (poll_ret == 0)
            {
                // if the client does not respond in 2.5 sec, we time-out and reset to wait for a new client.
                printf("---\nServer-Client Connection Timed Out. Preparing for New Client...\nWaiting for Client on Port %d.\n---\n\n", SERVER_PORT);
                exp_pkt = 0; // resetting the expected, incoming packet-segment counter.
            }
        }

        // We wait on the socket to get a data packet from the Client
        recv_len = recvfrom(server_fd, &client_pkt, sizeof(data_pkt), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len > 0)
        { // make sure tha packet was received properly.
            printf("Message Received from Client!\nMessage: \"%s\"\n", client_pkt.payload);
        }
        else
        {
            printf("Server Error -- Error in Package Reception from Client!\n");
        }

        client_active = 1; // Ensures that since we've got an active connection to the client
                           // that when Server waits on next packet, it uses the timer
                           // just in case the Client stops sending and the Server needs to wait
                           // for a new client.

        // Return ACK/REJECT packets have some similar values. Setting them now.
        server_pkt.start_id = START_ID;
        server_pkt.end_id = END_ID;
        server_pkt.type = REJECT; // easier to first REJECT (more REJECT cases than ACK), then fix it later
        server_pkt.client_id = client_pkt.client_id;
        server_pkt.seg_num = client_pkt.seg_num;

        client_pkt.seg_num = client_pkt.seg_num;

        // Check the Incoming Data Packet for Errors.
        // If there were Errors, mark the Return packet with the REJECT sub-code for the error.
        if (client_pkt.seg_num > exp_pkt)
        {
            printf("ERROR: REJECT Sub-Code 1. Out-of-Order Packets. Expected %d, Got %d.\n", exp_pkt, client_pkt.seg_num);
            server_pkt.rej_sub = REJECT_OUT_OF_SEQUENCE;
        }
        else if ((char)sizeof(client_pkt.payload) != client_pkt.length)
        {
            printf("ERROR: REJECT Sub-Code 2. Length Mis-Match in Packet %d.\n", client_pkt.seg_num);
            server_pkt.rej_sub = REJECT_LENGTH_MISMATCH;
        }
        else if (client_pkt.end_id != (short)END_ID)
        {
            printf("ERROR: REJECT Sub-Code 3. Invalid End-of-Packet ID on Packet %d.\n", client_pkt.seg_num);
            server_pkt.rej_sub = REJECT_PACKET_MISSING;
        }
        else if (client_pkt.seg_num < exp_pkt)
        {
            printf("ERROR: REJECT Sub-Code 4. Duplicate Packets. Expected %d, Got Duplicate %d.\n", exp_pkt, client_pkt.seg_num);
            server_pkt.rej_sub = REJECT_DUP_PACKET;
        }
        else
        {
            // No Errors in the incoming Data Packet. Send back an ACK.
            printf("Acknowledged Packet %d. Sending ACK to Client...\n\n", client_pkt.seg_num);
            server_pkt.type = ACK;
            server_pkt.rej_sub = 0x0000; // I got lazy here, and didn't bother making two return packet types
                                         // especially since in problem 2, there's only 1 return packet anyways
                                         // and I hope to reuse most of my code here for prob 2.
            exp_pkt++; // expect next packet
        }

        // Actually send that return packet to the Client via the socket.
        if (sendto(server_fd, &server_pkt, sizeof(return_pkt), 0, (struct sockaddr *)&client_addr, addr_len) < 0)
        {
            fprintf(stderr, "\nServer Error -- Failed to Send Packet to Client.\n\n");
            // doesn't return -1 on this failure: Server continues to operate in case issue was on Client's end
        }
    } // No exit for the Server - it will always wait for Clients. Force-kill Server via CLI (ctrl-C).
}
