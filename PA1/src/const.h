#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef START_ID
#define START_ID 0xFFFF
#endif

#ifndef END_ID
#define END_ID 0xFFFF
#endif

#ifndef LENGTH_MAX
#define LENGTH_MAX 0xFF
#endif

#ifndef MAX_ID
#define MAX_ID 0xFF
#endif

#ifndef DATA
#define DATA 0xFFF1
#endif

#ifndef ACK
#define ACK 0xFFF2
#endif

#ifndef REJECT
#define REJECT 0xFFF3
#endif

// Reject out of sequence
#ifndef NO_ERROR
#define NO_ERROR 0x0000
#endif

// Reject out of sequence
#ifndef REJECT_OUT_OF_SEQUENCE
#define REJECT_OUT_OF_SEQUENCE 0xFFF4
#endif

// Reject length mismatch
#ifndef REJECT_LENGTH_MISMATCH
#define REJECT_LENGTH_MISMATCH 0xFFF5
#endif

// Reject end of packet missing
#ifndef REJECT_PACKET_MISSING
#define REJECT_PACKET_MISSING 0xFFF6
#endif

// Reject duplicate packet
#ifndef REJECT_DUP_PACKET
#define REJECT_DUP_PACKET 0xFFF7
#endif

// User-made Definitions for hard-coded values.
// hard-coded the port number (picked it randomly, and it was available).
#ifndef DEFAULT_SERVER_PORT
#define DEFAULT_SERVER_PORT 8080
#endif

#ifndef BUFFER_LEN
#define BUFFER_LEN 2048
#endif

// client ID
#ifndef CLIENT_ID
#define CLIENT_ID 0x00
#endif

// Number of packets the client will send the server.
#ifndef NUM_PACKETS
#define NUM_PACKETS 5
#endif

// Number of packets the client will send the server.
#ifndef CLIENT_MAX_ATTEMPTS
#define CLIENT_MAX_ATTEMPTS 3
#endif

// Timeout for server to receive next packet from the same client
#ifndef SERVER_WAIT_TIMEOUT
#define SERVER_WAIT_TIMEOUT 2000
#endif

// Timeout for client to receive next ACK packet from the server
#ifndef CLIENT_RECV_TIMEOUT
#define CLIENT_RECV_TIMEOUT 3000
#endif

// Client packet struct
typedef struct request_packet {
    short start_id;
    char client_id;
    short data;
    char seg_num;
    char length;
    char payload[LENGTH_MAX];
    short end_id;
} request_packet;

// Unified server return packet interface
// For both ACK/REJECT packets
typedef struct response_packet {
    short start_id;
    char client_id;
    short type;
    short rej_sub;
    char seg_num;
    short end_id;
} response_packet;
