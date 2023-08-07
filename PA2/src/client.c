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

int main(int argc, char **argv) {
    // ======================== CLI ARGS PARSING ========================
    int port = DEFAULT_SERVER_PORT;
    // Set port from command line argument
    if (argc < 2) {
        log_info("Using default port %d <port>", DEFAULT_SERVER_PORT);
    } else {
        log_info("Using port %s", argv[1]);
        port = atoi(argv[1]);
    }

    // ======================== DB FILE PARSING ========================
    char buffer[128] = {0};  // a buffer to store intermediary string for parsing
    int db_len;              // Number of entries in db
    char wc_command[50];
    strcpy(wc_command, "wc -l ");
    FILE *db_wc = popen(strcat(wc_command, DB_FILE_NAME), "r");
    if (!db_wc) {
        log_error("DB Error: Could not get num entries in DB. Quit.");
        return -1;
    }
    fscanf(db_wc, "%s", buffer);
    db_len = atoi(buffer);
    pclose(db_wc);

    // parse the following field arrays from database file
    unsigned long sub_nums[db_len];  // all subscriber numbers in the database
    char sub_techs[db_len];          // technology which each subscribers is using
    char sub_paid_arr[db_len];       // subscribers payment status (1 = paid, 0 = not paid).

    // The following are just variables for getting the data out.
    size_t len = 0;
    ssize_t read;
    char *line = NULL;
    char *token = NULL;
    int idx = 0;
    char *end_ptr;

    FILE *input_dbfile = fopen(DB_FILE_NAME, "r");
    if (!input_dbfile) {
        log_error("DB Error: Could not open %s. Quit.", DB_FILE_NAME);
        return -1;
    }
    while ((read = getline(&line, &len, input_dbfile)) != -1) {
        token = strtok(line, " ");  // this grabs the subscriber-number and puts it into "token"
        // Skip any non-numeric characters in the subscriber numer, example '-' or '.'
        int j = 0;
        for (int k = 0; k < strlen(token); k++) {
            if (token[k] >= '0' && token[k] <= '9') {
                buffer[j++] = token[k];
            }
        }
        buffer[j] = '\0';
        sub_nums[idx] = strtoul(buffer, &end_ptr, 10);      // Parse substriber number and save to an unsigned long (4bytes)
        sub_techs[idx] = (char)atoi(strtok(NULL, " "));     // Parse technology field
        sub_paid_arr[idx] = (char)atoi(strtok(NULL, " "));  // Parse paid field
        idx++;
    }
    fclose(input_dbfile);  // done with the data-base file. We can close it now.

    // ======================== INIT VARIABLES AND SOCKETS ========================
    struct sockaddr_in client_addr, server_addr;  // sock addresses for client and server.
    int sock_fd;                                  // fd for socket
    socklen_t addr_len = sizeof(client_addr);     // length of a sockaddr_in
    int recv_len;                                 // variable to hold length of received message packet
    message_packet server_pkt;                    // struct to hold return packet from server
    message_packet client_pkt;                    // struct for data packet being sent to server
    int attempt_counter;                          // counter for each packet's send attempts
    int poll_res;                                 // return value for poll (used as timer).

    // Creating a UDP Socket for the Client
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        log_fatal("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Setup the Client Sock Addr
    // Bind it to the Socket and the Selected Port for this communication
    memset((char *)&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(0);

    if (bind(sock_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        log_fatal("Binding Failed.");
        exit(EXIT_FAILURE);
    }
    // Define the Server Sock Addr that Client needs to connect to for package sending/receiving.
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Setting up the Polling socket for doing Time-outs
    //  -- See reasoning in Assignment 1, client.c's comments.
    struct pollfd client_timer_pollfd;
    client_timer_pollfd.fd = sock_fd;
    client_timer_pollfd.events = POLLIN;  // notes anything coming in on the socket

    // Here, we fill out an array of data-packets containing the Test Cases we want to send the Server
    message_packet dp_arr[db_len + 1];
    for (int i = 0; i < db_len; i++) {
        dp_arr[i].start_id = START_ID;
        dp_arr[i].client_id = CLIENT_ID;
        dp_arr[i].type = ACC_PER;
        dp_arr[i].end_id = END_ID;
        dp_arr[i].seg_num = i;
        dp_arr[i].technology = sub_techs[i];
        dp_arr[i].sub_num = sub_nums[i];
        dp_arr[i].length = sizeof(dp_arr[i].technology) + sizeof(dp_arr[i].sub_num);
    }
    // Adding the Modifications for Testing Cases 3 and 5.
    // Test Case 3
    dp_arr[2].technology = 0x06;  // There is no 6G network, so this shouldn't pass.
    dp_arr[2].length = sizeof(dp_arr[2].technology) + sizeof(dp_arr[2].sub_num);
    // Test Case 5
    dp_arr[db_len] = dp_arr[db_len - 1];  // just copy the previous packet, but give a bad subscriber number.
    dp_arr[db_len].sub_num = strtoul("4084400332", &end_ptr, 10);
    dp_arr[db_len].length = sizeof(dp_arr[db_len].technology) + sizeof(dp_arr[db_len].sub_num);

    // Start Sending Packets for Verification.
    for (int packet_num = 0; packet_num < (db_len + 1); packet_num++) {
        client_pkt = dp_arr[packet_num];  // specify which packet in the array we're sending
        attempt_counter = 1;              // initialize the attempt number (we try thrice).

        // Send the packet to the server via the set-up socket connections.
        log_info("Client is sending Packet %d (sub#: %lu) to Server. Attempt %d\n", packet_num, client_pkt.sub_num, attempt_counter);
        if (sendto(sock_fd, &client_pkt, sizeof(message_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
            log_error("Error: Test case %d: sendto() packet number %d", packet_num);
            return -1;
        }

        while (attempt_counter <= 3) {
            poll_res = poll(&client_timer_pollfd, 1, CLIENT_RECV_TIMEOUT);  // The timer waits for three seconds to get an ACK
            if (poll_res > 0) {
                recv_len = recvfrom(sock_fd, &server_pkt, sizeof(message_packet), 0, (struct sockaddr *)&server_addr, &addr_len);
                if (recv_len == -1) {  // bad packet received. abort due to error in connection.
                    fprintf(stderr, "Client Experienced Error in Receiving server_pkt from Server.\n");
                    return -1;
                }
                // Handling server response
                if (server_pkt.type == (short)ACC_OK) {
                    // Successfully received ACK, send next packet
                    log_info("Received ACCESS OKAY for Packet %d (sub#: %lu) from Server.\nSubscriber May Access the Network.", packet_num, server_pkt.sub_num);
                    break;
                } else if (server_pkt.type == (short)NOT_PAID) {
                    log_warn("Error: Received NOT_PAID for Packet %d (sub#: %lu) from Server.\nSubscriber Has Not Paid for Access.", packet_num, server_pkt.sub_num);
                    break;
                } else if (server_pkt.type == (short)NOT_EXIST && server_pkt.technology == (char)0) {
                    log_warn("Error: Received NOT_EXIST for Packet %d (sub#: %lu) from Server.\nSubscriber Exists in the Database, but requests Incorrect Technology.", packet_num, server_pkt.sub_num);
                    break;
                } else if (server_pkt.type == (short)NOT_EXIST) {
                    log_warn("Error: Received NOT_EXIST for Packet %d (sub#: %lu) from Server.\nSubscriber Does Not Exist in the Database.", packet_num, server_pkt.sub_num);
                    break;
                } else {
                    log_error("Client Error -- Received neither ACK or REJECT Packet.");
                    return -1;
                }
            } else if (poll_res == 0) {
                attempt_counter++;
                // Retry
                if (attempt_counter <= 3) {
                    log_info("No Response from Server to Client. Attempt %d. Retransmitting...\n", attempt_counter);
                    if (sendto(sock_fd, &client_pkt, sizeof(message_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
                        log_error("Client experienced error in sending packet %d to Server.", packet_num);
                        return -1;
                    }
                }
            } else {
                log_error("Client Experienced Error in Polling. Stop.");
                return -1;
            }
        }

        // If we've somehow timed out after three attempts at transmission,
        // Then we need to quit sending packets and exit.
        if (attempt_counter > CLIENT_MAX_ATTEMPTS) {
            log_error("Retry timeout: Client attmpted to send packet %d with sub num: %lu three times and failed to get any responses from server. Quit.", packet_num, client_pkt.sub_num);
            return -1;
        }
    }

    close(sock_fd);
    log_info("Sent all packets successfully. End.");
    return 0;
}
