#include "util.h"
#include <assert.h>

//Linked list functions
int ll_get_length(LLnode *head)
{
    LLnode *tmp;
    int count = 1;
    if (head == NULL) {
        return 0;
    } else {
        tmp = head->next;
        while (tmp != head) {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode **head_ptr,
                    void *value)
{
    LLnode *prev_last_node;
    LLnode *new_node;
    LLnode *head;

    if (head_ptr == NULL) {
        return;
    }

    //Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode *) malloc(sizeof(LLnode));
    new_node->value = value;

    //The list is empty, no node is currently present
    if (head == NULL) {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    } else {
        //Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}

LLnode *ll_pop_node(LLnode **head_ptr)
{
    LLnode *last_node;
    LLnode *new_head;
    LLnode *prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL) {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    //We are about to set the head ptr to nothing because there is only one thing in list
    if (last_node == prev_head) {
        (*head_ptr) = NULL;
        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    } else {
        (*head_ptr) = new_head;
        last_node->next = new_head;
        new_head->prev = last_node;

        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
}

void ll_destroy_node(LLnode *node)
{
    if (node->type == llt_string) {
        free((char *) node->value);
    }
    free(node);
}

//Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval *start_time,
                      struct timeval *finish_time)
{
    long usec;
    usec = (finish_time->tv_sec - start_time->tv_sec) * 1000000;
    usec += (finish_time->tv_usec - start_time->tv_usec);
    return usec;
}

//Print out messages entered by the user
void print_cmd(Cmd *cmd)
{
    fprintf(stderr, "src=%d, dst=%d, message=%s\n",
            cmd->src_id,
            cmd->dst_id,
            cmd->message);
}

char *convert_frame_to_char(Frame *frame)
{
    char *char_buffer = (char *) malloc(MAX_FRAME_SIZE);
    char *write_head = char_buffer;
    memset(char_buffer, 0, MAX_FRAME_SIZE);
    memcpy(write_head, &(frame->SeqNum), sizeof(uchar_t));
    write_head += sizeof(uchar_t);
    memcpy(write_head, &(frame->recv_id), sizeof(uint16_t));
    write_head += sizeof(uint16_t);
    memcpy(write_head, &(frame->send_id), sizeof(uint16_t));
    write_head += sizeof(uint16_t);
    memcpy(write_head, &(frame->data), FRAME_PAYLOAD_SIZE);
    write_head += FRAME_PAYLOAD_SIZE;
    memcpy(write_head, &(frame->crc), sizeof(uint32_t));
    write_head += sizeof(uint32_t);
    // Ensure that the full frame of the exact size was written. Prevents overflows
    assert(MAX_FRAME_SIZE == write_head - char_buffer);
    return char_buffer;
}

Frame *convert_char_to_frame(char *char_buf)
{
    char *write_head = char_buf;
    Frame *frame = (Frame *) malloc(sizeof(Frame));
    memset(frame->data, 0, sizeof(char)*sizeof(frame->data));
    memcpy(&(frame->SeqNum), write_head, sizeof(frame->SeqNum));
    write_head += sizeof(frame->SeqNum);
    memcpy(&(frame->recv_id), write_head, sizeof(frame->recv_id));
    write_head += sizeof(frame->recv_id);
    memcpy(&(frame->send_id), write_head, sizeof(frame->send_id));
    write_head += sizeof(frame->send_id);
    memcpy(&(frame->data), write_head, FRAME_PAYLOAD_SIZE);
    write_head += FRAME_PAYLOAD_SIZE;
    memcpy(&(frame->crc), write_head, sizeof(uint32_t));
    write_head += sizeof(uint32_t);
    // Ensure that the full frame of the exact size was read. Prevents overflows
    assert(MAX_FRAME_SIZE == write_head - char_buf);
    return frame;
}

uchar_t wrapped_subtract(uchar_t x, uchar_t y)
{
    if (x < y) {
        return 256 + x - y;
    } else {
        return x - y;
    }
}

void send_ack(Receiver *receiver, Frame *inframe, LLnode **outgoing_frames_head_ptr)
{
    Frame *ack_frame = (Frame *) malloc(sizeof(Frame));
    memset(ack_frame, 0, sizeof(Frame));
    ack_frame->SeqNum = inframe->SeqNum;
    ack_frame->recv_id = inframe->send_id;
    ack_frame->send_id = receiver->recv_id;
    ack_frame->crc = inframe->crc;
    memcpy(ack_frame->data, inframe->data, strlen(inframe->data));
    assert(ack_frame->crc == inframe->crc);
    ll_append_node(outgoing_frames_head_ptr, convert_frame_to_char(ack_frame));
    fprintf(stderr, "Receiver %d successfully sent Ack %d to Sender %d\n",
            ack_frame->send_id, ack_frame->SeqNum, ack_frame->recv_id);
    free(ack_frame);
}

void ll_insert_node_at_front(LLnode **head_ptr, void *value)
{
    ll_append_node(head_ptr, value);
    *head_ptr = ((*head_ptr)->prev);
}


uint32_t crc(char *array, int array_len)
{
    uint32_t crc = 0xFFFFFFFF;
    int i, j;
    for (i = 0; i < array_len; i++) {
        crc ^= (uint32_t)array[i];
        for (j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (crc & 1) * CRC_POLY;
        }
    }
    return crc;
}

uchar_t is_corrupted(Frame *frame)
{
    char *char_buf = convert_frame_to_char(frame);
    uchar_t ret_val = 1;
    if (crc(char_buf, MAX_FRAME_SIZE - sizeof(uint32_t)) == frame->crc) {
        ret_val = 0;
    }
    free(char_buf);
    return ret_val;
}

uint32_t append_crc(Frame *frame)
{
    char *charbuf_before_crc = convert_frame_to_char(frame);
    frame->crc = crc(charbuf_before_crc, MAX_FRAME_SIZE - sizeof(uint32_t));
    free(charbuf_before_crc);
    return (uint32_t)frame->crc;
}

void calculate_timeout(struct timeval *timeout)
{
    gettimeofday(timeout, NULL); //use this to get the current time
    timeout->tv_usec += 100000; //0.1s
    if (timeout->tv_usec >= 1000000) {
        timeout->tv_usec -= 1000000; //1s
        timeout->tv_sec += 1;
    }
}
