#include "sender.h"
#include <stdbool.h>

#include <assert.h>

void init_sender(Sender* sender, int id) {
    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;

    sender->frame_buffer_head = NULL;
    sender->last_sent_frame = NULL;
    gettimeofday(&sender->timeout, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        sender->last_sent_seq_num_map[i] = 0;
    }

    sender->acknowledged = true;
}

struct timeval* sender_get_next_expiring_timeval(Sender* sender) {
    // TODO: You should fill in this function so that it returns the next
    // timeout that should occur
    if (sender->last_sent_frame == NULL) {
        return NULL;
    }
    return &sender->timeout;
}

void handle_incoming_acks(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    int input_length = ll_get_length(sender->input_framelist_head);
    if (input_length == 0) {
        return;
    }

    // Retrieve ack from input_framelist_head
    LLnode* input_node = ll_pop_node(&sender->input_framelist_head);
    Frame* ack = input_node->value; // ack will be free'd via ll_destroy_node(input_node)
    if (ack->src_id == sender->send_id &&
        !sender->acknowledged &&
        ack->seq_num == sender->last_sent_frame->seq_num &&
        checksum(ack) == ack->checksum) {
        int buffer_length = ll_get_length(sender->frame_buffer_head);
        if (buffer_length > 0) { // Send next frame
            if (sender->last_sent_frame != NULL) {
                free(sender->last_sent_frame);
            }
            LLnode* new_frame_node = ll_pop_node(&sender->frame_buffer_head);
            Frame* new_frame = new_frame_node->value;
            ll_append_node(outgoing_frames_head_ptr, copy_frame(new_frame));
            ll_destroy_node(new_frame_node);
            sender->acknowledged = false;
        } else {
            sender->acknowledged = true;
            sender->last_sent_frame = NULL;
        }

    }

    ll_destroy_node(input_node);
}

void handle_input_cmds(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    while (input_cmd_length > 0) {
        // Pop a node off and update the input_cmd_length
        LLnode* ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        // Cast to Cmd type and free up the memory for the node
        Cmd* outgoing_cmd = (Cmd*) ll_input_cmd_node->value;
        free(ll_input_cmd_node);

        // Ignore if message src is wrong
        if (outgoing_cmd->src_id != sender->send_id) {
            free(outgoing_cmd->message);
            free(outgoing_cmd);
            continue;
        }

        int msg_length = strlen(outgoing_cmd->message);
        int remaining = msg_length;

        while (remaining > 0) {
            Frame* outgoing_frame = malloc(MAX_FRAME_SIZE);
            int idx = msg_length - remaining;

            outgoing_frame->src_id = outgoing_cmd->src_id;
            outgoing_frame->dst_id = outgoing_cmd->dst_id;
            outgoing_frame->seq_num = sender->last_sent_seq_num_map[outgoing_frame->dst_id] % 2;

            // Update count
            sender->last_sent_seq_num_map[outgoing_frame->dst_id]++;

            // Determine if last frame
            if (remaining > FRAME_PAYLOAD_SIZE) {
                outgoing_frame->is_last = 0;
                outgoing_frame->length = FRAME_PAYLOAD_SIZE;
            } else {
                outgoing_frame->is_last = 1;
                outgoing_frame->length = remaining ; // +1 for null terminator
            }
            remaining -= outgoing_frame->length;

            // Copy data
            for (int i = 0; i < MAX_FRAME_SIZE; i++) {
                if (i < outgoing_frame->length) {
                    outgoing_frame->data[i] = outgoing_cmd->message[i + idx];
                } else {
                    outgoing_frame->data[i] = 0;
                }
            }

            outgoing_frame->checksum = checksum(outgoing_frame);

            int buffer_length = ll_get_length(sender->frame_buffer_head);
            if (sender->acknowledged) {
                ll_append_node(outgoing_frames_head_ptr, copy_frame(outgoing_frame));
                sender->last_sent_frame = copy_frame(outgoing_frame);
                free(outgoing_frame);
            } else {
                ll_append_node(&sender->frame_buffer_head, outgoing_frame);
            }
            sender->acknowledged = false;
        }

        free(outgoing_cmd->message);
        free(outgoing_cmd);
    }
}

void handle_timedout_frames(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Handle timeout by resending the appropriate message
    ll_append_node(outgoing_frames_head_ptr, copy_frame(sender->last_sent_frame));
}

void* run_sender(void* input_sender) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender* sender = (Sender*) input_sender;
    LLnode** outgoing_frames_head;
    struct timeval* expiring_timeval;
    long sleep_usec_time, sleep_sec_time;
    outgoing_frames_head = NULL;

    while (1) {

        // Get the current time
        gettimeofday(&curr_timeval, NULL);

        // time_spec is a data structure used to specify when the thread should
        // wake up The time is specified as an ABSOLUTE (meaning, conceptually,
        // you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        // Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        // Perform full on timeout
        if (expiring_timeval == NULL) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        } else {
            // Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval, expiring_timeval);

            // Sleep if the difference is positive
            if (sleep_usec_time > 0) {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        // Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames or input
        // commands should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        // Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        // Nothing (cmd nor incoming frame) has arrived, so do a timed wait on
        // the sender's condition variable (releases lock) A signal on the
        // condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 && inframe_queue_length == 0) {
            pthread_cond_timedwait(&sender->buffer_cv, &sender->buffer_mutex,
                                   &time_spec);
        }

        handle_incoming_acks(sender, &outgoing_frames_head);

        handle_input_cmds(sender, &outgoing_frames_head);

        if (expiring_timeval != NULL) {
            long time_diff_sec = timeval_usecdiff(&curr_timeval, expiring_timeval);
            if (time_diff_sec <= 0 && sender->last_sent_frame != NULL && ll_get_length(outgoing_frames_head) == 0) {
                handle_timedout_frames(sender, &outgoing_frames_head);
            }
        }

        pthread_mutex_unlock(&sender->buffer_mutex);

        // Handle timeout


        // Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            struct timeval next_time_out = curr_timeval;
            next_time_out.tv_sec = (next_time_out.tv_usec + 90000) / 1000000 + next_time_out.tv_sec;
            next_time_out.tv_usec = (next_time_out.tv_usec + 90000) % 1000000;
            sender->timeout = next_time_out;
            sender->last_sent_frame = copy_frame(ll_outframe_node->value);
            Frame* f = ll_outframe_node->value;

            send_msg_to_receivers(convert_frame_to_char(ll_outframe_node->value));

            ll_destroy_node(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
