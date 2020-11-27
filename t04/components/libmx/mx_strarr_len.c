#include "libmx.h"

int mx_strarr_len(char **arr) {
    if (arr == NULL) return -1;
    int i = 0;
    while(arr[i]) i++;
    return i;
}