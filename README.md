# Framing and Retransmission

### Introduction
This is an implementation of the __Data Link Layer protocol__ that facilitates communication between 
multiple hosts(threads). Each host can communicate with up to 256 other hosts. A host can only act as 
either a __sender__ or __receiver__. Tolerant against dropped and corrupted frames. A Sender will keep 
on sending the same frame every 0.01 seconds until an acknowledgement is received. A Receiver will only send acknowledgements
if the received frame passes a checksum even if it's a duplicate.

```
Transaction between Sender and Receiver

Sender 1 
         \ Frame 0
          \
           \
             Reciever 1
           /
          /
         / Ack 1
Sender 1  
         \ Frame 1
          \
           \
             Reciever 1
           /
          /
         / Ack 1
Sender 1             
```
***
### Framing
Each message will be divided to frames of size 64 bytes. 
```
===============================================================================
|  src_id  |  dst_id  |  length  |  seq_num  |  is_last  |  data  | checksum  |
-------------------------------------------------------------------------------
|  1 Byte  |  1 Byte  |  1 Byte  |  1 Byte   |  1 Byte   |51 Bytes|  8 Byte   | 
===============================================================================

struct Frame {
    unsigned char src_id;           // 1 Byte
    unsigned char dst_id;           // 1 Byte
    unsigned char length;           // 1 Byte
    unsigned char seq_num;          // 1 Byte
    char is_last;                   // 1 Byte
    char data[FRAME_PAYLOAD_SIZE];  // 51 Bytes
    int checksum;                   // 8 Bytes
};

```
___
## Sender

### Fields
- output_buffer:
  - Contains all frames that will be sent
- frame_buffer:
  - Frame buffer for each receiver it is communicating to.
- seq_map:
  - Keeps track of sequence number for each receiver it is communicating to
- last_sent_frame:
  - Keeps track of the last sent frame for retransmission
___
### Functions
___handle_input_cmds___
- pops all messages from the cmd buffer
- checks id
- Divides the message into frames and sends them 1 by 1 after setting:
  - src_id
  - dst_id
  - length
  - seq_num: 
    - Both sender and receiver keeps track of the current sequence number of all hosts its communication with.
  - is_last:
    - Flag to denote the message is completely sent.
  - data
  - checksum

___handle_incoming_acks___
- Sender only sends a frame if:
  - Pops frame from frame_buffer to output_buffer after receiving appropriate acknowledgement.
  - Must pass checksum and have corresponding src_id.
___
## Receiver
### Fields
- input_buffer:
  - Contains all frames that will be received.
- ingoing_buffer:
  - A map of buffers from each sender of incomplete messages.
- seq_map:
  - A map of most recent sequence numbers from each sender.
___
### Functions
___handle_incoming_msgs___
- pops all messages from the input_buffer and inserts into ingoing_buffer if appropriate.
- If is_last frame is true => pop all frames from the appropriate ingoing_buffer and print to stdout.
- Must pass checksum and have corresponding dst_id.
___
### Utility functions
___checksum___
- Computed everytime a frame is sent.
- Computed by calculating the number of 1s in the frame.

___copy_frame___
- Allocates a duplicate frame in memory and returns a pointer pointing to ir.

