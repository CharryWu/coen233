#ifndef START_ID
#define START_ID 0xFFFF
#endif

#ifndef END_ID
#define END_ID 0xFFFF
#endif

#ifndef LENGTH_MAX
#define LENGTH_MAX 0xFF
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
#ifndef SERVER_PORT
#define SERVER_PORT 23456
#endif

#ifndef BUFFER_LEN
#define BUFFER_LEN 2048
#endif

// client ID - set it myself to 0x42 because 42's the answer to life.
#ifndef CLIENT_ID
#define CLIENT_ID 0x42
#endif

// Number of packets the client will send the server.
#ifndef NUM_PACKETS
#define NUM_PACKETS 5
#endif


//Data structures for sending data to the server and receiving return ACK/REJECTs.
typedef struct data_pkt {
    short start_id;
    char client_id;
    short data;
    char seg_num;
    char length;
    char payload[LENGTH_MAX];
    short end_id;
} data_pkt;

typedef struct return_pkt {
    short start_id;
    char client_id;
    short type;
    short rej_sub;
    char seg_num;
    short end_id;
} return_pkt;
