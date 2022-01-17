#include "receiver.h"

void init_receiver(Receiver* receiver, int id) {
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->ingoing_frames_head_ptr_map = malloc(MAX_CLIENTS * sizeof(LLnode *));
//    receiver->last_received_seq_num_map = (int*) malloc(MAX_CLIENTS * sizeof(int));

    for (int i = 0; i < MAX_CLIENTS; i++) {
        receiver->ingoing_frames_head_ptr_map[i] = NULL;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        receiver->last_received_seq_num_map[i] = -1;
    }

}

void handle_incoming_msgs(Receiver* receiver,
                          LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is for this receiver
    //    4) Acknowledge that this frame was received
    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

    while (incoming_msgs_length > 0) {
        // Pop a node off the front of the link list and update the count
        LLnode* ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

        char* raw_char_buf = ll_inmsg_node->value;

        Frame* ingoing_frame = convert_char_to_frame(raw_char_buf);

        free(ll_inmsg_node);
        free(raw_char_buf);

        if (ingoing_frame->dst_id != receiver->recv_id) {
            free(ingoing_frame);
            continue;
        }

        // Send ack
        Frame* ack = malloc(MAX_FRAME_SIZE);
        ack->seq_num = ingoing_frame->seq_num;
        ack->src_id = ingoing_frame->src_id;
        ack->dst_id = ingoing_frame->dst_id;
        ll_append_node(outgoing_frames_head_ptr, ack);

        if (receiver->last_received_seq_num_map[ingoing_frame->src_id] != ingoing_frame->seq_num) { // New Frame
            // Retrieve data and save to buffer
            ll_append_node(receiver->ingoing_frames_head_ptr_map + ingoing_frame->src_id, copy_frame(ingoing_frame));
            receiver->last_received_seq_num_map[ingoing_frame->src_id] = ingoing_frame->seq_num;
            if (ingoing_frame->is_last == 1) { // Once all frames are collected, print the message.
                int frames = ll_get_length(*(receiver->ingoing_frames_head_ptr_map + ingoing_frame->src_id));
                char* msg = calloc(frames * FRAME_PAYLOAD_SIZE + 1, sizeof(char));
                int char_idx = 0;

                while (frames > 0) {
                    LLnode* cur_node = ll_pop_node(receiver->ingoing_frames_head_ptr_map + ingoing_frame->src_id);
                    Frame * cur_frame = cur_node->value;
                    for (int i = 0; i < cur_frame->length; i++) {
                        msg[char_idx + i] = cur_frame->data[i];
                    }
                    char_idx += cur_frame->length;

                    ll_destroy_node(cur_node);
                    frames -= 1;
                }
                printf("<RECV_%d>:[%s]\n", receiver->recv_id, msg);
                free(msg);
            }
        } else {
//            printf("duplicate\n");
        }

        free(ingoing_frame);
    }
}

void* run_receiver(void* input_receiver) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver* receiver = (Receiver*) input_receiver;
    LLnode* outgoing_frames_head; // Chanel where all messages will be sent through

    // This incomplete receiver thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up if there is nothing
    // in the incoming queue(s)
    // 2. Grab the mutex protecting the input_msg queue
    // 3. Dequeues messages from the input_msg queue and prints them
    // 4. Releases the lock
    // 5. Sends out any outgoing messages

    while (1) {
        // NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, NULL);

        // Either timeout or get woken up because you've received a datagram
        // NOTE: You don't really need to do anything here, but it might be
        // useful for debugging purposes to have the receivers periodically
        // wakeup and print info
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        // Check whether anything arrived
        int incoming_msgs_length =
            ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0) {
            // Nothing has arrived, do a timed wait on the condition variable
            // (which releases the mutex). Again, you don't really need to do
            // the timed wait. A signal on the condition variable will wake up
            // the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv,
                                   &receiver->buffer_mutex, &time_spec);
        }

        handle_incoming_msgs(receiver, &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            // The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);
//            printf("sending ack\n");
            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
}
