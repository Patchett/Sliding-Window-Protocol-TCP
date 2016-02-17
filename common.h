#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
#define MAX_FRAMES_IN_WINDOW 8
#define CRC_POLY 0xEDB88320

typedef unsigned char uchar_t;

//System configuration information
struct SysConfig_t {
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t  SysConfig;

//Command line input information
struct Cmd_t {
    uint16_t src_id;
    uint16_t dst_id;
    char *message;
};
typedef struct Cmd_t Cmd;

//Linked list information
enum LLtype {
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t {
    struct LLnode_t *prev;
    struct LLnode_t *next;
    enum LLtype type;

    void *value;
};
typedef struct LLnode_t LLnode;

#define MAX_FRAME_SIZE 64
//TODO: You should change this!
//Remember, your frame can be AT MOST 64 bytes!
#define FRAME_PAYLOAD_SIZE 55
struct Frame_t {
    uchar_t SeqNum;                // 1 byte
    uint16_t recv_id;              // 2 bytes
    uint16_t send_id;              // 2 bytes
    char data[FRAME_PAYLOAD_SIZE]; // 58 bytes
    uint32_t crc;                  // 4 bytes
};
typedef struct Frame_t Frame;

//Receiver and sender data structures
struct Receiver_t {
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode *input_framelist_head;
    uchar_t last_frame_received; // last ack sent out by the receiver
    uchar_t largest_acceptable_frame; // The highest SeqNum we can receive while staying in the window
    Frame **frame_buffer;
    uchar_t sliding_window;
    int recv_id;
};

struct Sender_t {
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode *input_cmdlist_head;
    LLnode *input_framelist_head;
    int send_id;
    Frame **frame_buffer;
    struct timeval **timeout_times;
    uchar_t last_frame_sent;
    uchar_t last_ack_received;
    uchar_t acks_in_window;
};

enum SendFrame_DstType {
    ReceiverDst,
    SenderDst
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;

//Declare global variables here
//DO NOT CHANGE:
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender *glb_senders_array;
Receiver *glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;
#endif
