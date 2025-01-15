#include "jobber.h"

/**
 * hazardously checks so that sem_getvalue is run correctly, exits if not
 * 
 * @param sem     the semaphore to check
 * @return      the value of the semaphore
 */
int haz_semval(sem_t* sem)
{
    int sem_val;
    if (sem_getvalue(sem, &sem_val) != 0)
    {
        fprintf(stderr,"failed to get sem value");
        perror("");
        exit(EXIT_FAILURE);
    }
    return sem_val;
}

/**
 * hazourdus lstat, checks so that lstat performed as it should, or it kills the program
 * 
 * @param path     potiner to the path to check
 * @param stat     pointer to the stat file to write to
 * @return      void
 */
void haz_lstat(char* path, struct stat* stat)
{
    if (lstat(path, stat) != 0)
    {
        perror("lstat failed");
        exit(EXIT_FAILURE);
    }
}


/**
 * Does error check for readdir and returns a dirent to check
 *
 * @param d     a DIR* to read from
 * @param exitLock     exit lock thatlocks exit_code
* @param exit_code    exit_code to modify
 * @return      the 512 block size of the file
 */
struct dirent* safe_readdir(DIR* d, pthread_mutex_t* exitLock, int* exit_code)
{
    errno = 0;
    struct dirent* dir = readdir(d);
    if (dir == NULL && errno != 0)
    {
        pthread_mutex_lock(exitLock);
        
        fprintf(stderr,"readdir error ");
        perror("");
        *exit_code = 1;

        pthread_mutex_unlock(exitLock);
    }
    return dir;
}

/**
 * gets the size of a file
 *
 * @param path     the origin path
 * @param d_name     the file/directory name
 * @return      the 512 block size of the file
 */
int job_getsize(char* path, const char* d_name)
{
    char* tempPath = haz_strdup(path);
    tempPath = target_appendstr(tempPath, d_name);

    struct stat file;
    haz_lstat(tempPath, &file);
    int size = file.st_blocks;
    free(tempPath);
    return size;
}

/**
 * checks wether threads should end themselves or not
 *
 * @param thread_job     the thread_job to check
 * @return      boolean, true if the thread should end itself
 */
bool job_kill(struct thread_job* thread_job)
{
    pthread_mutex_lock(&thread_job->threadsLock);
    if (thread_job->kill_threads)
    {
        pthread_mutex_unlock(&thread_job->threadsLock);
        return true;
    }
    pthread_mutex_unlock(&thread_job->threadsLock);
    return false;
}

/**
 * adds size to the target that the thread has been working on
 * 
 * @param thread_job     thread_job that is relevant for the thread
 * @param size     size to add
 * @return      void
 */
void job_add_size(struct thread_job* thread_job, int size)
{
    pthread_mutex_lock(&thread_job->targetsLock);

    thread_job->targets[thread_job->current_target].target_size += size;

    pthread_mutex_unlock(&thread_job->targetsLock);
}


/**
 * gets a job to perform (goes through the shared targets and gets the one it should be looking at)
 *
 * @param thread_job     thread_job to look for the job at
 * @return      a path to a file to look throgh
 */
char* job_get(struct thread_job* thread_job)
{
    pthread_mutex_lock(&thread_job->targetsLock);

    if (thread_job->targets[thread_job->current_target].path_num < 1)
    {
        pthread_mutex_unlock(&thread_job->targetsLock);
        return NULL;
    }

    char* tempPath = target_getpath(&thread_job->targets[thread_job->current_target]);

    pthread_mutex_unlock(&thread_job->targetsLock);
    return tempPath;
}

/**
 * Goes to sleep if it can
 *
 * @param thread_job     the thread_job it belongs to
 * @return      void
 */
void job_wait(struct thread_job* thread_job)
{
    pthread_mutex_lock(&thread_job->threadsLock);

    if ((thread_job->active_threads -1) > 0 && thread_job->kill_threads == false)
    {
        thread_job->active_threads--;
        pthread_mutex_unlock(&thread_job->threadsLock);

        sem_wait(&thread_job->sem_threads);
        
        pthread_mutex_lock(&thread_job->threadsLock);
        thread_job->active_threads++;
    }

    pthread_mutex_unlock(&thread_job->threadsLock);
}



/**
 * Checks if the threads should end themselves, or if it should change to another target
 * 
 * @param thread_job     thread_job that is relevant for the thread
 * @return      void
 */
void job_status(struct thread_job* thread_job)
{
    pthread_mutex_lock(&thread_job->threadsLock);
    pthread_mutex_lock(&thread_job->targetsLock);

    int sem_val = haz_semval(&thread_job->sem_threads);
    if (!(sem_val > 0))// not entierly necessary, but I think it'll reduce unnecessary wakes, and lower mutex competition
    {
        // this makes sure that even if there's some thread waking up when it shouldn't, but path_num is zero
        // so this thread for sure knows that there's nobody else who's gonna give out any more paths
        if (thread_job->active_threads == 1 && thread_job->targets[thread_job->current_target].path_num == 0) 
        {
            printf("%d\t%s\n", thread_job->targets[thread_job->current_target].target_size, thread_job->targets[thread_job->current_target].target);
            if ((thread_job->current_target + 1) < thread_job->num_targets)
            {
                thread_job->current_target++;
            }
            else if ((thread_job->current_target + 1) == thread_job->num_targets)
            {
                thread_job->kill_threads = true;
            }
            for (int i = 0; i < (thread_job->num_threads -1); i++)
            {
                sem_post(&thread_job->sem_threads);
            }
        }
    }

    pthread_mutex_unlock(&thread_job->targetsLock);
    pthread_mutex_unlock(&thread_job->threadsLock);
}



/**
 * will check if there are any sleeping threads, and add paths if it can, and wakes threads up that should wake up
 * 
 * @param thread_job     thread_job that is relevant for the thread
 * @param target     target to get paths from
 * @return      void
 */
void job_checkothers(struct thread_job* thread_job, struct target* target)
{
    pthread_mutex_lock(&thread_job->threadsLock);
    pthread_mutex_lock(&thread_job->targetsLock);
    int sem_val = haz_semval(&thread_job->sem_threads);
    if (!(sem_val > 0)) // not entierly necessary, but I think it'll reduce unnecessary wakes, and lower mutex competition
    {
        if (thread_job->active_threads < (thread_job->num_threads))
        {
            int num = 0;
            if (thread_job->targets[thread_job->current_target].path_num < (thread_job->num_threads - thread_job->active_threads))
            {
                num = thread_job->num_threads - thread_job->active_threads;
                while (target->path_num > 1 && num > 0)
                {
                    char* tempPath = target_getpath(target);
                    target_addpath(&thread_job->targets[thread_job->current_target], tempPath);
                    free(tempPath);
                    num--;
                }

            }
            for (int i = 0; i < ((thread_job->num_threads - thread_job->active_threads)-num); i++)
            {
                sem_post(&thread_job->sem_threads);
            }
        }
    }
    pthread_mutex_unlock(&thread_job->targetsLock);
    pthread_mutex_unlock(&thread_job->threadsLock);
}

/**
 * will check if there are any sleeping threads, and add paths if it can, and wakes threads up that should wake up
 * 
 * @param thread_job     thread_job that is relevant for the thread
 * @param path     the path that should be checked
 * @return      the size the job has measured
 */
int job_do(struct thread_job* thread_job, char* path)
{
    if (path == NULL)
    {
        return -1;
    }
    
    struct target* target = haz_malloc(sizeof(struct target));
    target_setup(target, path);
    char* current_path;
    int size = 0;

    while (target->path_num > 0)
    {
        job_checkothers(thread_job, target);
        current_path = target_getpath(target);
        DIR* d;
        if ((d = opendir(current_path)) == NULL) // if it's null we can't read directory, but handle the issue
        {
            size += job_opendir_failed(thread_job, current_path);
        }
        else // else read the directory
        {
            size += job_readdir(thread_job, target, d, current_path);
            if (closedir(d) != 0)
            {
                perror("closedir failed");
                exit(EXIT_FAILURE);
            }
        }
        free(current_path);
    }
    free(target->path_list);
    free(target->target);
    free(target);
    return size;
}

/**
 * handles the priting of error message when the directory couldn't be open, or gets the size if it was because it's a file
 * 
 * @param target     target to write to
 * @param d     DIR* d pointer to read from
 * @param current_path     path to where the directory is
 * @return      the size of the files and the directory itself
 */
int job_readdir(struct thread_job* thread_job, struct target* target, DIR* d, char* current_path)
{
    int size = 0;
    struct dirent* dir;
    while ((dir = safe_readdir(d, &thread_job->exitLock, thread_job->exit_code)) != NULL)
    {
        if (dir->d_type != DT_UNKNOWN)
        {
            if (dir->d_type == DT_DIR)
            {
                if (strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0) // add paths that are not . ..
                {
                    target_addcatpath(target, current_path, dir->d_name);
                }
                if (strcmp(dir->d_name,".") == 0) // add size for self directory
                {
                    size += job_getsize(current_path, dir->d_name); 
                }
            }
            else
            {
                size += job_getsize(current_path, dir->d_name); // add size of file
            }
        }
        else {
            fprintf(stderr,"size of unkown is %d\n", (int)job_getsize(current_path, dir->d_name)); // in case there's an unknown size I don't know what else cold happen
        }
    }
    
    return size;
}

/**
 * handles the priting of error message when the directory couldn't be open, or gets the size if it was because it's a file
 * 
 * @param thread_job     thread_job that is relevant for the thread
 * @param current_path     the path that should be checked
 * @return      the size of the file, or 0 if it couldn't opendir for other reason
 */
int job_opendir_failed(struct thread_job* thread_job, char* current_path)
{
    int size = 0;
    if (errno == EACCES) // google says this is thread safe...
    {
        pthread_mutex_lock(&thread_job->exitLock);
        
        fprintf(stderr,"access denied at %s: ", current_path);
        perror("");
        *thread_job->exit_code = 1;

        pthread_mutex_unlock(&thread_job->exitLock);
    }
    else if (errno == EBADF || errno == EFAULT || errno == ELOOP || errno == ENAMETOOLONG) // lstat will probably crash program from this, but I don't know what to do here??
    {
        pthread_mutex_lock(&thread_job->exitLock);

        fprintf(stderr,"filepath is bad %s error %d:", current_path, errno);
        perror("");
        *thread_job->exit_code = 1;

        pthread_mutex_unlock(&thread_job->exitLock);
    }
    { // still has to measure the size of the folder... but if this folder doesn't exist or something maybe program will crash
        struct stat file;
        haz_lstat(current_path, &file); // will crash on the other errno problem I think, but I don't know how I'm suposed to deal with it, since it's probably an invalid path.
        size += file.st_blocks;
    }
    return size;
}