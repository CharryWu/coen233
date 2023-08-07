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

void init_request_packets(request_packet req_pkts[NUM_PACKETS], char buffer[BUFFER_LEN]) {
    for (int i = 0; i < NUM_PACKETS; i++) {
        req_pkts[i].start_id = START_ID;
        req_pkts[i].client_id = CLIENT_ID;
        req_pkts[i].data = DATA;
        req_pkts[i].end_id = END_ID;
        // The more specific details to differentiate each packet (seg-no, payload, length).
        req_pkts[i].seg_num = i;
        strncpy(req_pkts[i].payload, buffer, LENGTH_MAX);  // used buffer to ensure message fit in the payload
        req_pkts[i].length = sizeof(req_pkts[i].payload);
    }
}

void init_test_case(int test_number, request_packet req_pkts[NUM_PACKETS]) {
    // Fill the req_pkts according to the test_number given by user cli input
    request_packet tmp;
    switch (test_number) {
        case 0: {
            log_info("Setting normal test cases...");
            break;
        }
        case 1: {
            log_info("Setting Test Case 1: Out-of-Order Packets.");
            tmp = req_pkts[1];  // swap any two packets
            req_pkts[1] = req_pkts[2];
            req_pkts[2] = tmp;
            break;
        }
        case 2: {
            log_info("Setting Test Case 2: Mismatch in Length.");
            req_pkts[2].length += 1; // Adding 1 to length to create bad length (off-by-one error)
            break;
        }
        case 3: {
            log_info("Setting Test Case 3: Incorrect End of Packet ID.");
            req_pkts[4].end_id = END_ID - 1; // One less than the specified end id
            break;
        }
        case 4: {
            log_info("Setting Test Case 4: Duplicate Packets.");
            req_pkts[4] = req_pkts[3]; // duplicate packet 3
            break;
        }
        default: {
            log_info("Unrecognized Test Case Number. Stop.");
        }
    }
}

void detect_print_error(response_packet *rsp_pkt, int index) {
    switch (rsp_pkt->rej_sub) {
        case (short)REJECT_OUT_OF_SEQUENCE:
            log_error("Error: REJECT Sub-Code 1. Out-of-Order Packets. Expected %d, Got %d.\n", index, rsp_pkt->seg_num);
            break;
        case (short)REJECT_LENGTH_MISMATCH:
            log_error("Error: REJECT Sub-Code 2. Length Mis-Match in Packet %d.", index);
            break;
        case (short)REJECT_PACKET_MISSING:
            log_error("Error: REJECT Sub-Code 3. Invalid End-of-Packet ID on Packet %d.", index);
            break;
        case (short)REJECT_DUP_PACKET:
            log_error("Error: REJECT Sub-Code 4. Duplicate Packets. Expected %d, Got Duplicate %d.", index, rsp_pkt->seg_num);
            break;
        default:
            log_error("Error: REJECT unrecognized subcode in Packet %d.", index);
    }
}

int main(int argc, char **argv) {
    // Handle CLI arguments: test_number and port
    int port = DEFAULT_SERVER_PORT;
    if (argc < 2) {  // Ensuring that Arguments is available for Client to know which test case to run.
        log_fatal("ERROR: Missing Arguments for determining which Client Test to run.");
        exit(EXIT_FAILURE);
    } else if (argc == 2) {
        log_info("Send to default server port %d <port>", DEFAULT_SERVER_PORT);
    } else {
        log_info("Send to port %s", argv[2]);
        port = atoi(argv[2]);
    }
    int test_number = atoi(argv[1]);  // setting the test case being run.
    if (test_number < 0 || test_number > 4) {
        log_error("Unrecognized test case number. Stop.");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr, server_addr;     // sock addresses for client and server.
    int client_sock_fd;                              // fd for client socket
    socklen_t addrlen = sizeof(struct sockaddr_in);  // length of a sockaddr_in to be used in bind() and recvfrom(), sendto()
    int recv_bytes;                                  // received packet size in bytes, used as sanity check
    response_packet rsp_pkt;                         // struct for response packet from server
    int poll_ret;                                    // return value for poll(), the number of fds which status changes been detected. Used as sanity check
    char buffer[BUFFER_LEN];                         // buffer for putting messages into (because I couldn't bother counting)

    memset((char *)&server_addr, 0, addrlen);
    memset((char *)&client_addr, 0, addrlen);

    // Create Client UDP socket
    if ((client_sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        log_fatal("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Setup the Client Sock Addr
    // Bind it to the Socket and the Selected Port for this communication
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // no need to specify client port

    // Bind to the Socket and the Selected Port
    if (bind(client_sock_fd, (struct sockaddr *)&client_addr, addrlen) < 0) {
        log_fatal("Binding Failed.");
        exit(EXIT_FAILURE);
    }
    // Define the Server Sock Addr that Client needs to connect to for package sending/receiving.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Use poll() to detect timeout
    struct pollfd client_timer_pollfd;
    client_timer_pollfd.fd = client_sock_fd;
    client_timer_pollfd.events = POLLIN;

    // Initialize request packets for testing
    request_packet req_pkts[NUM_PACKETS];  // holds 5 request packets for testing
    init_request_packets(req_pkts, buffer);
    init_test_case(test_number, req_pkts);

    // Send all packets in req_pkts
    for (int i = 0; i < NUM_PACKETS; i++) {
        request_packet req_pkt = req_pkts[i];  // Current packet
        unsigned int attempt_counter = 1;      // counter for each packet's send attempts
        attempt_counter = 1;                   // record number of attempts to send current req_pkt so far
        // Send the packe to the server via the set-up socket connections.
        log_info("Client is sending Packet %d to Server. Attempt %d", i, attempt_counter);
        if (sendto(client_sock_fd, &req_pkt, sizeof(request_packet), 0, (struct sockaddr *)&server_addr, addrlen) < 0) {
            log_error("Test case %d: sendto() packet number %d", test_number, i);
            return -1;
        }

        while (attempt_counter <= CLIENT_MAX_ATTEMPTS) {
            poll_ret = poll(&client_timer_pollfd, 1, CLIENT_RECV_TIMEOUT);
            if (poll_ret > 0) {  // Got something before time-out, need to read it in from the socket.
                // Reading in the return packet from the socket.
                recv_bytes = recvfrom(client_sock_fd, &rsp_pkt, sizeof(response_packet), 0, (struct sockaddr *)&server_addr, &addrlen);
                log_info("Received %d bytes from server", recv_bytes);
                if (recv_bytes < 0) {  // bad packet received. abort due to error in connection.
                    log_fatal("Client Experienced Error in Receiving rsp_pkt from Server.");
                    return -1;
                } else if (recv_bytes == 0) {
                    log_warn("Client receive zero bytes packet from server.");
                }

                // Handling server response
                if (rsp_pkt.type == (short)ACK) {
                    // Successfully received ACK, send next packet
                    log_info("Received ACK for Packet %d from Server.", i);
                    break;
                } else if (rsp_pkt.type == (short)REJECT) {
                    log_warn("Received REJECT for Packet %d from Server.", i);
                    detect_print_error(&rsp_pkt, i);
                    return -1;
                } else {
                    // I don't expect this to ever be relevant, but just on the safe side...
                    log_error("Unrecognized packet %d type: neither ACK or REJECT Packet. Quit.", i);
                    return -1;
                }
            } else if (poll_ret == 0) {
                attempt_counter++;
                // Retry
                if (attempt_counter <= CLIENT_MAX_ATTEMPTS) {
                    log_warn("No Response from Server to Client. Attempt %d. Retransmitting...", attempt_counter);
                    if (sendto(client_sock_fd, &req_pkt, sizeof(request_packet), 0, (struct sockaddr *)&server_addr, addrlen) < 0) {
                        log_error("Client experienced error in sending packet %d to Server.", i);
                        return -1;
                    }
                }
            } else {  // Something's gone wrong with the poll - abort!
                log_error("Client Experienced Error in Polling. Stop.");
                return -1;
            }
        }

        // If we've somehow timed out after three attempts at transmission,
        // Then we need to quit sending packets and exit.
        if (attempt_counter > CLIENT_MAX_ATTEMPTS) {
            log_error("Time-Out Error: Client Attmpted to Send Packet %d thrice. Quit.", i);
            close(client_sock_fd);
            return -1;
        }
    }

    close(client_sock_fd);
    log_info("Sent all packets successfully. End.");
    return 0;
}
