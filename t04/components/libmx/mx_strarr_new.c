#include "libmx.h"

char **mx_strarr_new(int size) {
	int len = size + 1;
	char **data = (char **)malloc(len * sizeof(char *));
    if (data == NULL) exit(1);
    for (int i = 0; i < 61; ++i) data[i] = NULL;
    return data;
}