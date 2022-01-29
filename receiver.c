#include "receiver.h"
#include <math.h>

void init_receiver(Receiver* receiver, int id) {
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->ingoing_frames_head_ptr_map = malloc(MAX_CLIENTS * sizeof(LLnode *));

    for (int i = 0; i < MAX_CLIENTS; i++) {
        receiver->ingoing_frames_head_ptr_map[i] = NULL;
        receiver->LCA[i] = 0;
        for (int j = 0; j < UINT8_MAX; j++) {
            receiver->frame_buffer[i][j] = NULL;
        }
    }
}

char* print_buffer(Receiver* receiver, int src_id, uint8_t last_seq_num) {
    int length = 0;
    int idx = last_seq_num;

    while (true) {
        length += receiver->frame_buffer[src_id][idx]->length;
        if (receiver->frame_buffer[src_id][idx]->is_first == 1) {
            break;
        }
        idx = prev_seq(idx);
    }

    char* msg = calloc(length, sizeof(char));
    int count = 0;
    while (true) {
        for (int i = 0; i < receiver->frame_buffer[src_id][idx]->length; i = next_seq(i)) {
            msg[count + i] = receiver->frame_buffer[src_id][idx]->data[i];
        }
        if (receiver->frame_buffer[src_id][idx]->is_last == 1) {
            break;
        }
        count += receiver->frame_buffer[src_id][idx]->length;
        idx = next_seq(idx);
    }
    return msg;
}

void clean_buffer(Receiver* receiver, int src_id, uint8_t last_seq_num) {
    while (true) {
        bool stop = receiver->frame_buffer[src_id][last_seq_num]->is_first == 1;
        free(receiver->frame_buffer[src_id][last_seq_num]);
        receiver->frame_buffer[src_id][last_seq_num] = NULL;
        if (stop) {
            return;
        }
        last_seq_num = prev_seq(last_seq_num);
    }
}

int calc_LCA(Receiver* receiver, int src_id, uint8_t last_seq_num) {
    uint8_t idx = next_seq(last_seq_num);

    while (true) {
        if (idx == 0) {
            idx++;
            continue;
        }

        if (receiver->frame_buffer[src_id][idx] == NULL) {
            return max_seq(prev_seq(idx), receiver->LCA[src_id]);
        } else if (receiver->frame_buffer[src_id][idx]->is_last == 1){
            return idx;
        }
        idx = next_seq(idx);
    }
}

void handle_incoming_msgs(Receiver* receiver,
                          LLnode** outgoing_frames_head_ptr) {
    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

    while (incoming_msgs_length > 0) {
        // Pop a node off the front of the link list and update the count
        LLnode* ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
        char* raw_char_buf = ll_inmsg_node->value;

        Frame* ingoing_frame = convert_char_to_frame(raw_char_buf);

        free(ll_inmsg_node);
        free(raw_char_buf);

        // Validate frame
        if (!(ingoing_frame->dst_id == receiver->recv_id && ingoing_frame->crc == compute_crc(ingoing_frame))) {
            free(ingoing_frame);
            continue;
        }

        if (within_window(ingoing_frame->seq_num, receiver->LCA[ingoing_frame->src_id])) {
            // Insert to buffer
            receiver->frame_buffer[ingoing_frame->src_id][ingoing_frame->seq_num] = malloc(sizeof(Frame));
            memcpy(receiver->frame_buffer[ingoing_frame->src_id][ingoing_frame->seq_num], ingoing_frame, sizeof(Frame));

            // Update LCA
            uint8_t new_LCA = calc_LCA(receiver, ingoing_frame->src_id, receiver->LCA[ingoing_frame->src_id]);
            receiver->LCA[ingoing_frame->src_id] = new_LCA;

            // Check if complete
            while (receiver->frame_buffer[ingoing_frame->src_id][new_LCA] != NULL && receiver->frame_buffer[ingoing_frame->src_id][new_LCA]->is_last == 1) {
                char* msg = print_buffer(receiver, ingoing_frame->src_id, receiver->LCA[ingoing_frame->src_id]);
                clean_buffer(receiver, ingoing_frame->src_id, receiver->LCA[ingoing_frame->src_id]);
                new_LCA = calc_LCA(receiver, ingoing_frame->src_id, receiver->LCA[ingoing_frame->src_id]);
                receiver->LCA[ingoing_frame->src_id] = new_LCA;
                printf("<RECV_%d>:[%s]\n", receiver->recv_id, msg);
                free(msg);
            }
        }

        // Send ack
        Frame* ack = malloc(sizeof(Frame));
        ack->seq_num = receiver->LCA[ingoing_frame->src_id];
        ack->src_id = ingoing_frame->src_id;
        ack->dst_id = ingoing_frame->dst_id;

        ll_append_node(outgoing_frames_head_ptr, ack);

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

        // Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = convert_frame_to_char(ll_outframe_node->value);

            // The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);
            Frame* frame = ll_outframe_node->value;
            ll_destroy_node(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
}
