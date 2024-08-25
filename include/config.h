#pragma once
#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    char *command;
    bool ignore_error;
    bool async;
    bool restart;
    bool initial;
} command_t;

typedef struct
{
    char *cache_path;
    char *theme_path;
    char *icon_theme_path;
    char *oomox_icons_command;
    char *oomox_theme_name;
    char *oomox_icon_theme_name;
    char *image_path;
    command_t *generating_commands;
    size_t generating_commands_size;
    command_t *reload_commands;
    size_t reload_commands_size;
    bool hidpi;
    bool send_notification;
} config_t;

void config_init(config_t *);
void config_free(config_t *);
