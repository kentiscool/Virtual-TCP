#include "util.h"
#include <stdint.h>

// Linked list functions
int ll_get_length(LLnode* head) {
    LLnode* tmp;
    int count = 1;
    if (head == NULL)
        return 0;
    else {
        tmp = head->next;
        while (tmp != head) {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode** head_ptr, void* value) {
    LLnode* prev_last_node;
    LLnode* new_node;
    LLnode* head;

    if (head_ptr == NULL) {
        return;
    }

    // Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode*) malloc(sizeof(LLnode));
    new_node->value = (char*) value;
    // The list is empty, no node is currently present
    if (head == NULL) {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    } else {
        // Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}

LLnode* ll_pop_node(LLnode** head_ptr) {
    LLnode* last_node;
    LLnode* new_head;
    LLnode* prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL) {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    // We are about to set the head ptr to nothing because there is only one
    // thing in list
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

void ll_destroy_node(LLnode* node) {
    if (node->type == llt_string) {
        free(node->value);
    }
    free(node);
}

// Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval* start_time, struct timeval* finish_time) {
    long usec;
    usec = (finish_time->tv_sec - start_time->tv_sec) * 1000000;
    usec += (finish_time->tv_usec - start_time->tv_usec);
    return usec;
}

// Print out messages entered by the user
void print_cmd(Cmd* cmd) {
    fprintf(stderr, "src=%d, dst=%d, message=%s\n", cmd->src_id, cmd->dst_id,
            cmd->message);
}

char* convert_frame_to_char(Frame* frame) {
    char* char_buffer = malloc(sizeof(Frame));
    memcpy(char_buffer, frame, sizeof(Frame));
    return char_buffer;
}

Frame* convert_char_to_frame(char* char_buf) {
    Frame* frame = malloc(sizeof(Frame));
    memcpy(frame, char_buf, sizeof(Frame));
    return frame;
}

Frame* copy_frame(Frame* frame) {
    Frame* new_frame = malloc(sizeof(Frame));
    memcpy(new_frame, (char*) frame, sizeof(Frame));
    return (Frame*) new_frame;
}

int checksum(Frame* frame) {
    int sum = 0;
    char* data = convert_frame_to_char(frame);
    for (int i = 0; i < MAX_FRAME_SIZE - 8; i++) {
        sum += (int) data[i];
    }
    free(data);
    return sum;
}

bool within_window(uint8_t seq_num, uint8_t LAR) {
    if (seq_num == LAR) {
        return false;
    }
    if (seq_num >= LAR) {
        return seq_num - LAR < WINDOW_SIZE;
    } else {
        return (UINT8_MAX - LAR) + seq_num < WINDOW_SIZE;
    }
}

uint8_t next_seq(uint8_t seq_num) {
    if (seq_num == UINT8_MAX) {
        return 1;
    } else {
        return seq_num + 1;
    }
}

uint8_t prev_seq(uint8_t seq_num) {
    if (seq_num == 1) {
        return UINT8_MAX;
    } else {
        return seq_num - 1;
    }
}

uint8_t max_seq(uint8_t a, uint8_t b) {
    uint8_t diff = a > b ? a - b : b - a;
    if (diff > WINDOW_SIZE) {
        return a > b ? b : a;
    }
    return a > b ? a : b;
}

unsigned int compute_crc(Frame* frame) {
    const unsigned int polynomial = 0x04C11DB7; /* divisor is 32bit */
    unsigned int crc = 0;                       /* CRC value is 32bit */
    const data_size = MAX_FRAME_SIZE - 4;
    char bytes[data_size];

    memcpy(bytes, frame, sizeof(char) * data_size);

    // Byte Loop
    for (int i = 0; i < data_size; i++) {
        char b = bytes[i];
        crc ^= (unsigned int) (b << 24);
        // Bit Loop
        for (int i = 0; i < 8; i++) {
            if ((crc & 0x80000000) != 0) /* test for MSB = bit 31 */
            {
                crc = (unsigned int) ((crc << 1) ^ polynomial);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}
