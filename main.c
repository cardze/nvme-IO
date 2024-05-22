#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "zns.h"
#include "msg.h"

#define SIZE 4095
#define BUF_SIZE 4096
#define QD  32



void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void* aligned_alloc(size_t alignment, size_t size) {
    void *ptr;
    if (posix_memalign(&ptr, alignment, size)) {
        handle_error("posix_memalign");
    }
    return ptr;
}

unsigned long long elapsed_utime(struct timeval start_time,
                                 struct timeval end_time)
{
    unsigned long long err = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                             (end_time.tv_usec - start_time.tv_usec);
    return err;
}

int print_stars_to(char * device){
    int ret = 0;
    int fd = 0;
    unsigned int dev_nsid;
    unsigned long long ret_address;
    unsigned char data[SIZE] = {0};
    for (register int i = 0; i < SIZE; i++)
    {
        data[i] = (i % 2) ? 'A' : '*';
        if (i % 20 == 0)
            data[i] = '\n';
    }
    data[SIZE - 1] = '\0';
    if (!check_if_zone_device(device))
    {
        not_zone_device(device);
        ret = -1;
        return ret;
    }


    // open device
    fd = open(device, O_RDWR);
    if (!fd)
    {
        cannot_open(device);
        return -1;
    }

    ret = nvme_get_nsid(fd, &dev_nsid);
    if (ret < 0){
        no_namespace_id(device);
        goto out;
    }
    // write the data
    // prepare passthru struct
    struct nvme_passthru_cmd64 nvme_cmd;
    struct nvme_zns_append_args append_cmd;
    struct timeval start,
        end; // timing

    // check the block size
    unsigned int block_size = 0;
    get_block_size(device, &block_size);
    printf("The block size of %s is %u.\n", device, block_size);

    for (int zone_index = 0; zone_index < 3; zone_index++)
    {
        gettimeofday(&start, NULL);
        ret = clear_zone(device, zone_index);
        gettimeofday(&end, NULL);
        printf(" latency: zone reset at %d: %llu us\n",
               zone_index,
               elapsed_utime(start, end));
        if (ret < 0)
        {
            printf("Error resetting zone 0\n");
            goto out;
        }
        memset(&nvme_cmd, 0, sizeof(struct nvme_passthru_cmd));
        nvme_cmd.opcode = nvme_cmd_write;
        nvme_cmd.addr = (long long unsigned int)data;
        nvme_cmd.nsid = dev_nsid;
        nvme_cmd.data_len = SIZE;
        nvme_cmd.cdw10 = (0x0 | (zone_index << 20)); // slba
        nvme_cmd.cdw11 = 0;
        nvme_cmd.cdw12 = (SIZE / block_size); // sector number
        nvme_cmd.result = (__u64)&ret_address;

        memset(&append_cmd, 0, sizeof(struct nvme_zns_append_args));
        append_cmd.args_size = sizeof(append_cmd);
        append_cmd.fd = fd;
        append_cmd.nsid = dev_nsid;
        append_cmd.zslba = (zone_index << 20);
        append_cmd.nlb = (SIZE / block_size);
        append_cmd.data = data;
        append_cmd.data_len = SIZE;
        append_cmd.timeout = NVME_DEFAULT_IOCTL_TIMEOUT;
        append_cmd.result = &ret_address;


        // nvme write
        gettimeofday(&start, NULL);
        ret = ioctl(fd, NVME_IOCTL_IO64_CMD, &nvme_cmd);
        // ret = pwrite(fd, data, SIZE, (zone_index << 20) << 12);
        gettimeofday(&end, NULL);
        if (ret < 0)
        {
            err_writing_device(ret, device);
            goto out;
        }
        printf(" latency: zone pwrite at %d: %llu us\n",
               zone_index,
               elapsed_utime(start, end));
        int wp = 0;
        get_zone_wp(device, zone_index, &wp);
        printf(" The wp of zone %d is %x.\n\n", zone_index, wp);
        // nvme zns append
        gettimeofday(&start, NULL);
        ret = nvme_zns_append(&append_cmd);
        gettimeofday(&end, NULL);
        if (ret != 0)
        {
            err_writing_device(ret, device);
            goto out;
        }
        printf(" latency: zone append at %d: %llu us, and data start at %llx\n",
               zone_index,
               elapsed_utime(start, end),
               ret_address);
        get_zone_wp(device, zone_index, &wp);
        printf(" The wp of zone %d is %x.\n\n", zone_index, wp);
    }

    memset(data, 0, SIZE);
    pread(fd, data, SIZE, ret_address << 12);
    // read from start
    printf("The read output : \n%s\n", data);

out:
    close(fd);
    zbd_close(fd);
    show_int(ret);
    return ret;
}


void write_to_zns(struct io_uring *ring, int fd, const char *buffer, unsigned int zone) {
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct iovec iov;
    int ret;

    printf("Attempting to get an SQE for write operation...\n");
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        handle_error("io_uring_get_sqe returned NULL");
    }
    printf("Successfully obtained an SQE for write operation.\n");

    iov.iov_base = (void *)buffer;
    iov.iov_len = BUF_SIZE;

    io_uring_prep_writev(sqe, fd, &iov, 1, zone * BUF_SIZE);

    printf("Submitting write request...\n");
    ret = io_uring_submit(ring);
    if (ret < 0) {
        handle_error("io_uring_submit");
    }
    printf("Write request submitted.\n");

    ret = io_uring_wait_cqe(ring, &cqe);
    if (ret < 0) {
        handle_error("io_uring_wait_cqe");
    }

    if (cqe->res < 0) {
        errno = -cqe->res;
        handle_error("io_uring write");
    }

    io_uring_cqe_seen(ring, cqe);

    printf("Write operation completed successfully.\n");
}




void read_from_zns(struct io_uring *ring, int fd, char *buffer, unsigned int zone) {
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct iovec iov;
    int ret;

    printf("Attempting to get an SQE for read operation...\n");
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        handle_error("io_uring_get_sqe returned NULL");
    }
    printf("Successfully obtained an SQE for read operation.\n");

    iov.iov_base = buffer;
    iov.iov_len = BUF_SIZE;

    io_uring_prep_readv(sqe, fd, &iov, 1, zone * BUF_SIZE);

    printf("Submitting read request...\n");
    ret = io_uring_submit(ring);
    if (ret < 0) {
        handle_error("io_uring_submit");
    }
    printf("Read request submitted.\n");

    ret = io_uring_wait_cqe(ring, &cqe);
    if (ret < 0) {
        handle_error("io_uring_wait_cqe");
    }

    if (cqe->res < 0) {
        errno = -cqe->res;
        handle_error("io_uring read");
    }

    io_uring_cqe_seen(ring, cqe);

    printf("Read operation completed successfully.\n");
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device> <zone>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *device = argv[1];
    unsigned int zone = atoi(argv[2]);
    struct io_uring ring;
    char write_buffer[BUF_SIZE];
    char read_buffer[BUF_SIZE];
    int fd, ret;

    // clear zone
    ret = clear_zone(device, zone);
    if (ret < 0) {
        handle_error("clear_zone");
    }

    // Open the ZNS device
    fd = open(device, O_RDWR | O_DIRECT);
    if (fd < 0) {
        handle_error("open");
    }

    // Initialize io_uring
    ret = io_uring_queue_init(8, &ring, 0);
    if (ret < 0) {
        handle_error("io_uring_queue_init");
    }

    // Prepare the data to write
    memset(write_buffer, 'A', BUF_SIZE);

    // Write to ZNS
    write_to_zns(&ring, fd, write_buffer, zone);

    // Prepare the buffer for reading
    memset(read_buffer, 0, BUF_SIZE);

    // Read from ZNS
    read_from_zns(&ring, fd, read_buffer, zone);

    // Compare the buffers
    if (memcmp(write_buffer, read_buffer, BUF_SIZE) == 0) {
        printf("Read content matches written content.\n");
    } else {
        printf("Read content does NOT match written content.\n");
    }

    // Cleanup
    io_uring_queue_exit(&ring);
    close(fd);

    return 0;
}