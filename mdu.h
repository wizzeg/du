#pragma once
#include "target.h"
#include "jobber.h"
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void haz_mutex_init(pthread_mutex_t* mutex);
void get_opts(int argc, char** argv, int* get_threadnum, int* get_optind);
struct thread_job* create_thread_job(int argc, char** argv, int threadnum, int set_optind, int* exit_code);
void* thread_loop(void* arg);