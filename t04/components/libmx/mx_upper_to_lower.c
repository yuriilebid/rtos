#include "libmx.h"

char *mx_upper_to_lower(char *str) {
    int len = strlen(str) + 1;
    char *new_str = mx_strnew(len);

    for (int i = 0; str[i]; ++i) {
        if(str[i] >= 65 && str[i] <= 90)
            new_str[i] = str[i] + 32;
        else
            new_str[i] = str[i];
    }
    return new_str;
}

