#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <libnvme.h>
#include <linux/nvme_ioctl.h>
#include "libzbd/zbd.h"
// #include "libzbd/zbd.h"

int clear_first_zone(char * device);
int clear_zone(char * device, int zone_num); // zone_num is 0-based
int get_zone_wp(char * device, int zone_num, int *wp);
int get_block_size(char *device, unsigned int *size);

bool check_if_zone_device(char * device);
