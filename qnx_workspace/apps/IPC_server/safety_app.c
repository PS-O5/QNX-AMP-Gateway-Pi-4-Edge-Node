#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dispatch.h>
#include "asg_messages.h"

int main() {
    printf("[CLIENT] Safety Application Starting...\n");

    int server_coid;
    while ((server_coid = name_open(ASG_SERVER_NAME, 0)) == -1) {
        sleep(1);
    }
    printf("[CLIENT] Connected to ASG Server.\n");

    asg_msg_request_t req;
    asg_msg_reply_t   reply;
    req.msg_type = ASG_MSG_GET_TEMP;
    
    uint8_t current_cooling_state = 0; // Track state to avoid spamming the bus

    while (1) {
        if (MsgSend(server_coid, &req, sizeof(req), &reply, sizeof(reply)) == -1) break;

        if (reply.link_status == 1) {
            printf("[CLIENT] Temp: %.2f C | Drops: %d\n", reply.current_temp, reply.total_drops);
                   
            // Trigger Logic
            if (reply.current_temp > 28.0f && current_cooling_state == 0) {
                printf("[CLIENT] CRITICAL TEMP! Sending COOLING_ON command...\n");
                
                asg_msg_actuate_t act_req;
                act_req.msg_type = ASG_MSG_SET_COOLING;
                act_req.state = 1;
                MsgSend(server_coid, &act_req, sizeof(act_req), NULL, 0);
                
                current_cooling_state = 1;
            } 
            else if (reply.current_temp < 25.0f && current_cooling_state == 1) {
                printf("[CLIENT] Temp Normal. Sending COOLING_OFF command...\n");
                
                asg_msg_actuate_t act_req;
                act_req.msg_type = ASG_MSG_SET_COOLING;
                act_req.state = 0;
                MsgSend(server_coid, &act_req, sizeof(act_req), NULL, 0);
                
                current_cooling_state = 0;
            }
        }

        usleep(500000); // 2Hz polling
    }

    name_close(server_coid);
    return 0;
}
