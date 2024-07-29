#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED 1
#include <ftw.h>

static char *format_string_internal(const char *, va_list) __attribute__((format(printf, 1, 0)));
static int unlink_cb(const char *, const struct stat *, int, struct FTW *);

void die(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    if (fmt[strlen(fmt) - 1] == ':')
    {
        fputs(" ", stderr);
        perror(NULL);
    }
    else
    {
        fprintf(stderr, "\n");
    }
    va_end(args);

    exit(EXIT_FAILURE);
}

void *safe_malloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
    {
        die("malloc failed:");
    }

    return p;
}

void *safe_realloc(void *p, size_t size)
{
    void *new_p = realloc(p, size);
    if (new_p == NULL)
    {
        die("realloc failed:");
    }

    return new_p;
}

void *safe_calloc(size_t count, size_t size)
{
    void *p = calloc(count, size);
    if (p == NULL)
    {
        die("calloc failed:");
    }

    return p;
}

char *format_string(const char *format, ...)
{
    va_list args, args_copy;
    va_start(args, format);

    va_copy(args_copy, args);
    ssize_t size = vsnprintf(NULL, 0, format, args);
    if (size < 0)
    {
        die("vsnprintf failed:");
    }

    char *buffer = safe_malloc((size_t)(size + 1));
    int result = vsnprintf(buffer, (size_t)(size + 1), format, args_copy);
    if (result < 0)
    {
        die("vsnprintf failed:");
    }

    va_end(args);
    va_end(args_copy);
    return buffer;
}

static char *format_string_internal(const char *format, va_list args)
{
    va_list args_copy;

    va_copy(args_copy, args);
    ssize_t size = vsnprintf(NULL, 0, format, args);
    if (size < 0)
    {
        die("vsnprintf failed:");
    }

    char *buffer = safe_malloc((size_t)(size + 1));
    int result = vsnprintf(buffer, (size_t)(size + 1), format, args_copy);
    if (result < 0)
    {
        die("vsnprintf failed:");
    }

    va_end(args_copy);
    return buffer;
}

char *expand_tilde(const char *path)
{
    if (path[0] != '~')
    {
        return strdup(path);
    }

    const char *home = getenv("HOME");
    if (home == NULL)
    {
        die("HOME not set");
    }

    return format_string("%s%s", home, path + 1);
}

void mkdir_p(const char *path)
{
    char *expanded_path = expand_tilde(path);

    struct stat st = {0};
    if (stat(expanded_path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        free(expanded_path);
        return;
    }

    char *sep = strrchr(expanded_path, '/');

    if (sep != NULL)
    {
        *sep = 0;
        mkdir_p(expanded_path);
        *sep = '/';
    }

    if (mkdir(expanded_path, 0755) && errno != EEXIST)
    {
        die("mkdir failed:");
    }

    free(expanded_path);
}

void read_file(FILE *file, char *output, size_t buffer_size)
{
    size_t len = buffer_size;
    size_t used = 0;
    output[0] = '\0';

    char *buffer = safe_malloc(buffer_size);
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        size_t buffer_len = strlen(buffer);
        if (used + buffer_len + 1 > len)
        {
            len += buffer_size;
            output = safe_realloc(output, len);
        }
        strcpy(output + used, buffer);
        used += buffer_len;
    }
    free(buffer);
}

void exec_command(const char *command, bool ignore_error)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        die("fork failed:");
    }

    if (pid == 0)
    {
        // open /dev/null and redirect stdout and stderr to it
        int fd = open("/dev/null", O_WRONLY);
        if (fd == -1)
        {
            die("open /dev/null failed");
        }
        if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1)
        {
            close(fd);
            die("dup2 failed");
        }
        close(fd);

        execv("/bin/sh", (char *[]){"sh", "-c", (char *)command, NULL});
        die("execv failed:");
    }

    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        die("waitpid failed:");
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    {
        if (!ignore_error)
        {
            die("%s failed with exit status %d", command, WEXITSTATUS(status));
        }
        else
        {
            fprintf(stderr, "%s failed with exit status %d\n", command, WEXITSTATUS(status));
        }
    }
    if (WIFSIGNALED(status))
    {
        die("%s terminated by signal %d", command, WTERMSIG(status));
    }
}

void exec_command_format(bool ignore_error, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *command = format_string_internal(format, args);
    va_end(args);

    exec_command(command, ignore_error);
    free(command);
}

void exec_command_with_output(const char *command, char *output, size_t buffer_size, bool ignore_error)
{
    int pfd[2];
    pid_t pid;

    if (pipe(pfd) == -1)
    {
        die("pipe failed:");
    }

    pid = fork();
    if (pid == -1)
    {
        die("fork failed:");
    }

    if (pid == 0)
    {
        // child process

        close(pfd[0]); // close read end
        if (dup2(pfd[1], STDOUT_FILENO) == -1)
        {
            die("dup2 failed:");
        }
        close(pfd[1]); // close original write end. Not needed anymore

        execv("/bin/sh", (char *[]){"sh", "-c", (char *)command, NULL});
        die("execv failed:");
    }

    // parent process

    close(pfd[1]); // close write end
    FILE *file = fdopen(pfd[0], "r");
    if (!file)
    {
        die("fdopen failed:");
    }

    read_file(file, output, buffer_size);

    fclose(file);

    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        die("waitpid failed:");
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    {
        if (!ignore_error)
        {
            die("%s failed with exit status %d", command, WEXITSTATUS(status));
        }
        else
        {
            fprintf(stderr, "%s failed with exit status %d\n", command, WEXITSTATUS(status));
        }
    }
    if (WIFSIGNALED(status))
    {
        die("%s terminated by signal %d", command, WTERMSIG(status));
    }
}

void exec_command_with_output_format(char *output, size_t buffer_size, bool ignore_error, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *command = format_string_internal(format, args);
    va_end(args);

    exec_command_with_output(command, output, buffer_size, ignore_error);
    free(command);
}

char *resolve_absolute_path(const char *path)
{
    char *resolved_path = realpath(path, NULL);

    if (resolved_path == NULL)
    {
        die("realpath failed:");
    }

    return resolved_path;
}

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        die("remove failed for %s:", fpath);

    return rv;
}

int rmrf(char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

int check_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
    {
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        return -1;
    }

    return 0;
}

void make_symlink(const char *from, const char *to)
{
    char *expanded_from = expand_tilde(from);
    char *expanded_to = expand_tilde(to);

    if (symlink(expanded_from, expanded_to) == -1)
    {
        die("symlink failed:");
    }

    free(expanded_from);
    free(expanded_to);
}

char *replace_substring(const char *str, const char *target, const char *replacement)
{
    char *result;
    size_t i, count = 0;
    size_t target_len = strlen(target);
    size_t replacement_len = strlen(replacement);

    // count occurrences of target
    for (i = 0; str[i] != '\0'; i++)
    {
        if (strstr(&str[i], target) == &str[i])
        {
            count++;
            i += target_len - 1;
        }
    }

    // allocate memory for the new string (i end of old string)
    result = (char *)malloc(i + count * (replacement_len - target_len) + 1);
    if (result == NULL)
    {
        die("malloc failed:");
    }

    i = 0;
    while (*str)
    {
        if (strstr(str, target) == str)
        {
            strcpy(&result[i], replacement);
            i += replacement_len;
            str += target_len;
        }
        else
        {
            result[i++] = *str++;
        }
    }
    result[i] = '\0';
    return result;
}
