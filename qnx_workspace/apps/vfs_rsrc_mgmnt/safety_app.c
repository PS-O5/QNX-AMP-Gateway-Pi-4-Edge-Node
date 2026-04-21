#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

// Standard structure matching the VFS output
typedef struct {
    float   current_temp;
    uint8_t link_status; 
    uint8_t total_drops;
} asg_vfs_telemetry_t;

int main() {
    printf("[CLIENT] Safety Application Starting...\n");

    int sensor_fd;
    while ((sensor_fd = open("/dev/asg_sensor", O_RDWR)) == -1) {
        sleep(1);
    }
    printf("[CLIENT] Opened /dev/asg_sensor.\n");

    asg_vfs_telemetry_t telemetry;
    uint8_t current_cooling_state = 0; 

    while (1) {
        // POSIX Read - Grabs the structured telemetry block
        if (read(sensor_fd, &telemetry, sizeof(telemetry)) == sizeof(telemetry)) {
            
            if (telemetry.link_status == 1) {
                printf("[CLIENT] Temp: %.2f C | Drops: %d\n", telemetry.current_temp, telemetry.total_drops);
                       
                // Trigger Logic
                if (telemetry.current_temp > 28.0f && current_cooling_state == 0) {
                    printf("[CLIENT] CRITICAL TEMP! Blasting COOLING_ON...\n");
                    current_cooling_state = 1;
                    write(sensor_fd, &current_cooling_state, 1);
                } 
                else if (telemetry.current_temp < 25.0f && current_cooling_state == 1) {
                    printf("[CLIENT] Temp Normal. Blasting COOLING_OFF...\n");
                    current_cooling_state = 0;
                    write(sensor_fd, &current_cooling_state, 1);
                }
            }
        }
        
        // Reset file offset for next poll 
        // (CRITICAL: Because standard POSIX read advances the offset)
        lseek(sensor_fd, 0, SEEK_SET);

        usleep(500000); // 2Hz polling
    }

    close(sensor_fd);
    return 0;
}
