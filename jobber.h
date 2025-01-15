#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <semaphore.h>
#include <pthread.h>
#include "target.h"

// struct for the thread_job that all the threads share
struct thread_job{
    struct target* targets;
    int current_target;
    int num_targets;
    pthread_mutex_t targetsLock;

    int num_threads;
    int active_threads;
    bool kill_threads;
    pthread_mutex_t threadsLock;
    sem_t sem_threads;

    int* exit_code;
    pthread_mutex_t exitLock;
};
int haz_semval(sem_t* sem);
void haz_lstat(char* path, struct stat* stat);
int job_getsize(char* path, const char* d_name);
int job_readdir(struct thread_job* thread_job, struct target* target, DIR* d, char* current_path);
struct dirent* safe_readdir(DIR* d, pthread_mutex_t* exitLock, int* exit_code);
int job_opendir_failed(struct thread_job* thread_job, char* current_path);
bool job_kill(struct thread_job* thread_job);
void job_wait(struct thread_job* thread_job);
char* job_get(struct thread_job* thread_job);
int job_do(struct thread_job* thread_job, char* path);
void job_add_size(struct thread_job* thread_job, int size);
void job_status(struct thread_job* thread_job);
void job_checkothers(struct thread_job* thread_job, struct target* target);

