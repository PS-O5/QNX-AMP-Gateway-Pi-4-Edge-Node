#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/dispatch.h>
#include "asg_messages.h"

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
float   global_temp = 0.0f;
uint8_t global_drops = 0;
uint8_t global_link_status = 0;
int     uart_fd = -1; // Shared File Descriptor

#pragma pack(push, 1)
typedef struct {
    uint16_t header;
    uint8_t  seq;
    float    temp;
    uint16_t crc;
} __attribute__((packed)) SensorPacket;
#pragma pack(pop)

uint16_t calculate_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void* uart_ingest_thread(void* arg) {
    printf("[SERVER] Hardware ingest thread active on /dev/ser2\n");
    uint8_t byte_in, last_byte = 0x00, expected_seq = 0;

    while (1) {
        if (read(uart_fd, &byte_in, 1) > 0) {
            if (last_byte == 0xAA && byte_in == 0x55) {
                SensorPacket pkt;
                pkt.header = 0x55AA;
                
                int bytes_read = 0;
                uint8_t *ptr = (uint8_t*)&pkt.seq;
                while (bytes_read < 7) {
                    int n = read(uart_fd, ptr + bytes_read, 7 - bytes_read);
                    if (n > 0) bytes_read += n;
                }

                if (calculate_crc16((uint8_t*)&pkt.seq, 5) == pkt.crc) {
                    pthread_mutex_lock(&data_mutex);
                    global_temp = pkt.temp;
                    global_drops += (uint8_t)(pkt.seq - expected_seq);
                    global_link_status = 1;
                    pthread_mutex_unlock(&data_mutex);
                    expected_seq = pkt.seq + 1;
                }
                last_byte = 0x00;
            } else {
                last_byte = byte_in;
            }
        }
    }
    return NULL;
}

int main() {
    // Open UART as READ/WRITE for bidirectional comms
    uart_fd = open("/dev/ser2", O_RDWR | O_NOCTTY);
    if (uart_fd < 0) { perror("Port Error"); return 1; }

    struct termios cfg;
    tcgetattr(uart_fd, &cfg);
    cfsetspeed(&cfg, B115200);
    cfg.c_cflag |= (CLOCAL | CREAD | CS8);
    cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tcsetattr(uart_fd, TCSANOW, &cfg);

    pthread_t ingest_tid;
    pthread_create(&ingest_tid, NULL, uart_ingest_thread, NULL);

    name_attach_t *attach = name_attach(NULL, ASG_SERVER_NAME, 0);
    if (attach == NULL) { perror("name_attach failed"); return EXIT_FAILURE; }
    printf("[SERVER] IPC Server attached as '%s'\n", ASG_SERVER_NAME);

    int rcvid;
    union {
        uint16_t type;
        asg_msg_request_t get_req;
        asg_msg_actuate_t set_req;
    } msg_in;

    while (1) {
        rcvid = MsgReceive(attach->chid, &msg_in, sizeof(msg_in), NULL);
        if (rcvid <= 0) continue; 

        if (msg_in.type == ASG_MSG_GET_TEMP) {
            asg_msg_reply_t reply;
            pthread_mutex_lock(&data_mutex);
            reply.current_temp = global_temp;
            reply.total_drops = global_drops;
            reply.link_status = global_link_status;
            pthread_mutex_unlock(&data_mutex);
            MsgReply(rcvid, 0, &reply, sizeof(reply));
        } 
        else if (msg_in.type == ASG_MSG_SET_COOLING) {
            printf("[SERVER] IPC Rx -> Actuate Cooling: %d\n", msg_in.set_req.state);
            
            // Construct and blast the binary command to the STM32
            uint8_t cmd_pkt[3] = {0xBB, 0x66, msg_in.set_req.state};
            write(uart_fd, cmd_pkt, 3);
            
            MsgReply(rcvid, 0, NULL, 0); // Acknowledge IPC Client
        } else {
            MsgError(rcvid, ENOSYS);
        }
    }
    return 0;
}
