#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <tinyxpc/tinyxpc.h>
#include <tinyxpc/xpc_relay.h>
#include <epoll_app.h>
#include <alibc/containers/array.h>
#include <alibc/containers/dynabuf.h>
#include <alibc/containers/iterator.h>
#include <alibc/containers/array_iterator.h>
#include <alc_string.h>
#include <os_log.h>
#include <unix_xpc_relay.h>

typedef struct {
    xpc_relay_state_t *relay;
    int debug_out_fd, pilot_ctrl_fd, quaternion_out_fd;
    int imu_out_fd, avi_in_tune_fd, avi_out_fd;
    int uart_fd;
} app_ctx_t;

app_ctx_t *global_app;
epoll_app_t *global_epoll_app;

int debug_log_fd = 0;

static const int epoll_rd_flags = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
static const int epoll_wr_flags = EPOLLOUT | EPOLLHUP;
static const int epoll_rdwr_flags = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP;


void epoll_io_notify_config(void *io_ctx, int which, bool enable) {
    // this is called by the xpc relay
    int fd = global_app->uart_fd;
    struct epoll_event *events = epoll_app_get_fd_events(global_epoll_app, fd);
    if(events == NULL) {
        epoll_app_add_fd(global_epoll_app, fd, epoll_rdwr_flags);
    }
    else {
        if(which) { // change write notify state
            if(enable) {
                printf("write notify enabled\n");
                events->events |= (epoll_wr_flags);
            }
            else {
                printf("write notify disabled\n");
                events->events &= ~(epoll_wr_flags);
            }
        }
        else {
            if(enable) {
                printf("read notify enabled\n");
                events->events |= (epoll_rd_flags);
            }
            else {
                printf("read notify disabled\n");
                events->events &= ~(epoll_rd_flags);
            }
        }
        epoll_app_mod_fd(global_epoll_app, fd, events->events);
    }
}

bool route_xpc_msg(void *msg_ctx, txpc_hdr_t *msg_hdr, char *payload) {
    printf("payload: \"%s\"\n", payload);
    fflush(stdout);
    return true;
}

int configure_device(char *dev_path, int baud, int flags) {
    int fd = -1;
    fd = open(dev_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(fd == -1) {
        goto done;
    }

    // set flags
    fcntl(fd, F_SETFL, flags);

    // set the port as being exclusive
    ioctl(fd, TIOCEXCL, NULL);

    struct termios options;
    tcgetattr(fd, &options);
    cfmakeraw(&options);
    // FIXME there has to be a better way to set these baud rates.
    cfsetspeed(&options, B115200);
    tcsetattr(fd, TCSANOW, &options);

done:
    return fd;
}

static void unix_signal_handler(int signum) {
    switch(signum) {
        case SIGINT:
            global_epoll_app->run_mainloop = false;
        break;

        default:
        fprintf(stderr, "got unhandled signal: %s\n", strsignal(signum));
        break;
    }
}

int openfifo(char *filename, mode_t fifo_mode, int open_mode) {
    int fd = -1;
    if(mkfifo(filename, fifo_mode) < 0 && errno != EEXIST) {
        perror("mkfifo");
        goto done;
    }
    fd = open(filename, open_mode | O_NONBLOCK);
done:
    return fd;
}

char pread_buf[4096];
void epoll_read_cb(void *ctx, int fd) {
    /*
     *int bytes = pread(fd, pread_buf, 4096, SEEK_CUR);
     *write(debug_log_fd, pread_buf, bytes);
     */
    /*printf("read notify on fd: %i\n", fd);*/
    if(fd == global_app->uart_fd) {
        /*printf("rd state: %i\n", global_app->relay->inflight_rd_op.op);*/
        xpc_rd_op_continue(global_app->relay);
    }
}

void epoll_write_cb(void *ctx, int fd) {
    /*printf("write notify on fd: %i\n", fd);*/
    if(fd == global_app->uart_fd) {
        /*printf("wr state: %i\n", global_app->relay->inflight_wr_op.op);*/
        xpc_wr_op_continue(global_app->relay);
    }
}

int main(int argc, char **argv) {
    int status = 0;
/*
 *
 *    log_info_t *log_info = initialize_log_info("debug_indices.bin", "strings.bin");
 *    // strings loaded, they can now be used
 *    printf(string_get_cstr((string_t *)array_fetch(log_info->log_strings, 0)), "world");
 */
    // application parameters
    size_t xpc_buf_sz = 4096;
    char *uart_filename = "/dev/ttyACM0";
    int uart_baud = 115200;

    char *debug_filename = "xpc_debug";
    char *pilot_ctrl_filename = "xpc_pilot_ctrl";
    char *quaternion_filename = "xpc_quaternion_stream";
    char *raw_data_filename = "xpc_raw_imu_stream";
    char *avionics_tune_filename = "xpc_avionics_tune";
    char *avionics_data = "xpc_avionics_data";

    // application state variables
    struct sigaction sa;
    xpc_relay_state_t uart_relay;
    unix_xpc_io_ctx_t *uart_io_ctx = NULL;
    global_app = malloc(sizeof(app_ctx_t));
    if(global_app == NULL) {
        status = -4;
        goto done;
    }
    global_app->relay = &uart_relay;
    
    // state and submodule init
    uart_io_ctx = create_unix_io_ctx(xpc_buf_sz, 0, 0);

    xpc_relay_config(
        global_app->relay, uart_io_ctx, global_app, NULL,
        unix_xpc_write, unix_xpc_read, unix_xpc_reset,
        epoll_io_notify_config, route_xpc_msg,
        NULL, NULL
    );

    global_epoll_app = create_epoll_app(0, global_app);
    if(global_epoll_app == NULL) {
        fprintf(stderr, "epoll init failed\n");
        status = -1;
    }

    // set up unix signals
    sa.sa_handler = unix_signal_handler;
    sigfillset(&sa.sa_mask);
    sigdelset(&sa.sa_mask, SIGINT);

    // set event handlers
    global_epoll_app->epollin_cb = epoll_read_cb;
    global_epoll_app->epollout_cb = epoll_write_cb;

    // open files and fifos
    // see man 7 fifo - use O_RDWR to prevent blocking and ENXIO on O_WRONLY
    // for fifo open
    global_app->uart_fd = configure_device(uart_filename, uart_baud, 0 /*FNDELAY*/);
    if(global_app->uart_fd == -1) {
        perror("open");
        status = -2;
        goto bad_init;
    }


    global_app->debug_out_fd = openfifo(debug_filename, 0660, O_RDWR);
    if(global_app->debug_out_fd == -1) {
        close(global_app->uart_fd);
        status = -3;
        goto bad_init;
    }

    global_app->pilot_ctrl_fd = openfifo(pilot_ctrl_filename, 0660, O_RDONLY);
    if(global_app->pilot_ctrl_fd == -1) {
        close(global_app->uart_fd);
        close(global_app->debug_out_fd);
        status = -3;
        goto bad_init;
    }

    global_app->quaternion_out_fd = openfifo(quaternion_filename, 0660, O_RDWR);
    if(global_app->quaternion_out_fd == -1) {
        close(global_app->uart_fd);
        close(global_app->debug_out_fd);
        close(global_app->pilot_ctrl_fd);
        status = -3;
        goto bad_init;
    }

    global_app->imu_out_fd = openfifo(raw_data_filename, 0660, O_RDWR);
    if(global_app->imu_out_fd == -1) {
        close(global_app->uart_fd);
        close(global_app->debug_out_fd);
        close(global_app->pilot_ctrl_fd);
        close(global_app->quaternion_out_fd);
        status = -3;
        goto bad_init;
    }

    global_app->avi_in_tune_fd = openfifo(avionics_tune_filename, 0660, O_RDONLY);
    if(global_app->avi_in_tune_fd == -1) {
        close(global_app->uart_fd);
        close(global_app->debug_out_fd);
        close(global_app->pilot_ctrl_fd);
        close(global_app->quaternion_out_fd);
        close(global_app->imu_out_fd);
        status = -3;
        goto bad_init;
    }

    global_app->avi_out_fd = openfifo(avionics_data, 0660, O_RDWR);
    if(global_app->avi_out_fd == -1) {
        close(global_app->uart_fd);
        close(global_app->debug_out_fd);
        close(global_app->pilot_ctrl_fd);
        close(global_app->quaternion_out_fd);
        close(global_app->imu_out_fd);
        close(global_app->avi_out_fd);
        status = -3;
        goto bad_init;
    }

    // wow, that's a lot of fds
    epoll_app_add_fd(global_epoll_app, global_app->uart_fd, epoll_rdwr_flags);
    epoll_app_add_fd(global_epoll_app, global_app->debug_out_fd, epoll_wr_flags);
    epoll_app_add_fd(global_epoll_app, global_app->pilot_ctrl_fd, epoll_rdwr_flags);
    epoll_app_add_fd(global_epoll_app, global_app->quaternion_out_fd, epoll_wr_flags);
    epoll_app_add_fd(global_epoll_app, global_app->imu_out_fd, epoll_wr_flags);
    epoll_app_add_fd(global_epoll_app, global_app->avi_in_tune_fd, epoll_rd_flags);
    epoll_app_add_fd(global_epoll_app, global_app->avi_out_fd, epoll_wr_flags);

    uart_io_ctx->read_fd = global_app->uart_fd;
    uart_io_ctx->write_fd = global_app->uart_fd;
    
    // FIXME: have to flush buffers here, I think?
    
    unix_xpc_reset(uart_io_ctx, 1, -1);
    unix_xpc_reset(uart_io_ctx, 0, -1);
    printf("flush finished\n");

    debug_log_fd = open("debug.txt", O_WRONLY|O_CREAT, S_IRWXU);
    // send reset message
    /*xpc_relay_send_reset(global_app->relay);*/

    // linux epoll event loop
    epoll_app_mainloop(global_epoll_app);

    close(global_app->uart_fd);
    close(global_app->debug_out_fd);
    close(global_app->pilot_ctrl_fd);
    close(global_app->quaternion_out_fd);
    close(global_app->imu_out_fd);
    close(global_app->avi_out_fd);
    close(global_app->avi_out_fd);
    close(debug_log_fd);
bad_init:
    unix_xpc_io_ctx_free(uart_io_ctx);
    destroy_epoll_app(global_epoll_app);
    free(global_app);
done:
    return status;
}
