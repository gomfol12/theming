#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct
{
    const char *command;
    bool ignore_error;
    bool async;
} command_t;

typedef struct
{
    char *cache_path;
    char *theme_path;
    char *icon_theme_path;
    char *oomox_icons_command;
    const char *oomox_theme_name;
    const char *oomox_icon_theme_name;
    char *image_path;
    command_t *generating_commands;
    size_t generating_commands_size;
    command_t *reload_commands;
    size_t reload_commands_size;
    bool hidpi;
} config_t;

void config_init(config_t *, char *);
void config_free(config_t *);
