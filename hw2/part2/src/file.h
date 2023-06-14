#include <sys/types.h>

u_int64_t get_file_size(int fd);
u_int64_t calculate_cell_size(u_int64_t fnlength, u_int64_t fsize);
void clean_directory(char* path);