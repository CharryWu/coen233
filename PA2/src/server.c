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

/**
 * Linear search on subscriber number array subn_arr to find the sub_num
 * Return index if found; -1 if not found
*/
int find(unsigned long *subn_arr, int arr_len, unsigned long sub_num) {
    int ret_index = -1;
    for (int i = 0; i < arr_len; i++) {
        if (subn_arr[i] == sub_num) {
            ret_index = i;
            break;
        }
    }
    return ret_index;
}

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
    // Initializing values for completing socket programming communications
    // Most of the following is just lifted from Assignment 1.
    struct sockaddr_in server_addr, client_addr;      // sock addresses for client and server.
    int server_fd;                                    // fd for socket
    socklen_t addr_len = sizeof(struct sockaddr_in);  // length of a sockaddr_in
    int recv_bytes;                                   // variable to hold length of received message packet
    message_packet client_pkt;                        // struct to hold data packet being sent to server
    message_packet server_pkt;                        // struct for return packet from server
    int index = -1;                                   // Index of a Subscriber Number on the Verified Database

    // Creating a UDP Socket for the Client
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        log_fatal("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Setup the Server Sock Addr
    // Bind it to the Socket and the Selected Port for this communication
    memset((char *)&server_addr, 0, addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&server_addr, addr_len) < 0) {
        log_fatal("Binding Failed.");
        exit(EXIT_FAILURE);
    }

    // Use poll() to detect timeout, easier to use than using C timers
    struct pollfd server_timer_pollfd;
    server_timer_pollfd.fd = server_fd;
    server_timer_pollfd.events = POLLIN;  // notes anything coming in on the socket.

    // ======================== SERVER LOOP ========================
    while (TRUE) {
        // We wait on the socket to get a data packet from the Client
        recv_bytes = recvfrom(server_fd, &client_pkt, sizeof(message_packet), 0, (struct sockaddr *)&client_addr, &addr_len);
        char *client_ip = inet_ntoa(client_addr.sin_addr);
        // Sanity check: packet has content
        if (recv_bytes < 0) {
            log_error("Error at recvfrom(), client ip = %s", client_ip);
            return -1;
        } else if (recv_bytes == 0) {
            log_warn("Received zero bytes at recvfrom(), client ip = %s", client_ip);  // datagram sockets might permit zero length packets
        } else {
            log_info("Message received from client ip = %s", client_ip);
        }

        // Data packes sent back to the user have several commonalities, regardless of response type.
        server_pkt.start_id = START_ID;
        server_pkt.end_id = END_ID;
        server_pkt.client_id = client_pkt.client_id;
        server_pkt.seg_num = client_pkt.seg_num;
        server_pkt.technology = client_pkt.technology;  // This will get changed later if there's a Tech Mis-Match.
        server_pkt.sub_num = client_pkt.sub_num;
        server_pkt.length = sizeof(client_pkt.technology) + sizeof(client_pkt.sub_num);

        // First, search the database for the client's subscriber number, and verify it.
        index = find(sub_nums, db_len, client_pkt.sub_num);
        // Now, run through verification checks
        if (index < 0) {  // The subscriber number couldn't be found on the database.
            log_warn("Access Denied: Subscriber %lu Does Not Exist in the Verification Database.", client_pkt.sub_num);
            server_pkt.type = NOT_EXIST;
        } else if (client_pkt.technology != sub_techs[index]) {  // The subscriber number asked for the wrong Technology
            log_warn("Access Denied: Subscriber %lu Requested Access to Incorrect Technology. Requested %dG, but is authorized for %dG.", client_pkt.sub_num, (int)client_pkt.technology, (int)sub_techs[index]);
            server_pkt.type = NOT_EXIST;
            server_pkt.technology = (char)INVALID_TECHNOLOGY;
        } else if (sub_paid_arr[index] == 0) {  // The subscriber number has not paid.
            log_warn("Access Denied: Subscriber %lu have not paid.", client_pkt.sub_num);
            server_pkt.type = NOT_PAID;
        } else {  // No issues found in database or client-packet. Give Access Permission to Client.
            log_info("Access Granted: Subscriber %lu request has been verified against the Database.", client_pkt.sub_num);
            server_pkt.type = ACC_OK;
        }

        // Send information packet back to client
        if (sendto(server_fd, &server_pkt, sizeof(message_packet), 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
            log_error("Server Error: Failed to Send Packet to Client ip = %s.", client_ip);
            // doesn't return -1 on this failure: Server continues to operate in case issue was on Client's end
        }
    }
    close(server_fd);
    return 0;
}
