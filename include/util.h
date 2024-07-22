#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

void die(const char *, ...) __attribute__((format(printf, 1, 2)));
void *safe_malloc(size_t);
void *safe_realloc(void *, size_t);
void *safe_calloc(size_t, size_t);
char *format_string(const char *, ...) __attribute__((format(printf, 1, 2)));
char *expand_tilde(const char *);
void mkdir_p(const char *);
void read_file(FILE *, char *, size_t);
void exec_command(const char *, bool);
void exec_command_format(bool, const char *, ...) __attribute__((format(printf, 2, 3)));
void exec_command_with_output(const char *, char *, size_t, bool);
void exec_command_with_output_format(char *, size_t, bool, const char *, ...) __attribute__((format(printf, 4, 5)));
char *resolve_absolute_path(const char *);
int rmrf(char *);
int check_directory(const char *);
void make_symlink(const char *, const char *);
char *replace_substring(const char *, const char *, const char *);
