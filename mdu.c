#include "mdu.h"

/**
 * hazardously checks so that hte mutex initialized correctly, exits if not
 * 
 * @param mutex     the mutex to initialize
 * @return      void
 */
void haz_mutex_init(pthread_mutex_t* mutex)
{
    if (pthread_mutex_init(mutex, NULL) != 0)
    {
        perror("failed to init mutex");
        exit(EXIT_FAILURE);
    }
}

/**
 * checks through the argv for options, sets pointers to wanted values;
 * 
 * @param argc     the argc the program got at start
 * @param argv     pointer of argv the program got at start
 * @param get_threadnum     pointer to communicate number of threads, which is set to number of threads to use;
 * @param get_optind     a pointer to a copy of optind, though optind is a global variable, seems more proper to global variable here since it wasn't set during this function
 * @return      void
 */
void get_opts(int argc, char** argv, int* get_threadnum, int* get_optind)
{
    //get the arguments/options and set number of threads
    int argnum;
    *get_threadnum = 1;
    while ((argnum = getopt(argc, argv, "j:")) != -1) // this was considered ok in mmake
    {

        if (argnum == 'j'){
            *get_threadnum = atoi(optarg);
            if (*get_threadnum < 1)
            {
                fprintf(stderr,"Less than one thread assigned, or no integers, setting threads to 1\n");
                *get_threadnum = 1;
            }
            
        }
        else
        {
            fprintf(stderr,"program shut down, %c is not an option or invalid argument\n", (char)optopt); // I dunno, looks nice I guess, did not use this i mmake but was ok anyway
            exit(EXIT_FAILURE);
        }
    }
    *get_optind = optind;
}

/**
 * checks through the argv for options, makes the thread_job and targets;
 * 
 * @param argc     the argc the program got at start
 * @param argv     pointer of argv the program got at start
 * @param threadnum     number of threads;
 * @param set_optind     a copy of optind, though optind is a global variable, seems more proper to use local variable here since it wasn't set during this function
 * @param exit_code     a pointer so that the thread_job can set the exit result for the whole program
 * @return      void
 */
struct thread_job* create_thread_job(int argc, char** argv, int threadnum, int set_optind, int* exit_code)
{
    struct thread_job* thread_job = haz_malloc(sizeof(struct thread_job));
    thread_job->num_targets = 0;
    thread_job->num_threads = threadnum;
    thread_job->current_target = 0;
    thread_job->kill_threads = false;
    thread_job->exit_code = exit_code;
    thread_job->active_threads = threadnum;
    if (sem_init(&thread_job->sem_threads, 0, 0) != 0)
    {
        perror("failed to init semaphore");
        exit(EXIT_FAILURE);
    }
    haz_mutex_init(&thread_job->targetsLock);
    haz_mutex_init(&thread_job->threadsLock);
    haz_mutex_init(&thread_job->exitLock);

    struct target* targets;
    if ((argc - set_optind) > 0)
    {
        targets = haz_malloc((size_t)(sizeof(struct target) * (argc - set_optind)));
        for (int i = set_optind; argv[i] != NULL; i++)
        {
            target_setup(&targets[i - set_optind], argv[i]);
            thread_job->num_targets++;
        }
    }
    else
    {
        targets = haz_malloc((size_t)(sizeof(struct target)));
        char* path = ".";
        target_setup(targets, path);
        thread_job->num_targets++;
    }

    // put the targets into the thread_job
    thread_job->targets = targets;
    return thread_job;
}

/**
 * this is the thread loop that all the threads run, it'll run as long as thread aren't told to end themselves
 * it'll check if it should change the jobs, if not it'll try and get a job, if it got no job then thread goes to sleep
 * if it did get a job it'll do the job and in that job it may wakeup threads, it'll only return once it's reached the end of the directory
 * 
 * @param arg     a void pointer to a thrad_job
 * @return      void*
 */
void* thread_loop(void* arg)
{
    struct thread_job* thread_job;
    thread_job = (struct thread_job*) arg;
    // idea, do the work loop, and add to a local list, add paths as many times as there are inactive threads
    while (!job_kill(thread_job))
    {
        char* path = job_get(thread_job);
        if (path == NULL)
        {
            job_wait(thread_job);
        }
        else
        {
            int size = job_do(thread_job, path);
            if (size < 0)
            {
                fprintf(stderr, "job_do got a NULL job, shouldn't have happened but proceed\n");
            }
            else
            {
                job_add_size(thread_job, size);
                free(path);
            }
        }
        job_status(thread_job);
    }
    return NULL;
}

/**
 * main of mdu, runs the program. Cleans up everything before it quits.
 * 
 * @param argc     the argc the program got at start
 * @param argv     pointer of argv the program got at start
 * @return      a pointer to the thread_job that it created
 */
int main(int argc, char **argv)
{
    int* exit_code = haz_malloc(sizeof(int));
    int* get_threadnum = haz_malloc(sizeof(int));
    int* get_optind = haz_malloc(sizeof(int));
    *exit_code = 0;

    get_opts(argc, argv, get_threadnum, get_optind);
    struct thread_job* thread_job = create_thread_job(argc, argv, *get_threadnum, *get_optind, exit_code);
    free(get_optind);
    free(get_threadnum);

    pthread_t threads[thread_job->num_threads];
    for (int i = 0; i < thread_job->num_threads; i++) // loop and make threads
    {
        if (pthread_create(&threads[i], NULL, &thread_loop, (void*) thread_job) != 0) // create threads
        {
            perror("failed to creat thread\n");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < thread_job->num_threads; i++) // join all the threads
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("failed to join thread\n");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < thread_job->num_targets; i++) // clean up
    {
        free(thread_job->targets[i].target);
        free(thread_job->targets[i].path_list);
    }

    int result = *exit_code; // valgrind workaround, and cleanup
    free(exit_code);
    pthread_mutex_destroy(&thread_job->threadsLock);
    pthread_mutex_destroy(&thread_job->exitLock);
    pthread_mutex_destroy(&thread_job->targetsLock);
    if (sem_destroy(&thread_job->sem_threads) != 0)
    {
        perror("failed to destory semaphore");
        exit(EXIT_FAILURE);
    }
    
    ;
    free(thread_job->targets);
    free(thread_job);

    return result;
}