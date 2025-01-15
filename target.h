#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#define STARTSIZE 2;

// structure that holds information needed to go through a list of path
struct target{
    char** path_list;
    char* target;
    int path_num;
    int path_head;
    int path_size;

    int target_size;
};
void target_extend_pathlist(struct target *target);
void* haz_strdup(char* string);
void* haz_malloc(size_t size);
void* haz_realloc(void* source, size_t size);
void target_setup(struct target* target, char* path);
void target_addpath(struct target* target, char* path);
void target_addcatpath(struct target* target, char* path, const char* d_path);
char* target_getpath(struct target* target);
char* target_appendstr(char* destination, const char* appendee);