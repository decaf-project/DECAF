#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int setenv(const char *name, const char *value, int overwrite)
{
    int result = 0;
    if (overwrite || !getenv(name)) {
        size_t length = strlen(name) + strlen(value) + 2;
        char *string = malloc(length);
        snprintf(string, length, "%s=%s", name, value);
        result = putenv(string);
    }
    return result;
}

#endif
