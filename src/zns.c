#include "msg.h"
#include "zns.h"


int get_block_size(char *device, unsigned int *size){
    int ret = 0;
    int fd = 0;
    struct zbd_info dev_info;
    fd = zbd_open(device, O_RDONLY, &dev_info);
    *size = dev_info.pblock_size;
    zbd_close(fd);
    return ret;
}

int get_zone_wp(char * device, int zone_num, int *wp){
    int ret = 0;
    int fd = 0;
    struct zbd_info dev_info;
    struct zbd_zone *zones;
    int num_of_zone =0;

    fd = zbd_open(device, O_RDONLY, &dev_info);
    if(fd < 0){
        ret = -1;
        return ret;
    }
    zbd_list_zones(fd, (zone_num << 20),0, ZBD_RO_ALL, &zones, &num_of_zone);
    *wp = zbd_zone_wp(&zones[zone_num]);
    zbd_close(fd);
    return ret;
}

/*
* This is a costy check.
*/
bool zone_is_clear(char * device, int zone_num){
    bool ret = false;
    int fd = 0;
    struct zbd_info dev_info;
    struct zbd_zone *zones;
    int num_of_zone =0;

    fd = zbd_open(device, O_RDONLY, &dev_info);
    zbd_list_zones(fd, (zone_num << 20),0, ZBD_RO_ALL, &zones, &num_of_zone);
    ret = zbd_zone_empty(&zones[zone_num]);
    zbd_close(fd);
    return ret;
}

int clear_first_zone(char *device){
    int ret = 0;
    int fd = 0;
    struct zbd_info dev_info;
    struct zbd_zone *zones;
    int num_of_zone =0;


    fd = open(device, O_RDWR);

    struct nvme_zns_mgmt_send_args args = {
		.args_size	= sizeof(args),
		.fd		= fd,
		.nsid		= 1,
		.slba		= 0,
		.zsa		= NVME_ZNS_ZSA_RESET,
		.select_all	= 0,
		.zsaso		= 0,
		.data_len	= 0,
		.data		= NULL,
		.result		= NULL,
	};

    ret = nvme_zns_mgmt_send(&args);
    if(ret){
        printf("reset error!\n");
    }
    close(fd);
    return ret;
}

int clear_zone(char * device, int zone_num){
    int ret = 0;
    int fd = 0;
    struct zbd_info dev_info;
    struct zbd_zone *zones;
    int num_of_zone =0;

    fd = open(device, O_RDWR);
    __u32 result = 0;
    struct nvme_zns_mgmt_send_args args = {
		.args_size	= sizeof(args),
		.fd		= fd,
		.nsid		= 1,
		.slba		= (zone_num << 20),
		.zsa		= NVME_ZNS_ZSA_RESET,
		.select_all	= 0,
		.zsaso		= 0,
		.data_len	= 0,
		.data		= NULL,
		.result		= &result,
	};

    ret = nvme_zns_mgmt_send(&args);
    if(ret){
        printf("reset error!\n");
    }
    close(fd);
    return ret;
}

bool check_if_zone_device(char * device){
    return zbd_device_is_zoned(device) == 1;
}
