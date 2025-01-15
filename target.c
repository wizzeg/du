#include "target.h"

/**
 * hazardous strdup, cannot recover upon failure
 *
 * @param string     the string to duplicate
 * @return      void* to allocation, and kills program if something goes wrong
 */
void *haz_strdup(char *string)
{
    char *temp = strdup(string);
    if (temp == NULL)
    {
        perror("failed to dup string");
        exit(EXIT_FAILURE);
    }
    return temp;
}

/**
 * hazardous malloc, cannot recover upon failure
 *
 * @param size     the size to be allocated
 * @return      void* to allocation, and kills program if something goes wrong
 */
void *haz_malloc(size_t size)
{
    void *temp = malloc(size);
    if (temp == NULL)
    {
        fprintf(stderr, "failed to allocate space");
        exit(EXIT_FAILURE);
    }
    return temp;
}

/**
 * hazardous realloc, cannot recover upon failure
 *
 * @param source     pointer to the source allocation
 * @param size     the size to be allocated
 * @return      void* to allocation, but kills program if something goes wrong
 */
void *haz_realloc(void *source, size_t size)
{
    void *temp = realloc(source, size);
    if (temp == NULL)
    {
        fprintf(stderr, "failed to allocate space");
        exit(EXIT_FAILURE);
    }
    return temp;
}

/**
 * concatenates two strings in a path life fashion
 *
 * @param destination     the path
 * @param appendee     the file name
 * @return      the pointer to the allocated destination file
 */
char *target_appendstr(char *destination, const char *appendee)
{
    int length = (strlen((const char *)destination) * sizeof(char) + strlen(appendee) * sizeof(char) + 2 * sizeof(char));
    destination = haz_realloc(destination, (size_t)length);
    destination = strncat(destination, "/", (strlen((const char *)destination) * sizeof(char) + strlen((const char *)"/") * sizeof(char)));
    destination = strncat(destination, appendee, (strlen((const char *)destination) * sizeof(char) + strlen((const char *)appendee) * sizeof(char)));
    return destination;
}


/**
 * extends the path list for the target
 *
 * @param target     target to extend
 * @return      void
 */
void target_extend_pathlist(struct target *target)
{
    target->path_size += target->path_size;
    target->path_list = haz_realloc(target->path_list, (size_t)(sizeof(char *) * target->path_size));
}


/**
 * adds a copy of the path to the target
 *
 * @param target     target to add to
 * @param path     the size to be allocated
 * @return      void
 */
void target_addpath(struct target *target, char *path)
{
    target->path_head++;
    if (target->path_head >= target->path_size)
    {
        target_extend_pathlist(target);
    }
    char *tempPath = haz_strdup(path);
    target->path_list[target->path_head] = tempPath;
    target->path_num++;
}


/**
 * adds a copy of the path+filename to the target
 *
 * @param target     target to add to
 * @param path     the origin path
 * @param d_path     the file/directory name
 * @return      void
 */
void target_addcatpath(struct target *target, char *path, const char *d_path)
{
    target->path_head++;
    if (target->path_head >= target->path_size)
    {
        target_extend_pathlist(target);
    }
    char *tempPath = haz_strdup(path);
    tempPath = target_appendstr(tempPath, d_path);
    target->path_list[target->path_head] = tempPath;
    target->path_num++;
}

/**
 * gets a path from the target
 *
 * @param target     target to get path from
 * @return      a pointer to the target
 */
char *target_getpath(struct target *target)
{
    if (target->path_num < 1)
    {
        return NULL;
    }
    char *tempPath = haz_strdup(target->path_list[target->path_head]);
    free(target->path_list[target->path_head]);
    target->path_num--;
    target->path_head--;
    return tempPath;
}

/**
 * setsup a new target
 *
 * @param target     target target
 * @param path     the starting path
 * @return      void
 */
void target_setup(struct target *target, char *path)
{
    target->path_size = STARTSIZE;
    target->path_list = haz_malloc((size_t)(sizeof(char *) * target->path_size));
    target->path_head = 0;
    target->path_num = 1;

    target->target_size = 0;

    target->path_list[target->path_head] = haz_strdup(path);
    target->target = haz_strdup(path);
}