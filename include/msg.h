#pragma once
#include <stdio.h>
#include <stdlib.h>

void hello_world();
void show_op_code(int op_code);
void wrong_arg();
void cannot_open(char *device);
void err_writing_device(int error_code, char *device);
void show_result(int result);
void show_int(int value);
void not_zone_device(char * device);
void debug_seg();
void no_namespace_id(char * device);