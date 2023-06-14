#include "file.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>

u_int64_t get_file_size(int fd)
{
    struct stat st = {0};

    if (fstat(fd, &st) == -1)
    {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    return st.st_size;
}

u_int64_t calculate_cell_size(u_int64_t fnlength, u_int64_t fsize)
{
    return fnlength + 1 + fsize;
}
void clean_directory(char *dir_path)
{
    DIR *dir = opendir(dir_path);
    struct dirent *dir_stream;

    char file_path[512];
    while ((dir_stream = readdir(dir)) != NULL)
    {
        sprintf(file_path, "%s/%s", dir_path, dir_stream->d_name);
        remove(file_path);
    }
}