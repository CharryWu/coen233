#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef INVALID_TECHNOLOGY
#define INVALID_TECHNOLOGY 0
#endif

#ifndef DB_FILE_NAME
#define DB_FILE_NAME "Verification_Database.txt"
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

#ifndef ACC_PER
#define ACC_PER 0xFFF8
#endif

#ifndef NOT_PAID
#define NOT_PAID 0xFFF9
#endif

#ifndef NOT_EXIST
#define NOT_EXIST 0xFFFA
#endif

#ifndef ACC_OK
#define ACC_OK 0xFFFB
#endif

// User-made Definitions for hard-coded values.
// hard-coded the port number (picked it randomly, and it was available).
#ifndef DEFAULT_SERVER_PORT
#define DEFAULT_SERVER_PORT 8080
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

// Timeout for client to receive next ACK packet from the server
#ifndef CLIENT_RECV_TIMEOUT
#define CLIENT_RECV_TIMEOUT 3000
#endif

//Data structure for sending and receiving data with the Client.
typedef struct message_packet {
    short start_id;
    char client_id;
    short type;
    char seg_num;
    char length;
    char technology;
    unsigned long sub_num;
    short end_id;
} message_packet;
