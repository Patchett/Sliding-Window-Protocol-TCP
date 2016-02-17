#include "sender.h"

void init_sender(Sender *sender, int id)
{
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;
    sender->last_frame_sent = 255;
    sender->last_ack_received = 255;
    sender->acks_in_window = 0;
    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);
    sender->frame_buffer = (Frame **) malloc(MAX_FRAMES_IN_WINDOW * sizeof(Frame *));
    sender->timeout_times = (struct timeval **) malloc(MAX_FRAMES_IN_WINDOW * sizeof(struct timeval *));
    int i;
    for (i = 0; i < MAX_FRAMES_IN_WINDOW; i++) {
        (sender->frame_buffer)[i] = (Frame *)malloc(sizeof(Frame));
        (sender->timeout_times)[i] = (struct timeval *)malloc(sizeof(struct timeval));
    }
}

struct timeval *sender_get_next_expiring_timeval(Sender *sender)
{
    int i;
    struct timeval *next_timeout = NULL;
    for (i = 0; i < MAX_FRAMES_IN_WINDOW; i++) {
        if (wrapped_subtract(sender->last_frame_sent, sender->last_ack_received) <= i) {
            break;
        }
        if ((sender->acks_in_window & (0x80 >> i)) == 0) {
            if (NULL == next_timeout || timeval_usecdiff(sender->timeout_times[i], next_timeout) > 0) {
                next_timeout = (sender->timeout_times[i]);
            }
        }
    }
    return next_timeout;
}

void handle_incoming_acks(Sender *sender,
                          LLnode **outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this sender
    //    5) Do sliding window protocol for sender/receiver pair
    while (ll_get_length(sender->input_framelist_head) > 0) {
        LLnode *inc_msg = ll_pop_node(&sender->input_framelist_head);
        Frame *ack_frame = convert_char_to_frame((char *)inc_msg->value);
        if (is_corrupted(ack_frame) == 1) {
            fprintf(stderr, "Incoming ack was corrupted at sender.\n");
            free(ack_frame);
            free(inc_msg);
            ack_frame = NULL;
            continue;
        }
        // Make sure Ack is for this sender
        if ((sender->send_id != ack_frame->recv_id) ||
                (wrapped_subtract(ack_frame->SeqNum, sender->last_ack_received + 1) > MAX_FRAMES_IN_WINDOW ||
                 wrapped_subtract(sender->last_frame_sent, ack_frame->SeqNum) > MAX_FRAMES_IN_WINDOW)) {
            fprintf(stderr, "Ack %d was not for this sender or was out of range of the window.\n", ack_frame->SeqNum);
            free(ack_frame);
            free(inc_msg);
            ack_frame = NULL;
            continue;
        }
        fprintf(stderr, "Incoming_Ack - Ack_Num: %d LAR: %d LFS: %d msg: %s\n", ack_frame->SeqNum, sender->last_ack_received, sender->last_frame_sent, ack_frame->data);
        if (wrapped_subtract(ack_frame->SeqNum, sender->last_ack_received) == 1) {
            sender->acks_in_window <<= 1;
            (sender->last_ack_received)++;
            int i;
            Frame *tmp_buffer = sender->frame_buffer[0];
            struct timeval *tmp_time = sender->timeout_times[0];
            for (i = 1; i < MAX_FRAMES_IN_WINDOW; i++) {
                sender->timeout_times[i - 1] = sender->timeout_times[i];
                sender->frame_buffer[i - 1] = sender->frame_buffer[i];
            }
            sender->frame_buffer[MAX_FRAMES_IN_WINDOW - 1] = tmp_buffer;
            sender->timeout_times[MAX_FRAMES_IN_WINDOW - 1] = tmp_time;
            for (i = 0; i < MAX_FRAMES_IN_WINDOW; i++) {
                fprintf(stderr, "sender->frame_buffer[%d]: %p\n", i, (sender->frame_buffer)[i]);
            }
            fprintf(stderr, "Sender %d successfully received Ack %d from Receiver %d\n",
                    sender->send_id, sender->last_ack_received, ack_frame->send_id);
            while (0x80 == (sender->acks_in_window & 0x80)) {
                sender->acks_in_window <<= 1;
                (sender->last_ack_received)++;
                int i;
                Frame *tmp_buffer = sender->frame_buffer[0];
                struct timeval *tmp_time = sender->timeout_times[0];
                for (i = 1; i < MAX_FRAMES_IN_WINDOW; i++) {
                    sender->timeout_times[i - 1] = sender->timeout_times[i];
                    sender->frame_buffer[i - 1] = sender->frame_buffer[i];
                }
                sender->frame_buffer[MAX_FRAMES_IN_WINDOW - 1] = tmp_buffer;
                sender->timeout_times[MAX_FRAMES_IN_WINDOW - 1] = tmp_time;
                for (i = 0; i < MAX_FRAMES_IN_WINDOW; i++) {
                    fprintf(stderr, "sender->frame_buffer[%d]: %p\n", i, (sender->frame_buffer)[i]);
                }
                fprintf(stderr, "Sender %d successfully received Ack %d from Receiver %d\n",
                        sender->send_id, sender->last_ack_received, ack_frame->send_id);
            }
        } else if (wrapped_subtract(ack_frame->SeqNum, sender->last_ack_received) <= MAX_FRAMES_IN_WINDOW) {
            uchar_t frame_number = wrapped_subtract(ack_frame->SeqNum, sender->last_ack_received);
            sender->acks_in_window |= (1 << (MAX_FRAMES_IN_WINDOW - frame_number));
            fprintf(stderr, "Ack %d came in out of order. Inserting it into position %d\n",
                    ack_frame->SeqNum, (MAX_FRAMES_IN_WINDOW - frame_number));
        }
        free(inc_msg);
        free(ack_frame);
    }
}
void handle_input_cmds(Sender *sender,
                       LLnode **outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the sliding wstructindow protocol
    //    4) Compute CRC and add CRC to Frame

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);


    //Recheck the command queue length to see if stdin_thread dumped a command on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    while (input_cmd_length > 0) {
        if (ll_get_length(*outgoing_frames_head_ptr) >= MAX_FRAMES_IN_WINDOW ||
                wrapped_subtract(sender->last_frame_sent, sender->last_ack_received) >= MAX_FRAMES_IN_WINDOW) {
            //fprintf(stderr, "Sender %d cannot send anymore frames right now, waiting...\n", sender->send_id);
            break;
        }
        //Pop a node off and update the input_cmd_length
        LLnode *ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        //Cast to Cmd type and free up the memory for the node
        Cmd *outgoing_cmd = (Cmd *) ll_input_cmd_node->value;
        // Make sure we are sending a frame that belongs to this sender
        free(ll_input_cmd_node);
        if (outgoing_cmd->src_id != sender->send_id) {
            free(outgoing_cmd);
            continue;
        }

        int msg_length = strlen(outgoing_cmd->message);
        bool frame_was_too_big = false;
        char *adjusted_message = (char *) malloc(sizeof(char) * FRAME_PAYLOAD_SIZE);
        memset(adjusted_message, 0, FRAME_PAYLOAD_SIZE);
        if (msg_length > FRAME_PAYLOAD_SIZE - 1) {
            //Do something about messages that exceed the frame size
            memcpy(adjusted_message, outgoing_cmd->message, FRAME_PAYLOAD_SIZE - 1);
            adjusted_message[FRAME_PAYLOAD_SIZE - 1] = '\0';
            outgoing_cmd->message += FRAME_PAYLOAD_SIZE - 1;
            Cmd *remaining_cmd = (Cmd *) malloc(sizeof(Cmd));
            char *rest_of_message = (char *) malloc(sizeof(char) * (strlen(outgoing_cmd->message) + 1));
            remaining_cmd->src_id = outgoing_cmd->src_id;
            remaining_cmd->dst_id = outgoing_cmd->dst_id;
            strcpy(rest_of_message, outgoing_cmd->message);
            remaining_cmd->message = rest_of_message;
            ll_insert_node_at_front(&sender->input_cmdlist_head, (void *) remaining_cmd);
            frame_was_too_big = true;
        }
        if (false == frame_was_too_big) {
            memcpy(adjusted_message, outgoing_cmd->message, strlen(outgoing_cmd->message));
        }
        //This is probably ONLY one step you want
        Frame *outgoing_frame = (Frame *) malloc(sizeof(Frame));
        memset(outgoing_frame, 0, sizeof(Frame));
        outgoing_frame->send_id = outgoing_cmd->src_id;
        outgoing_frame->recv_id = outgoing_cmd->dst_id;
        if (false == frame_was_too_big) {
            memcpy(outgoing_frame->data, adjusted_message, strlen(outgoing_cmd->message));
        } else {
            memcpy(outgoing_frame->data, adjusted_message, FRAME_PAYLOAD_SIZE);
        }
        outgoing_frame->SeqNum = ++(sender->last_frame_sent);
        append_crc(outgoing_frame);
        uchar_t pos = wrapped_subtract(sender->last_frame_sent, sender->last_ack_received) - 1;
        memcpy((sender->frame_buffer)[pos], outgoing_frame, MAX_FRAME_SIZE);
        append_crc((sender->frame_buffer)[pos]);
        calculate_timeout(sender->timeout_times[pos]);
        //At this point, we don't need the outgoing_cmd
        free(outgoing_cmd->message);
        free(outgoing_cmd);
        fprintf(stderr, "Now sending frame %d with address %p and message %s\n",
                outgoing_frame->SeqNum, (sender->frame_buffer)[pos], (sender->frame_buffer)[pos]->data);
        //Convert the message to the outgoing_charbuf
        char *outgoing_charbuf = convert_frame_to_char((Frame *)(sender->frame_buffer)[pos]);
        ll_append_node(outgoing_frames_head_ptr,
                       outgoing_charbuf);
        free(outgoing_frame);
        free(adjusted_message);
    }
}



void handle_timedout_frames(Sender *sender,
                            LLnode **outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling timed out datagrams
    //    1) Iterate through the sliding window protocol information you maintain for each receiver
    //    2) Locate frames that are timed out and add them to the outgoing frames
    //    3) Update the next timeout field on the outgoing frames
    struct timeval curr_timeval;
    int i;
    // Make sure that at least 1 frame has been sent

    for (i = 0; i < MAX_FRAMES_IN_WINDOW; i++) {
        if (wrapped_subtract(sender->last_frame_sent, sender->last_ack_received) <= i) {
            // fprintf(stderr, "Broke out of handle_timedout_frames with sender->last_frame_sent: %d sender->last_ack_received: %d i: %d\n",
            //         sender->last_frame_sent, sender->last_ack_received, i);
            break;
        }
        // Check if we have already received an ack for the frame we are examining.
        if ((sender->acks_in_window & (0x80 >> i)) == 0) {
            // Check if the frame has been in flight for more than 0.1 sec
            gettimeofday(&curr_timeval, NULL);
            // fprintf(stderr, "In handle_timedout_frames on frame %d, time until timeout is %ld sender->acks_in_window is %X i is %d address is %p\n",
            //         (sender->frame_buffer[i])->SeqNum, timeval_usecdiff((sender->timeout_times[i]), &curr_timeval),
            //         sender->acks_in_window, i, (sender->frame_buffer)[i]);
            // fprintf(stderr, "curr_tv.tv_sec: %ld curr_tv.tv_usec: %ld Frame %d tv_sec %ld tv_usec %ld\n",
            //         curr_timeval.tv_sec, curr_timeval.tv_usec, sender->frame_buffer[i]->SeqNum,
            //         (sender->timeout_times[i])->tv_sec, (sender->timeout_times[i])->tv_usec);
            if (timeval_usecdiff((sender->timeout_times[i]), &curr_timeval) >= 0) {
                // update current time on outgoing frame and resend frame
                calculate_timeout(sender->timeout_times[i]);
                char *resend_frame_buffer = convert_frame_to_char((Frame *)sender->frame_buffer[i]);
                fprintf(stderr, "Frame %d timed out! Resending...\n", (sender->frame_buffer[i])->SeqNum);
                ll_append_node(outgoing_frames_head_ptr, resend_frame_buffer);
            }
        }
    }
}

void *run_sender(void *input_sender)
{
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender *sender = (Sender *) input_sender;
    LLnode *outgoing_frames_head;
    struct timeval *expiring_timeval;
    long sleep_usec_time, sleep_sec_time;

    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages


    while (1) {
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval,
                     NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        //Perform full on timeout
        if (expiring_timeval == NULL) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        } else {
            //Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            //Sleep if the difference is positive
            if (sleep_usec_time > 0) {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        //Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }


        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames or input commands should go
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        //Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        //Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        //A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
                inframe_queue_length == 0) {

            pthread_cond_timedwait(&sender->buffer_cv,
                                   &sender->buffer_mutex,
                                   &time_spec);
        }
        //Implement this
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        //Implement this
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);


        //Implement this
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);

        while (ll_outgoing_frame_length > 0) {
            LLnode *ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char *char_buf = (char *)  ll_outframe_node->value;

            //Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
