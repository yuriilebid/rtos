#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


char *mx_string_copy(char *str);
char *mx_strnew(int size);
char **mx_strarr_new(int size);
char *mx_upper_to_lower(char *str);
int  mx_strarr_len(char **arr);