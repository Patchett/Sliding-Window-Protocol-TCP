#ifndef __UTIL_H__
#define __UTIL_H__

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
#include "common.h"

//Linked list functions
int ll_get_length(LLnode *);
void ll_append_node(LLnode **, void *);
LLnode *ll_pop_node(LLnode **);
void ll_destroy_node(LLnode *);

//Print functions
void print_cmd(Cmd *);

//Time functions
long timeval_usecdiff(struct timeval *,
                      struct timeval *);

//TODO: Implement these functions
uchar_t wrapped_subtract(uchar_t, uchar_t);
void receive_frame(Receiver *, Frame *);
void send_ack(Receiver *, Frame *, LLnode **);
char *convert_frame_to_char(Frame *);
Frame *convert_char_to_frame(char *);
long time_delta(struct timeval *, struct timeval *);
uchar_t is_corrupted(Frame *);
uint32_t crc(char *, int);
uint32_t append_crc(Frame *);
void calculate_timeout(struct timeval *);
void ll_insert_node_at_front(LLnode **, void *);



#endif
