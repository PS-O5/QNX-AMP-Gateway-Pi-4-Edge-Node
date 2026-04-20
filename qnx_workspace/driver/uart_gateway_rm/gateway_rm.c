#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

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

int main() {
    int fd = open("/dev/ser2", O_RDONLY | O_NOCTTY);
    if (fd < 0) { perror("Port Error"); return 1; }

    struct termios cfg;
    tcgetattr(fd, &cfg);
    cfsetspeed(&cfg, B115200);
    cfg.c_cflag |= (CLOCAL | CREAD | CS8);
    cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tcsetattr(fd, TCSANOW, &cfg);

    printf("ASG: Binary Payload Ingest Active. Hunting for packets...\n");

    uint8_t byte_in;
    uint8_t last_byte = 0x00;
    uint8_t expected_seq = 0;

    while (1) {
        if (read(fd, &byte_in, 1) > 0) {
            // State Machine: Hunt for 0xAA followed by 0x55
            if (last_byte == 0xAA && byte_in == 0x55) {
                SensorPacket pkt;
                pkt.header = 0x55AA; // We just found it
                
                // Read the remaining 7 bytes (seq + temp + crc)
                int bytes_read = 0;
                uint8_t *ptr = (uint8_t*)&pkt.seq;
                
                while (bytes_read < 7) {
                    int n = read(fd, ptr + bytes_read, 7 - bytes_read);
                    if (n > 0) bytes_read += n;
                }

                // Verify Integrity
                uint16_t calc_crc = calculate_crc16((uint8_t*)&pkt.seq, 5);
                
                if (calc_crc == pkt.crc) {
                    int drops = (uint8_t)(pkt.seq - expected_seq);
                    printf("[VALID] Seq: %03d | Temp: %.2f C | Drops: %d\n", pkt.seq, pkt.temp, drops);
                    expected_seq = pkt.seq + 1;
                } else {
                    printf("[FAULT] CRC Mismatch! Packet Dropped.\n");
                }
                last_byte = 0x00; // Reset state machine
            } else {
                last_byte = byte_in;
            }
        }
    }
    return 0;
}
