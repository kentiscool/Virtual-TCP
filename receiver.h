#ifndef __RECEIVER_H__
#define __RECEIVER_H__

#include "common.h"
#include "communicate.h"
#include "util.h"
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

void init_receiver(Receiver*, int);
void* run_receiver(void*);
void clean_buffer(Receiver* receiver, int src_id, uint8_t last_seq_num);
int calc_LCA(Receiver* receiver, int src_id, uint8_t last_seq_num);
char* print_buffer(Receiver* receiver, int src_id, uint8_t last_seq_num);
#endif
