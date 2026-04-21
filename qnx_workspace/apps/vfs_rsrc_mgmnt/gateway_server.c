#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/resmgr.h>

// --- POSIX VFS Data Structures ---
typedef struct {
    float   current_temp;
    uint8_t link_status; 
    uint8_t total_drops;
} asg_vfs_telemetry_t;

typedef struct {
    pthread_mutex_t lock;
    asg_vfs_telemetry_t data;
} asg_state_t;

asg_state_t asg_state = { .lock = PTHREAD_MUTEX_INITIALIZER };
int uart_fd = -1;

#pragma pack(push, 1)
typedef struct {
    uint16_t header;
    uint8_t  seq;
    float    temp;
    uint16_t crc;
} __attribute__((packed)) SensorPacket;
#pragma pack(pop)

// --- Shared Hardware Logic ---
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
    printf("[ASG_VFS] Hardware ingest thread active on /dev/ser2\n");
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
                    pthread_mutex_lock(&asg_state.lock);
                    asg_state.data.current_temp = pkt.temp;
                    asg_state.data.total_drops += (uint8_t)(pkt.seq - expected_seq);
                    asg_state.data.link_status = 1;
                    pthread_mutex_unlock(&asg_state.lock);
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

// --- POSIX Override: _IO_READ ---
int asg_io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    int status;
    asg_vfs_telemetry_t current_data;

    if ((status = iofunc_read_verify(ctp, msg, ocb, NULL)) != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) { return ENOSYS; }
    pthread_mutex_lock(&asg_state.lock);
    current_data = asg_state.data;
    pthread_mutex_unlock(&asg_state.lock);

    size_t bytes_left = sizeof(asg_vfs_telemetry_t) - ocb->offset;
    size_t nbytes = min(msg->i.nbytes, bytes_left);

    if (nbytes > 0) {
        MsgReply(ctp->rcvid, nbytes, ((char*)&current_data) + ocb->offset, nbytes);
        ocb->offset += nbytes;
    } else {
        MsgReply(ctp->rcvid, 0, NULL, 0); // EOF
    }
    return _RESMGR_NOREPLY;
}

// --- POSIX Override: _IO_WRITE ---
int asg_io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb) {
    int status;
    uint8_t user_cmd; 

    if ((status = iofunc_write_verify(ctp, msg, ocb, NULL)) != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) { return ENOSYS; }
    _IO_SET_WRITE_NBYTES(ctp, msg->i.nbytes);

    // Read 1 byte from the client (0 = OFF, 1 = ON)
    if (resmgr_msgread(ctp, &user_cmd, 1, sizeof(msg->i)) != -1) {
        printf("[ASG_VFS] Actuate Cooling: %d\n", user_cmd);
        
        // Abstract the protocol: VFS Server wraps the physical layer framing
        uint8_t hw_cmd[3] = {0xBB, 0x66, user_cmd};
        write(uart_fd, hw_cmd, 3);
        fsync(uart_fd); // Flush to serial driver immediately
    }

    if (msg->i.nbytes > 0) ocb->offset += msg->i.nbytes;
    return _RESMGR_NPARTS(0);
}

// --- Main Setup ---
int main(int argc, char *argv[]) {
    // 1. Hardware Init
    uart_fd = open("/dev/ser2", O_RDWR | O_NOCTTY);
    if (uart_fd < 0) { perror("Port Error"); return EXIT_FAILURE; }

    struct termios cfg;
    tcgetattr(uart_fd, &cfg);
    cfsetspeed(&cfg, B115200);
    cfg.c_cflag |= (CLOCAL | CREAD | CS8);
    cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tcsetattr(uart_fd, TCSANOW, &cfg);

    pthread_t ingest_tid;
    pthread_create(&ingest_tid, NULL, uart_ingest_thread, NULL);

    // 2. QNX VFS Init
    dispatch_t *dpp = dispatch_create();
    resmgr_attr_t resmgr_attr;
    resmgr_context_t *ctp;
    iofunc_attr_t io_attr;
    resmgr_connect_funcs_t connect_funcs;
    resmgr_io_funcs_t io_funcs;

    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 1;
    resmgr_attr.msg_max_size = 2048;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read = asg_io_read;
    io_funcs.write = asg_io_write;

    iofunc_attr_init(&io_attr, S_IFCHR | 0666, NULL, NULL);

    if (resmgr_attach(dpp, &resmgr_attr, "/dev/asg_sensor", _FTYPE_ANY, 0, 
                      &connect_funcs, &io_funcs, &io_attr) == -1) {
        perror("resmgr_attach failed");
        return EXIT_FAILURE;
    }

    printf("[ASG_VFS] Mounted /dev/asg_sensor\n");

    // 3. Thread Pool Init
    thread_pool_attr_t pool_attr;
    memset(&pool_attr, 0, sizeof(pool_attr));
    pool_attr.handle = dpp;
    
    // QNX 8.0 strict casting for dispatch context to resmgr context
    pool_attr.context_alloc = (resmgr_context_t *(*)(dispatch_t *)) dispatch_context_alloc;
    pool_attr.block_func    = (resmgr_context_t *(*)(resmgr_context_t *)) dispatch_block;
    pool_attr.unblock_func  = (void (*)(resmgr_context_t *)) dispatch_unblock;
    pool_attr.handler_func  = (int (*)(resmgr_context_t *)) dispatch_handler;
    pool_attr.context_free  = (void (*)(resmgr_context_t *)) dispatch_context_free;

    pool_attr.lo_water = 2;
    pool_attr.hi_water = 4;
    pool_attr.increment = 1;
    pool_attr.maximum = 10; 

    thread_pool_t *tpp = thread_pool_create(&pool_attr, POOL_FLAG_USE_SELF);
    if (tpp == NULL) {
        perror("thread_pool_create failed");
        return EXIT_FAILURE;
    }
    thread_pool_start(tpp);
    return EXIT_SUCCESS;
}
