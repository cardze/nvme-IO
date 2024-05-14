#include "msg.h"


void hello_world(){
    printf("Hello world.\n");
}

void show_op_code(int op_code){
    printf("The op_code is: %d\n", op_code);
}

void wrong_arg(){
    printf("ERROR: The number of args is wrong, example: ./main /dev/nvme0n1\n");
}

void cannot_open(char *device){
    printf("Cannot open device %s\n", device);
}

void err_writing_device(int error_code, char *device){
    printf("ERROR : error %d on writing device %s\n", error_code, device);
}

void show_result(int result){
    printf("The result code is %d\n", result);
}

void show_int(int value){
    printf("The value is %d\n", value);
}

void not_zone_device(char * device){
    printf("%s is not a zone deivce.\n", device);
}
void no_namespace_id(char * device){
    printf("%s has no namespace id.\n", device);
}
void debug_seg(){
    printf("Seg not here!\n");
}