#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "zns.h"
#include "msg.h"

#define SIZE 4095
#define BUF_SIZE 4096
#define BLOCK_SIZE 4096

unsigned long long elapsed_utime(struct timeval start_time,
                                 struct timeval end_time)
{
    unsigned long long err = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                             (end_time.tv_usec - start_time.tv_usec);
    return err;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int fd, zbd_fd;
    unsigned int dev_nsid;
    char * device;
    unsigned long long ret_address;

    if (argc < 2)
    {
        wrong_arg();
        return -1;
    }
    device = argv[1];
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

    // if(fd != zbd_fd){
    //     printf("zbd_fd != fd\n");
    // }

    ret = nvme_get_nsid(fd, &dev_nsid);
    if (ret < 0){
        no_namespace_id(device);
        goto out;
    }
    // write the data
    // prepare passthru struct
    struct nvme_passthru_cmd nvme_cmd;
    struct nvme_zns_append_args append_cmd;
    // unsigned int result = 0;
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
        nvme_cmd.result = NULL;

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
        ret = ioctl(fd, NVME_IOCTL_IO_CMD, &nvme_cmd);
        gettimeofday(&end, NULL);
        if (ret != 0)
        {
            err_writing_device(ret, device);
            goto out;
        }
        printf(" latency: zone write at %d: %llu us\n",
               zone_index,
               elapsed_utime(start, end));
        unsigned long long wp = 0;
        get_zone_wp(device, zone_index, &wp);
        printf(" The wp of zone %d is %llx.\n\n", zone_index, wp);
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
        printf(" The wp of zone %d is %llx.\n\n", zone_index, wp);
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