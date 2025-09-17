#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char **environ; // Environment variable array (NULL-terminated)

int init_env()
{
    // Start with an empty environment
    environ = (char **)malloc(sizeof(char *));
    if (!environ) return -1; // Allocation failure
    environ[0] = NULL;
    return 0;
}

// List of environment variables
int setenv(const char *__name, const char *__value, int __replace)
{
    // Validate input
    if (!__name || !*__name || strchr(__name, '=') || !__value) {
        return -1; // Invalid arguments
    }

    // Check if variable already exists
    for (char **env = environ; *env; ++env) {
        if (strncmp(*env, __name, strlen(__name)) == 0 && (*env)[strlen(__name)] == '=') {
            if (__replace) {
                // Replace existing value
                size_t new_len = strlen(__name) + 1 + strlen(__value) + 1;
                char *new_entry = (char *)malloc(new_len);
                if (!new_entry) return -1; // Allocation failure
                snprintf(new_entry, new_len, "%s=%s", __name, __value);
                free(*env);
                *env = new_entry;
            }
            return 0; // Success
        }
    }

    // Variable does not exist; add new entry
    size_t new_len = strlen(__name) + 1 + strlen(__value) + 1;
    char *new_entry = (char *)malloc(new_len);
    if (!new_entry) return -1; // Allocation failure
    snprintf(new_entry, new_len, "%s=%s", __name, __value);

    // Count current environment variables
    size_t env_count = 0;
    while (environ[env_count]) ++env_count;

    // Allocate new environment array with space for new entry and NULL terminator
    char **new_environ = (char **)malloc((env_count + 2) * sizeof(char *));
    if (!new_environ) {
        free(new_entry);
        return -1; // Allocation failure
    }

    // Copy existing entries and add new entry
    for (size_t i = 0; i < env_count; ++i) {
        new_environ[i] = environ[i];
    }
    new_environ[env_count] = new_entry;
    new_environ[env_count + 1] = NULL;

    // Replace old environment array
    free(environ);
    environ = new_environ;

    return 0; // Success
}

char *getenv(const char *name)
{
    if (!name || !*name) return NULL;
    size_t nlen = strlen(name);
    if (!environ) return NULL;
    for (char **env = environ; *env; ++env) {
        if (strncmp(*env, name, nlen) == 0 && (*env)[nlen] == '=') {
            return *env + nlen + 1;
        }
    }
    return NULL;
}

int unsetenv(const char *name)
{
    if (!name || !*name) return -1;
    size_t nlen = strlen(name);
    if (!environ) return 0;

    size_t env_count = 0;
    while (environ[env_count]) ++env_count;

    for (size_t i = 0; i < env_count; ++i) {
        if (strncmp(environ[i], name, nlen) == 0 && environ[i][nlen] == '=') {
            // Found the variable to remove
            free(environ[i]);
            // Shift remaining entries down
            for (size_t j = i; j < env_count - 1; ++j) {
                environ[j] = environ[j + 1];
            }
            environ[env_count - 1] = NULL;
            return 0; // Success
        }
    }
    return 0; // Variable not found, nothing to do
}

int putenv(char *__string)
{
    if (!__string || !*__string) return -1;
    char *eq = strchr(__string, '=');
    if (!eq) return -1; // Invalid format
    size_t name_len = eq - __string;
    if (name_len == 0) return -1; // Invalid name

    // Check if variable already exists
    for (char **env = environ; *env; ++env) {
        if (strncmp(*env, __string, name_len) == 0 && (*env)[name_len] == '=') {
            // Replace existing entry
            free(*env);
            *env = __string;
            return 0; // Success
        }
    }

    // Variable does not exist; add new entry
    size_t env_count = 0;
    while (environ[env_count]) ++env_count;

    char **new_environ = (char **)malloc((env_count + 2) * sizeof(char *));
    if (!new_environ) return -1; // Allocation failure

    for (size_t i = 0; i < env_count; ++i) {
        new_environ[i] = environ[i];
    }
    new_environ[env_count] = __string;
    new_environ[env_count + 1] = NULL;

    free(environ);
    environ = new_environ;

    return 0; // Success
}
