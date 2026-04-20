#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

int main() {
    // 1. Open for Read/Write
    int fd = open("/dev/ser2", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) { perror("Failed to open /dev/ser2"); return 1; }

    // 2. Force strict RAW mode (115200 8N1)
    struct termios cfg;
    tcgetattr(fd, &cfg);
    cfsetspeed(&cfg, B115200);
    cfg.c_cflag |= (CLOCAL | CREAD | CS8);
    cfg.c_cflag &= ~(PARENB | CSTOPB);
    cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // KILL Canonical mode
    cfg.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    cfg.c_oflag &= ~OPOST;
    
    // Non-blocking read timeout (1 second)
    cfg.c_cc[VMIN] = 0;
    cfg.c_cc[VTIME] = 10; 
    tcsetattr(fd, TCSANOW, &cfg);

    // 3. Blast the hardware
    char *tx_msg = "BARE_METAL_SUCCESS\r\n";
    printf("TX: %s", tx_msg);
    write(fd, tx_msg, strlen(tx_msg));
    tcdrain(fd); // Wait for the physical UART FIFO to empty

    // 4. Read the loopback
    char rx_buf[64] = {0};
    int bytes = read(fd, rx_buf, sizeof(rx_buf) - 1);
    
    if (bytes > 0) {
        printf("RX: %s", rx_buf);
    } else {
        printf("RX: [NOTHING] -> Pinmux is dead or wire is broken.\n");
    }

    close(fd);
    return 0;
}
