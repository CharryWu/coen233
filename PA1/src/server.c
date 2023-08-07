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

void init_resp_packet(response_packet *rsp_pkt, request_packet *req_pkt) {
    // Whether ACK or REJECT, the return packets have similar values
    rsp_pkt->start_id = START_ID;
    rsp_pkt->end_id = END_ID;
    rsp_pkt->type = REJECT; // less code to set default REJECT
    rsp_pkt->client_id = req_pkt->client_id;
    rsp_pkt->seg_num = req_pkt->seg_num;
}

void handle_cases(response_packet *rsp_pkt, request_packet *req_pkt, int *packet_counter) {
    // Detect and Handle any errors
    if (req_pkt->seg_num > *packet_counter) { // out-of-sequence would have at least one packet seg no. greater than expected
        log_warn("ERROR: REJECT Sub-Code 1. Out-of-Sequence Packets. Expected seg_num=%d, Got seg_num=%d.", *packet_counter, req_pkt->seg_num);
        rsp_pkt->rej_sub = REJECT_OUT_OF_SEQUENCE;
    } else if ((char)sizeof(req_pkt->payload) != req_pkt->length) {
        log_warn("ERROR: REJECT Sub-Code 2. Length Mis-Match in Packet %d. Expected length: %d, actual length: %d", req_pkt->seg_num, req_pkt->length, (char)sizeof(req_pkt->payload));
        rsp_pkt->rej_sub = REJECT_LENGTH_MISMATCH;
    } else if (req_pkt->end_id != (short)END_ID) {
        log_warn("ERROR: REJECT Sub-Code 3. Invalid End-of-Packet ID: %d, on Packet %d.", req_pkt->end_id, req_pkt->seg_num);
        rsp_pkt->rej_sub = REJECT_PACKET_MISSING;
    } else if (req_pkt->seg_num < *packet_counter) { // seg no. would have at least one packet seg no. less than expected
        log_warn("ERROR: REJECT Sub-Code 4. Duplicate Packets. Expected seg_num=%d, Got Duplicate seg_num=%d.", *packet_counter, req_pkt->seg_num);
        rsp_pkt->rej_sub = REJECT_DUP_PACKET;
    } else {
        // No Errors in the incoming Data Packet
        log_warn("Acknowledged Packet %d. Sending ACK to Client...", req_pkt->seg_num);
        rsp_pkt->type = ACK;
        rsp_pkt->rej_sub = NO_ERROR;
        (*packet_counter)++;
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in server_addr, client_addr; // sock addresses for client and server.
    int server_fd; // socket file descriptor
    int port = DEFAULT_SERVER_PORT;
    socklen_t addrlen = sizeof(struct sockaddr_in); // length of a sockaddr_in to be used in bind() and recvfrom(), sendto()
    int recv_bytes; // received packet size in bytes, used as sanity check
    request_packet req_pkt; // struct to hold data from recv()
    response_packet rsp_pkt; // struct for response packet from server
    int poll_ret; // return value for poll(), the number of fds which status changes been detected. Used as sanity check
    int packet_counter = 0; // packet-segment-num expected
    int is_connect_alive = FALSE; // flag for determining if a connection is still alive
    log_info("test");

    // Set port from command line argument
    if (argc < 2) {
        log_info("Using default port %d <port>", DEFAULT_SERVER_PORT);
    } else {
        log_info("Using port %s", argv[1]);
        port = atoi(argv[1]);
    }

    // Create UDP socket
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        log_fatal("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Setup the Server Sock Addr
    memset((char *)&server_addr, 0, addrlen);
    memset((char *)&client_addr, 0, addrlen);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // accepts traffic from all IPv4 addresses on the local machine
    server_addr.sin_port = htons(port);

    // Bind to the Socket and the Selected Port
    if (bind(server_fd, (struct sockaddr *)&server_addr, addrlen) < 0) {
        log_fatal("Binding Failed.");
        exit(EXIT_FAILURE);
    }

    // Use poll() to detect timeout
    // Unlike the Timer used in the Client (which wait for ACK/REJECT from Server)
    // This Timer is used to determine if the Server needs to drop connection to a Client
    // and instead ready itself for a new client to connect.
    struct pollfd server_timer_pollfd;
    server_timer_pollfd.fd = server_fd;
    server_timer_pollfd.events = POLLIN; // notes anything coming in on the socket.

    log_info("PA1 Server: Listening for incoming connection on port %d", port);

    // ======================== SERVER LOOP ========================
    // since we're using UDP protocol, no need to call accept()
    while (TRUE) {
        if (is_connect_alive) {
            // Detect if socket status has been changed. If changed, then proceed to get data using recvfrom
            // Otherwise, if poll returns, The Server will wait 2 seconds between each received packet.
            // If the Server receives no packets from Client in 2 sec, Server will assume Client has
            // no more packets to send and will reset itself, waiting for next Client.
            poll_ret = poll(&server_timer_pollfd, 1, SERVER_WAIT_TIMEOUT);
            if (poll_ret == 0) { // no state mutated after poll returns, can only be timeout
                // we time-out and reset to wait for a new client.
                log_info("Current client connection time out. Waiting for new client");
                packet_counter = 0; // resetting the expected packet counter.
            } else if (poll_ret < 0) { // handle error polling
                log_error("Error at poll(). Stop.");
                return -1;
            }
        }

        // We wait on the socket to get a data packet from the Client
        recv_bytes = recvfrom(server_fd, &req_pkt, sizeof(request_packet), 0, (struct sockaddr *)&client_addr, &addrlen);
        char * client_ip = inet_ntoa(client_addr.sin_addr);
        // Sanity check: packet has content
        if (recv_bytes < 0) {
            log_error("Error at recvfrom(), client ip = %s", client_ip);
            return -1;
        } else if (recv_bytes == 0) {
            log_warn("Received zero bytes at recvfrom(), client ip = %s", client_ip); // datagram sockets might permit zero length packets
        } else {
            log_info("Message received from client ip = %s", client_ip);
        }

        // Ensures that since we've got an active connection to the client
        // that when Server waits on next packet, it uses the timer
        // just in case the Client stops sending and the Server needs to wait
        // for a new client.
        is_connect_alive = TRUE;
        init_resp_packet(&rsp_pkt, &req_pkt);
        handle_cases(&rsp_pkt, &req_pkt, &packet_counter);

        // Send return packet to the Client via the socket.
        if (sendto(server_fd, &rsp_pkt, sizeof(response_packet), 0, (struct sockaddr *)&client_addr, addrlen) < 0) {
            log_error("Server Error: Failed to Send Packet to Client ip = %s.", client_ip);
            // doesn't return -1 on this failure: Server continues to operate in case issue was on Client's end
        }
    }  // No exit for the Server - it will always wait for Clients. Force-kill Server via CLI (ctrl-C).

    close(server_fd);
    return 0;
}
