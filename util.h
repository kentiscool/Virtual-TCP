#ifndef __UTIL_H__
#define __UTIL_H__

#include "common.h"
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

// Linked list functions
int ll_get_length(LLnode*);
void ll_append_node(LLnode**, void*);
LLnode* ll_pop_node(LLnode**);
void ll_destroy_node(LLnode*);

// Print functions
void print_cmd(Cmd*);

// Time functions
long timeval_usecdiff(struct timeval*, struct timeval*);

char* convert_frame_to_char(Frame*);
Frame* convert_char_to_frame(char*);

int checksum(Frame* frame);

Frame* copy_frame(Frame* frame);

bool within_window(uint8_t seq_num, uint8_t LAR);

uint8_t next_seq(uint8_t seq_num);
uint8_t prev_seq(uint8_t seq_num);
uint8_t max_seq(uint8_t a, uint8_t b);

unsigned int compute_crc(Frame* frame);
#endif
