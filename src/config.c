#include "config.h"

#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

static void config_resolve_variables(config_t, command_t *, size_t);
static struct json_object *json_find_by_name_safe(struct json_object *, json_type, const char *);
static struct json_object *json_find_by_name(struct json_object *, json_type, const char *);

static struct json_object *json_find_by_name_safe(struct json_object *jobj, json_type jtype, const char *name)
{
    struct json_object *tmp;

    if (!json_object_object_get_ex(jobj, name, &tmp))
    {
        die("config: %s not found", name);
    }

    if (!json_object_is_type(tmp, jtype))
    {
        if (jtype == json_type_string)
        {
            die("config: %s is not a string", name);
        }
        else if (jtype == json_type_boolean)
        {
            die("config: %s is not a boolean", name);
        }
        else if (jtype == json_type_array)
        {
            die("config: %s is not an array", name);
        }
        else
        {
            die("config: %s is not a valid type", name);
        }
    }

    return tmp;
}

static struct json_object *json_find_by_name(struct json_object *jobj, json_type jtype, const char *name)
{
    struct json_object *tmp;

    json_object_object_get_ex(jobj, name, &tmp);

    if (tmp == NULL)
    {
        return NULL;
    }

    if (!json_object_is_type(tmp, jtype))
    {
        if (jtype == json_type_string)
        {
            die("config: %s is not a string", name);
        }
        else if (jtype == json_type_boolean)
        {
            die("config: %s is not a boolean", name);
        }
        else if (jtype == json_type_array)
        {
            die("config: %s is not an array", name);
        }
        else
        {
            die("config: %s is not a valid type", name);
        }
    }

    return tmp;
}

void config_init(config_t *config, char *image_path)
{
    // read config file
    char *config_file_path = format_string("%s/theming/config.json", getenv("XDG_CONFIG_HOME"));
    FILE *file = fopen(config_file_path, "r");
    if (!file)
    {
        die("fopen failed:");
    }
    free(config_file_path);
    char *output = safe_malloc(BUFSIZ);
    read_file(file, output, BUFSIZ);
    fclose(file);

    json_object *jobj = json_tokener_parse(output);
    if (jobj == NULL)
    {
        die("json_tokener_parse failed. Possibly invalid JSON file.");
    }
    free(output);

    config->cache_path =
        expand_tilde(json_object_get_string(json_find_by_name_safe(jobj, json_type_string, "cache_path")));
    config->theme_path =
        expand_tilde(json_object_get_string(json_find_by_name_safe(jobj, json_type_string, "theme_path")));
    config->icon_theme_path =
        expand_tilde(json_object_get_string(json_find_by_name_safe(jobj, json_type_string, "icon_theme_path")));
    config->oomox_icons_command =
        expand_tilde(json_object_get_string(json_find_by_name_safe(jobj, json_type_string, "oomox_icons_command")));
    config->oomox_theme_name =
        strdup(json_object_get_string(json_find_by_name_safe(jobj, json_type_string, "oomox_theme_name")));
    config->oomox_icon_theme_name =
        strdup(json_object_get_string(json_find_by_name_safe(jobj, json_type_string, "oomox_icon_theme_name")));
    config->image_path = resolve_absolute_path(image_path);
    config->hidpi = json_object_get_boolean(json_find_by_name_safe(jobj, json_type_boolean, "hidpi"));
    config->send_notification =
        json_object_get_boolean(json_find_by_name_safe(jobj, json_type_boolean, "send_notification"));

    // generating commands
    json_object *json_generating_commands = json_find_by_name_safe(jobj, json_type_array, "generating_commands");
    config->generating_commands_size = json_object_array_length(json_generating_commands);
    config->generating_commands = safe_calloc(config->generating_commands_size, sizeof(command_t));

    for (size_t i = 0; i < config->generating_commands_size; i++)
    {
        json_object *json_command = json_object_array_get_idx(json_generating_commands, i);
        config->generating_commands[i] = (command_t){
            .command =
                strdup(json_object_get_string(json_find_by_name_safe(json_command, json_type_string, "command"))),
            .async = json_object_get_boolean(json_find_by_name(json_command, json_type_boolean, "async")),
            .ignore_error = json_object_get_boolean(json_find_by_name(json_command, json_type_boolean, "ignore_error")),
            .restart = json_object_get_boolean(json_find_by_name(json_command, json_type_boolean, "restart")),
        };
    }

    // reload commands
    json_object *json_reload_commands = json_find_by_name_safe(jobj, json_type_array, "reload_commands");
    config->reload_commands_size = json_object_array_length(json_reload_commands);
    config->reload_commands = safe_calloc(config->reload_commands_size, sizeof(command_t));

    for (size_t i = 0; i < config->reload_commands_size; i++)
    {
        json_object *json_command = json_object_array_get_idx(json_reload_commands, i);
        config->reload_commands[i] = (command_t){
            .command =
                strdup(json_object_get_string(json_find_by_name_safe(json_command, json_type_string, "command"))),
            .async = json_object_get_boolean(json_find_by_name(json_command, json_type_boolean, "async")),
            .ignore_error = json_object_get_boolean(json_find_by_name(json_command, json_type_boolean, "ignore_error")),
            .restart = json_object_get_boolean(json_find_by_name(json_command, json_type_boolean, "restart")),
        };
    }

    json_object_put(jobj);

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
        char *new_command = commands[i].command;
        for (size_t j = 0; j < sizeof(variables) / sizeof(variables[0]); j++)
        {
            char *replaced_command = replace_substring(new_command, variables[j], values[j]);
            free(new_command);
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
    free(config->oomox_theme_name);
    free(config->oomox_icon_theme_name);
    free(config->image_path);
    for (size_t i = 0; i < config->generating_commands_size; i++)
    {
        free(config->generating_commands[i].command);
    }
    for (size_t i = 0; i < config->reload_commands_size; i++)
    {
        free(config->reload_commands[i].command);
    }
    free(config->generating_commands);
    free(config->reload_commands);
}
