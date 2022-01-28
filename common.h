#ifndef __COMMON_H__
#define __COMMON_H__

#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
typedef unsigned char uchar_t;

// System configuration information
struct SysConfig_t {
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t SysConfig;

// Command line input information
struct Cmd_t {
    uint16_t src_id;
    uint16_t dst_id;
    char* message;
};
typedef struct Cmd_t Cmd;

// Linked list information
enum LLtype { llt_string, llt_frame, llt_integer, llt_head } LLtype;

struct LLnode_t {
    struct LLnode_t* prev;
    struct LLnode_t* next;
    enum LLtype type;

    void* value;
};
typedef struct LLnode_t LLnode;

#define MAX_FRAME_SIZE 64
#define GENERATOR 9
// TODO: You should change this!
// Remember, your frame can be AT MOST 64 bytes!
#define FRAME_PAYLOAD_SIZE 58
struct Frame_t {
    unsigned char src_id;           // 1b
    unsigned char dst_id;           // 1b
    unsigned char length;           // 1b
    uint8_t seq_num;                // 1b
    char is_first;                  // 1b
    char is_last;                   // 1b
    char data[FRAME_PAYLOAD_SIZE];
};
typedef struct Frame_t Frame;

struct Timed_frame_t {
    struct timeval timeout;
    Frame frame;
};

typedef struct Timed_frame_t Timed_frame;

#define MAX_CLIENTS 10
#define WINDOW_SIZE 8

// Receiver and sender data structures
struct Receiver_t {
    // DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_framelist_head;
    int recv_id;
    LLnode** ingoing_frames_head_ptr_map;
    Frame* frame_buffer[MAX_CLIENTS][UINT8_MAX];
    uint8_t LCA[MAX_CLIENTS];
};

struct Sender_t {
    // DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_cmdlist_head;
    LLnode* input_framelist_head;
    uint8_t send_id;

    LLnode* frame_buffer[MAX_CLIENTS];

    LLnode* timeout;
    uint8_t LAR[MAX_CLIENTS];
    uint8_t LFS[MAX_CLIENTS];
};

enum SendFrame_DstType { ReceiverDst, SenderDst } SendFrame_DstType;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;

// Declare global variables here
// DO NOT CHANGE:
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender* glb_senders_array;
Receiver* glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;

#endif
