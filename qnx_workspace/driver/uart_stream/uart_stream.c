#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd = open("/dev/ser2", O_RDONLY | O_NOCTTY);
    if (fd < 0) { perror("Failed to open"); return 1; }

    struct termios cfg;
    tcgetattr(fd, &cfg);
    cfsetspeed(&cfg, B115200);
    cfg.c_cflag |= (CLOCAL | CREAD | CS8);
    cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tcsetattr(fd, TCSANOW, &cfg);

    printf("Automotive Safety Gateway: Sensor Link Established.\n");
    printf("Listening for Edge Node data on UART3...\n");

    char buf[128];
    int idx = 0;

    // Infinite Ingest Loop
    while (1) {
        char c;
        if (read(fd, &c, 1) > 0) {
            // End of line detection
            if (c == '\n' || c == '\r') {
                if (idx > 0) {
                    buf[idx] = '\0'; // Terminate string
                    printf("[EDGE NODE]: %s\n", buf);
                    idx = 0; // Reset buffer
                }
            } else if (idx < sizeof(buf) - 1) {
                buf[idx++] = c;
            }
        }
    }
    close(fd);
    return 0;
}
