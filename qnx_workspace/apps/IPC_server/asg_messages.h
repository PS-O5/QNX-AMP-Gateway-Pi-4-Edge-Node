#ifndef ASG_MESSAGES_H
#define ASG_MESSAGES_H

#include <sys/iomsg.h>
#include <stdint.h>

#define ASG_SERVER_NAME "asg_sensor_node"

// IPC Message Types
#define ASG_MSG_GET_TEMP    (_IO_MAX + 1)
#define ASG_MSG_SET_COOLING (_IO_MAX + 2)

// --- Structs for GET_TEMP ---
typedef struct {
    uint16_t msg_type;
} asg_msg_request_t;

typedef struct {
    float   current_temp;
    uint8_t link_status; 
    uint8_t total_drops;
} asg_msg_reply_t;

// --- Structs for SET_COOLING ---
typedef struct {
    uint16_t msg_type;
    uint8_t  state; // 1 = ON, 0 = OFF
} asg_msg_actuate_t;

#endif
