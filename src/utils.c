#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* copy_string(const char* s)
{
    int size = strlen(s) + 1;
    char *new_string = malloc(size);
    if (new_string == NULL) {
        perror("out of memory");
        abort();
    }
    strcpy(new_string, s);
    return new_string;
}

bool is_dir_staff(const char *s) {
    return strcmp(s, ".") == 0 || strcmp(s, "..") == 0;
}