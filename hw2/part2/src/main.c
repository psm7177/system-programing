#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include "file.h"

#define COMMAND_PACK 1
#define COMMAND_UNPACK 2
#define COMMAND_ADD 3
#define COMMAND_DEL 4
#define COMMAND_LIST 5

#define COUNT_LEN sizeof(u_int32_t)
#define SIZE_LEN sizeof(u_int64_t)

int parse_command(char *str)
{
    if (strcmp(str, "pack") == 0)
    {
        return COMMAND_PACK;
    }
    else if (strcmp(str, "unpack") == 0)
    {
        return COMMAND_UNPACK;
    }
    else if (strcmp(str, "add") == 0)
    {
        return COMMAND_ADD;
    }
    else if (strcmp(str, "del") == 0)
    {
        return COMMAND_DEL;
    }
    else if (strcmp(str, "list") == 0)
    {
        return COMMAND_LIST;
    }
    else
    {
        fprintf(stderr, "%s is not supported command.\n", str);
        exit(1);
    }
}
void concate_path_and_file(char *dest, char *path, char *filename)
{
    sprintf(dest, "%s/%s", path, filename);
    return;
}

void pack(char *arch, char *src_dir)
{
    char filepath[128];

    DIR *dir_stream;

    struct dirent *dir_entry;

    dir_stream = opendir(src_dir);

    if (dir_stream == NULL)
    {
        fprintf(stderr, "%s directory does not exist.\n", src_dir);
        return;
    }

    u_int32_t count = 0;

    int fd_arch = open(arch, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd_arch < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    lseek(fd_arch, COUNT_LEN, SEEK_SET);
    while (1)
    {
        dir_entry = readdir(dir_stream);
        if (dir_entry == NULL)
        {
            break;
        }
        if (dir_entry->d_type != DT_REG)
        {
            continue;
        }

        concate_path_and_file(filepath, src_dir, dir_entry->d_name);
        int fd_entry = open(filepath, O_RDONLY);

        if (fd_entry < 0)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }

        u_int64_t fn_len = strlen(dir_entry->d_name);
        u_int64_t file_size = get_file_size(fd_entry);
        u_int64_t cell_size = calculate_cell_size(fn_len, file_size);

        write(fd_arch, &cell_size, sizeof(u_int64_t));
        write(fd_arch, dir_entry->d_name, fn_len + 1);

        int p_entry = lseek(fd_entry, 0, SEEK_CUR);
        int p_arch = lseek(fd_arch, 0, SEEK_CUR);

        int ret = copy_file_range(fd_entry, NULL, fd_arch, NULL, file_size, 0);
        if (ret == -1)
        {
            perror("copy_file_range");
            exit(EXIT_FAILURE);
        }
        count += 1;
        close(fd_entry);
    }
    lseek(fd_arch, 0, SEEK_SET);
    write(fd_arch, &count, sizeof(u_int32_t));
    printf("%d file(s) archived\n", count);
    close(fd_arch);
}
void unpack(char *arch, char *dest_path)
{
    u_int32_t count = 0;
    u_int64_t offset = COUNT_LEN;
    u_int64_t size;
    u_int64_t file_size;
    u_int64_t name_len;

    int fd_arch = open(arch, O_RDONLY);
    if (fd_arch < 0)
    {
        fprintf(stderr, "For %s\n", arch);
        perror("open");
        exit(EXIT_FAILURE);
    }
    read(fd_arch, &count, COUNT_LEN);

    char filepath[128];
    char name[32];

    struct stat st = {0};
    if (stat(dest_path, &st) == -1)
    {
        int ret = mkdir(dest_path, 0775);
        if (ret == -1)
        {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }
    clean_directory(dest_path);

    for (u_int32_t i = 0; i < count; i++)
    {
        read(fd_arch, &size, SIZE_LEN);
        read(fd_arch, name, sizeof(name));
        concate_path_and_file(filepath, dest_path, name);

        int fd_unpack = open(filepath, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if (fd_unpack < 0)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }
        name_len = strlen(name);
        file_size = size - name_len - 1;

        offset += SIZE_LEN + name_len + 1;
        int ret = copy_file_range(fd_arch, &offset, fd_unpack, NULL, file_size, 0);
        if (ret == -1)
        {
            perror("copy_file_range");
            exit(EXIT_FAILURE);
        }
        printf("unpacked %s\n", name);

        lseek(fd_arch, offset, SEEK_SET);
        close(fd_unpack);
    }
    close(fd_arch);
}

void add(char *arch, char *add_file)
{
    u_int32_t count = 0;
    char *bsname = basename(add_file);
    int fd_arch = open(arch, O_RDWR);
    if (fd_arch < 0)
    {
        fprintf(stderr, "For %s\n", arch);
        perror("open");
        exit(EXIT_FAILURE);
    }
    int fd_add = open(add_file, O_RDONLY);

    if (fd_add < 0)
    {
        fprintf(stderr, "For %s\n", add_file);
        perror("open");
        exit(EXIT_FAILURE);
    }

    int ret = read(fd_arch, &count, COUNT_LEN);
    if (ret == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    u_int64_t offset = COUNT_LEN;
    u_int64_t size;

    u_int64_t name_len;
    char name[32];

    for (u_int32_t i = 0; i < count; i++)
    {
        read(fd_arch, &size, SIZE_LEN);
        read(fd_arch, name, sizeof(name));

        name_len = strlen(name);
        u_int64_t file_size = size - name_len - 1;
        if (strcmp(bsname, name) == 0)
        {
            fprintf(stderr, "%s already exists in achieve file\n", bsname);
            exit(EXIT_FAILURE);
        }
        offset += size + SIZE_LEN;
        lseek(fd_arch, offset, SEEK_SET);
    }

    lseek(fd_arch, 0, SEEK_END);

    u_int64_t add_f_size = get_file_size(fd_add);
    u_int64_t arch_f_size = get_file_size(fd_arch);

    u_int64_t add_bsn_length = strlen(bsname);
    size = calculate_cell_size(add_bsn_length, add_f_size);

    ret = write(fd_arch, &size, SIZE_LEN);
    if (ret == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    ret = write(fd_arch, bsname, add_bsn_length + 1);
    if (ret == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    ret = copy_file_range(fd_add, NULL, fd_arch, NULL, add_bsn_length, 0);
    if (ret == -1)
    {
        perror("copy_file_range");
        exit(EXIT_FAILURE);
    }

    count += 1;
    lseek(fd_arch, 0, SEEK_SET);
    write(fd_arch, &count, COUNT_LEN);
    printf("1 file added.\n");
    close(fd_arch);
    close(fd_add);
}

void del(char *arch, char *delete_filename)
{
    u_int32_t count = 0;
    u_int64_t offset = COUNT_LEN;
    u_int64_t size;
    int fd_temp = open("./", O_RDWR | O_TMPFILE | O_TRUNC, 0777);
    int fd_arch = open(arch, O_RDWR);
    read(fd_arch, &count, COUNT_LEN);

    u_int64_t file_size = get_file_size(fd_arch);

    char name[32];

    for (u_int32_t i = 0; i < count; i++)
    {
        read(fd_arch, &size, SIZE_LEN);
        read(fd_arch, name, sizeof(name));
        if (strcmp(name, delete_filename) == 0)
        {
            u_int64_t backup_offset = offset + size + SIZE_LEN;
            u_int64_t backup_size = file_size - backup_offset;

            int ret = copy_file_range(fd_arch, &backup_offset, fd_temp, NULL, backup_size, 0);
            if (ret == -1)
            {
                perror("copy_file_range");
                exit(EXIT_FAILURE);
            }
            printf("backup_size: %d\n", backup_size);
            lseek(fd_temp, 0, SEEK_SET);
            ret = copy_file_range(fd_temp, NULL, fd_arch, &offset, backup_size, 0);
            if (ret == -1)
            {
                perror("copy_file_range");
                exit(EXIT_FAILURE);
            }
            count -= 1;
            ftruncate(fd_arch, offset);
            printf("1 file deleted.\n");
            lseek(fd_arch, 0, SEEK_SET);
            write(fd_arch, &count, COUNT_LEN);
            close(fd_temp);
            close(fd_arch);
            return;
        }

        offset += size + SIZE_LEN;
        lseek(fd_arch, offset, SEEK_SET);
    }

    fprintf(stderr, "No such file exists.\n");
    exit(EXIT_FAILURE);
}

void list(char *arch)
{
    u_int32_t count = 0;
    u_int64_t offset = COUNT_LEN;
    u_int64_t size;

    u_int64_t name_len;
    int fd_arch = open(arch, O_RDONLY);
    if (fd_arch < 0)
    {
        fprintf(stderr, "For %s\n", arch);
        perror("open");
        exit(EXIT_FAILURE);
    }

    read(fd_arch, &count, COUNT_LEN);

    printf("%d file(s) exist.\n", count);
    char name[32];

    for (u_int32_t i = 0; i < count; i++)
    {
        read(fd_arch, &size, SIZE_LEN);
        read(fd_arch, name, sizeof(name));

        name_len = strlen(name);
        u_int64_t file_size = size - name_len - 1;
        if (size != 1)
        {
            printf("%s %ld bytes\n", name, file_size);
        }
        else
        {
            printf("%s %ld byte\n", name, file_size);
        }
        offset += size + SIZE_LEN;
        lseek(fd_arch, offset, SEEK_SET);
    }
}
int main(int argc, char **argv)
{
    mode_t old_mask = umask(0);
    if (argc < 2)
    {
        // mannual
        exit(1);
    }

    int command = parse_command(argv[1]);
    switch (command)
    {
    case COMMAND_PACK:
        pack(argv[2], argv[3]);
        break;
    case COMMAND_LIST:
        list(argv[2]);
        break;
    case COMMAND_UNPACK:
        unpack(argv[2], argv[3]);
        break;
    case COMMAND_ADD:
        add(argv[2], argv[3]);
        break;
    case COMMAND_DEL:
        del(argv[2], argv[3]);
        break;
    default:
        break;
    }
    umask(old_mask);
    return 0;
}