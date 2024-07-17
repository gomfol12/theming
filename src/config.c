#include "config.h"

static void config_resolve_variables(config_t, command_t *, size_t);

void config_init(config_t *config, char *image_path)
{
    config->cache_path = expand_tilde("~/.cache/theming");
    config->theme_path = expand_tilde("~/.local/share/themes");
    config->icon_theme_path = expand_tilde("~/.local/share/icons");
    config->oomox_icons_command = expand_tilde("/opt/oomox/plugins/icons_numix/change_color.sh");
    config->oomox_theme_name = "oomox-xresources-reverse";
    config->oomox_icon_theme_name = "oomox-xresources-reverse-flat";
    config->image_path = image_path == NULL ? strdup("") : resolve_absolute_path(image_path);
    config->hidpi = false;

    config->generating_commands_size = 3;
    config->generating_commands = malloc(sizeof(command_t) * config->generating_commands_size);
    config->generating_commands[0] = (command_t){
        .command = "betterlockscreen -u %IMAGE_PATH%",
        .async = true,
    };
    config->generating_commands[1] = (command_t){
        .command = "oomox-cli -o %OOMOX_THEME_NAME% -t %THEME_PATH% --hidpi %HIDPI% %CACHE_PATH%/colors-oomox",
        .async = true,
    };
    config->generating_commands[2] = (command_t){
        .command = "%OOMOX_ICONS_COMMAND% -o %OOMOX_ICON_THEME_NAME% -d %ICON_THEME_PATH%/%OOMOX_ICON_THEME_NAME% "
                   "%CACHE_PATH%/colors-oomox",
        .async = true,
    };

    config->reload_commands_size = 4;
    config->reload_commands = malloc(sizeof(command_t) * config->reload_commands_size);
    config->reload_commands[0] = (command_t){
        .command = "xrdb -merge %CACHE_PATH%/colors.Xresources",
    };
    config->reload_commands[1] = (command_t){
        .command = "pidof st | xargs -r kill -SIGUSR1",
    };
    config->reload_commands[2] = (command_t){
        .command = "awesome-client 'awesome.restart()'",
        .ignore_error = true,
    };
    config->reload_commands[3] = (command_t){
        .command = "pywalfox update",
    };

    config_resolve_variables(*config, config->generating_commands, config->generating_commands_size);
    config_resolve_variables(*config, config->reload_commands, config->reload_commands_size);
}

static void config_resolve_variables(config_t config, command_t *commands, size_t commands_size)
{
    const char *variables[] = {"%CACHE_PATH%",       "%THEME_PATH%",
                               "%ICON_THEME_PATH%",  "%OOMOX_ICONS_COMMAND%",
                               "%OOMOX_THEME_NAME%", "%OOMOX_ICON_THEME_NAME%",
                               "%IMAGE_PATH%",       "%HIDPI%"};
    const char *values[] = {config.cache_path,       config.theme_path,
                            config.icon_theme_path,  config.oomox_icons_command,
                            config.oomox_theme_name, config.oomox_icon_theme_name,
                            config.image_path,       config.hidpi ? "true" : "false"};

    for (size_t i = 0; i < commands_size; i++)
    {
        char *new_command = (void *)commands[i].command;
        for (size_t j = 0; j < sizeof(variables) / sizeof(variables[0]); j++)
        {
            char *replaced_command = replace_substring(new_command, variables[j], values[j]);
            if (new_command != commands[i].command)
            {
                free(new_command);
            }
            new_command = replaced_command;
        }
        commands[i].command = new_command;
    }
}

void config_free(config_t *config)
{
    free(config->cache_path);
    free(config->theme_path);
    free(config->icon_theme_path);
    free(config->oomox_icons_command);
    free(config->image_path);
    for (size_t i = 0; i < config->generating_commands_size; i++)
    {
        free((void *)config->generating_commands[i].command);
    }
    for (size_t i = 0; i < config->reload_commands_size; i++)
    {
        free((void *)config->reload_commands[i].command);
    }
    free(config->generating_commands);
    free(config->reload_commands);
}
