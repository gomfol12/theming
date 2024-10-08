#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED 1
#include <dirent.h>
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

void exec_command(const char *command, bool ignore_error, char *output, size_t buffer_size)
{
    int pfd[2];
    pid_t pid;

    if (output != NULL)
    {
        if (pipe(pfd) == -1)
        {
            die("pipe failed:");
        }
    }

    pid = fork();
    if (pid == -1)
    {
        die("fork failed:");
    }

    if (pid == 0)
    {
        // child process

        if (output != NULL)
        {
            close(pfd[0]); // close read end
            if (dup2(pfd[1], STDOUT_FILENO) == -1)
            {
                die("dup2 failed:");
            }
            close(pfd[1]); // close original write end. Not needed anymore
        }
        else
        {
            // redirect standard input, output and error to /dev/null
            if (freopen("/dev/null", "r", stdin) == NULL)
            {
                die("freopen failed");
            }
            if (freopen("/dev/null", "w", stdout) == NULL)
            {
                die("freopen failed");
            }
            if (freopen("/dev/null", "w", stderr) == NULL)
            {
                die("freopen failed");
            }
        }

        execv("/bin/sh", (char *[]){"sh", "-c", (char *)command, NULL});
        die("execv failed:");
    }

    // parent process
    if (output != NULL)
    {
        close(pfd[1]); // close write end
        FILE *file = fdopen(pfd[0], "r");
        if (!file)
        {
            die("fdopen failed:");
        }

        read_file(file, output, buffer_size);

        fclose(file);
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

void exec_command_format(bool ignore_error, char *output, size_t buffer_size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *command = format_string_internal(format, args);
    va_end(args);

    exec_command(command, ignore_error, output, buffer_size);
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

int check_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
    {
        return -1;
    }

    if (!S_ISREG(st.st_mode))
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

pid_t find_pid_by_name(const char *name)
{
    DIR *dir;
    struct dirent *ent; // dir entry

    if (!(dir = opendir("/proc")))
    {
        die("opendir failed:");
    }

    while ((ent = readdir(dir)) != NULL)
    {
        if (!isdigit(*ent->d_name))
        {
            continue;
        }

        char *proc_comm = format_string("/proc/%s/comm", ent->d_name);
        FILE *file = fopen(proc_comm, "r");
        free(proc_comm);
        if (!file)
        {
            continue;
        }

        char *output = safe_malloc(512);
        read_file(file, output, 512);
        output[strcspn(output, "\n")] = 0; // remove newline character
        fclose(file);

        if (strcmp(output, name) == 0)
        {
            free(output);
            closedir(dir);
            return (pid_t)strtol(ent->d_name, NULL, 10);
        }
        free(output);
    }

    closedir(dir);
    return -1;
}

void exec_command_and_disown(const char *command)
{
    pid_t pid;
    pid_t sid;

    pid = fork();
    if (pid == -1)
    {
        die("fork failed");
    }

    if (pid == 0)
    {
        // child process

        sid = setsid(); // create a new session and become the session leader
        if (sid == -1)
        {
            die("setsid failed");
        }

        pid = fork();
        if (pid == -1)
        {
            die("fork failed");
        }

        if (pid == 0)
        {
            // grandchild process

            // redirect standard input, output and error to /dev/null
            if (freopen("/dev/null", "r", stdin) == NULL)
            {
                die("freopen failed");
            }
            if (freopen("/dev/null", "w", stdout) == NULL)
            {
                die("freopen failed");
            }
            if (freopen("/dev/null", "w", stderr) == NULL)
            {
                die("freopen failed");
            }

            execv("/bin/sh", (char *[]){"sh", "-c", (char *)command, NULL});
            die("execv failed");
        }

        // parent process

        exit(EXIT_SUCCESS);
    }

    // parent process

    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        die("waitpid failed:");
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
    }
    else
    {
        die("failed to spawn orphan");
    }
}

int cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do
        {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}
