#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "color.h"
#include "config.h"
#include "project_vars.h"
#include "util.h"
#include "vector.h"

static vector_t *parse_colors(const char *);
static vector_t *get_colors(const char *, bool);
static void create_cache_file(const char *, vector_t *, const char *, void (*)(FILE *, vector_t *, void *), void *);
static void generate_colors_oomox(FILE *, vector_t *, void *);
static void generate_colors_xresources(FILE *, vector_t *, void *);
static void generate_colors(FILE *, vector_t *, void *);
static void generate_colors_json(FILE *, vector_t *, void *);
static void make_dirs(config_t config);
static void generate_themes(config_t config);
static void print_usage(const char *);
static void wal_compatibility(config_t config);

static vector_t *get_colors(const char *image_path, bool dark)
{
    // call imagemagick
    char *output = safe_malloc(BUFSIZ);
    exec_command_format(false, output, BUFSIZ, "magick %s -resize 25%% -colors 16 -unique-colors txt:-", image_path);

    // get colors array
    vector_t *parsed_colors = parse_colors(output);
    free(output);

    // fuckery to rearrange the colors
    for (size_t i = 1; i < parsed_colors->size; i++)
    {
        RGB *tmp_color = safe_malloc(sizeof(RGB));

        if (i < 9)
            memcpy(tmp_color, parsed_colors->items[i + 7], sizeof(RGB));
        else
            memcpy(tmp_color, parsed_colors->items[i - 8], sizeof(RGB));

        vector_set(parsed_colors, i, tmp_color);
    }

    // adjut colors
    if (!dark)
    {
        for (size_t i = 0; i < parsed_colors->size; i++)
        {
            lighten_color((RGB *)(parsed_colors->items[i]), 0.5);
        }

        // TODO: make better
        RGB *tmp_color_last_1 = safe_malloc(sizeof(RGB));
        RGB *tmp_color_last_2 = safe_malloc(sizeof(RGB));
        memcpy(tmp_color_last_1, parsed_colors->items[parsed_colors->size - 1], sizeof(RGB));
        memcpy(tmp_color_last_2, parsed_colors->items[parsed_colors->size - 1], sizeof(RGB));
        lighten_color(tmp_color_last_1, 0.85);
        darken_color(tmp_color_last_2, 0.4);

        RGB *tmp_color_0_1 = safe_malloc(sizeof(RGB));
        RGB *tmp_color_0_2 = safe_malloc(sizeof(RGB));
        memcpy(tmp_color_0_1, parsed_colors->items[0], sizeof(RGB));
        memcpy(tmp_color_0_2, parsed_colors->items[0], sizeof(RGB));

        vector_set(parsed_colors, 0, tmp_color_last_1);
        vector_set(parsed_colors, 7, tmp_color_0_1);
        vector_set(parsed_colors, 8, tmp_color_last_2);
        vector_set(parsed_colors, 15, tmp_color_0_2);

        return parsed_colors;
    }

    RGB *blend = from_hex_string_to_RGB("#EEEEEE");

    darken_color((RGB *)(parsed_colors->items[0]), 0.4);
    blend_color((RGB *)(parsed_colors->items[7]), blend);
    darken_color((RGB *)(parsed_colors->items[8]), 0.3);
    blend_color((RGB *)(parsed_colors->items[15]), blend);

    free(blend);

    return parsed_colors;
}

static vector_t *parse_colors(const char *text)
{
    regex_t regex;

    if (regcomp(&regex, "#[0-9A-Fa-f]{6}", REG_EXTENDED))
    {
        die("regcomp failed:");
    }

    regmatch_t match;
    vector_t *colors = vector_init(sizeof(RGB));

    // loop through all the matches and append them to the output
    const char *search_start = text;
    while (regexec(&regex, search_start, 1, &match, 0) == 0)
    {
        // copy match to a new string
        ssize_t len = match.rm_eo - match.rm_so;
        if (len < 0)
        {
            die("regexec failed:");
        }
        char *color = safe_malloc((size_t)(len + 1));
        memcpy(color, search_start + match.rm_so, (size_t)len);
        color[len] = '\0';

        vector_insert(colors, from_hex_string_to_RGB(color));
        free(color);

        search_start += match.rm_eo;
    }

    regfree(&regex);
    return colors;
}

static void create_cache_file(const char *name, vector_t *colors, const char *cache_path,
                              void (*callback)(FILE *, vector_t *, void *), void *userdata)
{
    char *file_path = format_string("%s/%s", cache_path, name);
    char *expanded_path = expand_tilde(file_path);
    FILE *file = fopen(expanded_path, "w");
    if (file == NULL)
    {
        die("fopen failed:");
    }
    free(expanded_path);
    free(file_path);

    vector_t *color_strings = vector_init(sizeof(char *));
    for (size_t i = 0; i < colors->size; i++)
        vector_insert(color_strings, from_RGB_to_hex_string(colors->items[i]));

    callback(file, color_strings, userdata);

    if (ferror(file))
    {
        die("writing to file failed:");
    }

    vector_free(color_strings);
    fclose(file);
}

static void generate_colors_oomox(FILE *file, vector_t *colors, void *userdata)
{
    const char *bg = (char *)colors->items[0] + 1;
    const char *fg = (char *)colors->items[7] + 1;
    const char *sel_bg = (char *)colors->items[1] + 1;
    const char *sel_fg = (char *)colors->items[0] + 1;
    const char *btn_bg = (char *)colors->items[4] + 1;
    const char *btn_fg = (char *)colors->items[0] + 1;
    const char *wm_border_focus = (char *)colors->items[1] + 1;
    const char *icons_light_folder = (char *)colors->items[2] + 1;
    const char *icons_light = (char *)colors->items[3] + 1;
    const char *icons_medium = (char *)colors->items[4] + 1;
    const char *icons_dark = (char *)colors->items[5] + 1;

    fprintf(file,
            "NAME=\"Theme\"\n"
            "NOGUI=True\n"
            "BG=%s\n"
            "FG=%s\n"
            "TXT_BG=%s\n"
            "TXT_FG=%s\n"
            "SEL_BG=%s\n"
            "SEL_FG=%s\n"
            "HDR_BG=%s\n"
            "HDR_FG=%s\n"
            "BTN_BG=%s\n"
            "BTN_FG=%s\n"
            "WM_BORDER_FOCUS=%s\n"
            "ICONS_LIGHT_FOLDER=%s\n"
            "ICONS_LIGHT=%s\n"
            "ICONS_MEDIUM=%s\n"
            "ICONS_DARK=%s",
            bg, fg, bg, fg, sel_bg, sel_fg, bg, fg, btn_bg, btn_fg, wm_border_focus, icons_light_folder, icons_light,
            icons_medium, icons_dark);

    // TODO: look at this
    // NAME=wal
    // BG=141516
    // FG=e8edeb
    // MENU_BG=141516
    // MENU_FG=e8edeb
    // SEL_BG=A87E85
    // SEL_FG=141516
    // TXT_BG=141516
    // TXT_FG=e8edeb
    // BTN_BG=709791
    // BTN_FG=e8edeb
    // HDR_BTN_BG=9FA39D
    // HDR_BTN_FG=e8edeb
    // GTK3_GENERATE_DARK=True
    // ROUNDNESS=0
    // SPACING=3
    // GRADIENT=0.0
}

static void generate_colors_xresources(FILE *file, vector_t *colors, void *userdata)
{
    const char *fg = (char *)colors->items[7];
    const char *bg = (char *)colors->items[0];
    const char *color0 = (char *)colors->items[0];
    const char *color1 = (char *)colors->items[1];
    const char *color2 = (char *)colors->items[2];
    const char *color3 = (char *)colors->items[3];
    const char *color4 = (char *)colors->items[4];
    const char *color5 = (char *)colors->items[5];
    const char *color6 = (char *)colors->items[6];
    const char *color7 = (char *)colors->items[7];
    const char *color8 = (char *)colors->items[8];

    char first[3] = "\0";
    char second[3] = "\0";
    char third[3] = "\0";
    strncpy(first, fg + 1, 2);
    strncpy(second, fg + 3, 2);
    strncpy(third, fg + 5, 2);
    char *strange_format_fg = format_string("rgba:%s/%s/%s/ff", first, second, third);

    fprintf(file,
            "! X colors.\n"
            "*foreground:        %s\n"
            "*background:        %s\n"
            "*.foreground:       %s\n"
            "*.background:       %s\n"
            "emacs*foreground:   %s\n"
            "emacs*background:   %s\n"
            "URxvt*foreground:   %s\n"
            "XTerm*foreground:   %s\n"
            "UXTerm*foreground:  %s\n"
            "URxvt*background:   [100]%s\n"
            "XTerm*background:   %s\n"
            "UXTerm*background:  %s\n"
            "URxvt*cursorColor:  %s\n"
            "XTerm*cursorColor:  %s\n"
            "UXTerm*cursorColor: %s\n"
            "URxvt*borderColor:  [100]%s\n"
            "\n"
            "! Colors 0-15.\n"
            "*.color0: %s\n"
            "*color0:  %s\n"
            "*.color1: %s\n"
            "*color1:  %s\n"
            "*.color2: %s\n"
            "*color2:  %s\n"
            "*.color3: %s\n"
            "*color3:  %s\n"
            "*.color4: %s\n"
            "*color4:  %s\n"
            "*.color5: %s\n"
            "*color5:  %s\n"
            "*.color6: %s\n"
            "*color6:  %s\n"
            "*.color7: %s\n"
            "*color7:  %s\n"
            "*.color8: %s\n"
            "*color8:  %s\n"
            "*.color9: %s\n"
            "*color9:  %s\n"
            "*.color10: %s\n"
            "*color10:  %s\n"
            "*.color11: %s\n"
            "*color11:  %s\n"
            "*.color12: %s\n"
            "*color12:  %s\n"
            "*.color13: %s\n"
            "*color13:  %s\n"
            "*.color14: %s\n"
            "*color14:  %s\n"
            "*.color15: %s\n"
            "*color15:  %s\n"
            "\n"
            "! Black color that will not be affected by bold highlighting.\n"
            "*.color66: %s\n"
            "*color66:  %s\n"
            "\n"
            "! Xclock colors.\n"
            "XClock*foreground: %s\n"
            "XClock*background: %s\n"
            "XClock*majorColor:  %s\n"
            "XClock*minorColor:  %s\n"
            "XClock*hourColor:   %s\n"
            "XClock*minuteColor: %s\n"
            "XClock*secondColor: %s\n"
            "\n"
            "! Set depth to make transparency work.\n"
            "URxvt*depth: 32",
            fg, bg, fg, bg, fg, bg, fg, fg, fg, bg, bg, bg, fg, fg, fg, bg, color0, color0, color1, color1, color2,
            color2, color3, color3, color4, color4, color5, color5, color6, color6, color7, color7, color8, color8,
            color1, color1, color2, color2, color3, color3, color4, color4, color5, color5, color6, color6, color7,
            color7, color0, color0, fg, bg, strange_format_fg, strange_format_fg, strange_format_fg, strange_format_fg,
            strange_format_fg);

    free(strange_format_fg);
}

static void generate_colors_json(FILE *file, vector_t *colors, void *userdata)
{
    const char *bg = (char *)colors->items[0];
    const char *fg = (char *)colors->items[7];
    const char *image_path = (char *)userdata;

    fprintf(file,
            "{\n"
            "    \"wallpaper\": \"%s\",\n"
            "    \"alpha\": 100,\n"
            "    \"special\": {\n"
            "        \"background\": \"%s\",\n"
            "        \"foreground\": \"%s\",\n"
            "        \"cursor\": \"%s\"\n"
            "    },\n"
            "    \"colors\": {\n",
            image_path, bg, fg, fg);

    for (size_t i = 0; i < colors->size; i++)
    {
        fprintf(file, "        \"color%d\": \"%s\"", (int)i, (char *)colors->items[i]);
        if (i < colors->size - 1)
        {
            fprintf(file, ",");
        }
        fprintf(file, "\n");
    }

    fprintf(file, "    }\n"
                  "}");
}

static void generate_colors(FILE *file, vector_t *colors, void *userdata)
{
    for (size_t i = 0; i < colors->size; i++)
    {
        fprintf(file, "%s\n", (char *)colors->items[i]);
    }
}

static void make_dirs(config_t config)
{
    mkdir_p(config.cache_path);
    mkdir_p(config.theme_path);
    mkdir_p(config.icon_theme_path);
}

static void *pthread_generate_wrapper(void *arg)
{
    command_t *command = (command_t *)arg;

    exec_command(command->command, command->ignore_error, NULL, 0);

    return NULL;
}

static void generate_themes(config_t config)
{
    vector_t *vec = get_colors(config.image_path, true);

    // generate needed files
    create_cache_file("colors-oomox", vec, config.cache_path, generate_colors_oomox, NULL);
    create_cache_file("colors.Xresources", vec, config.cache_path, generate_colors_xresources, NULL);
    create_cache_file("colors", vec, config.cache_path, generate_colors, NULL);
    create_cache_file("colors.json", vec, config.cache_path, generate_colors_json, config.image_path);

    vector_free(vec);

    // generate theme stuff

    // exec sync commands
    for (size_t i = 0; i < config.generating_commands_size; i++)
    {
        if (!config.generating_commands[i].async)
        {
            exec_command(config.generating_commands[i].command, config.generating_commands[i].ignore_error, NULL, 0);
        }
    }

    // exec async commands
    size_t thread_count = config.generating_commands_size;
    pthread_t t[thread_count];

    for (size_t i = 0; i < thread_count; i++)
    {
        if (config.generating_commands[i].async)
        {
            pthread_create(&t[i], NULL, pthread_generate_wrapper, &config.generating_commands[i]);
        }
    }

    for (size_t i = 0; i < thread_count; i++)
    {
        pthread_join(t[i], NULL);
    }
}

static void print_usage(const char *program_name)
{
    printf("Usage: %s [-vhi:rw] [<image_path>]\n", program_name);
    printf("Options:\n");
    printf("  -v, --version\t\t\tShow version\n");
    printf("  -h, --help\t\t\tShow this help message\n");
    printf("  -i, --image <image_path>\tGenerate theme from image\n");
    printf("  -r, --reload\t\t\tReload theme\n");
    printf("  -w, --wal\t\t\tGenerate pywal .cache file to make generated theme compatible.\n");
}

static void wal_compatibility_helper(config_t config, const char *wal_cache_path, const char *file_name)
{
    char *colors_path_from = format_string("%s/%s", config.cache_path, file_name);
    char *colors_path_to = format_string("%s/%s", wal_cache_path, file_name);
    make_symlink(colors_path_from, colors_path_to);
    free(colors_path_from);
    free(colors_path_to);
}

static void wal_compatibility(config_t config)
{
    char *wal_cache_path = expand_tilde("~/.cache/wal");

    if (check_directory(wal_cache_path) == 0)
    {
        die("pywal cache directory already exists");
    }

    mkdir_p(wal_cache_path);

    wal_compatibility_helper(config, wal_cache_path, "colors");
    wal_compatibility_helper(config, wal_cache_path, "colors.json");
    wal_compatibility_helper(config, wal_cache_path, "colors-oomox");
    wal_compatibility_helper(config, wal_cache_path, "colors.Xresources");
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"version", no_argument, 0, 0}, {"help", no_argument, 0, 0}, {"image", required_argument, 0, 0},
        {"reload", no_argument, 0, 0},  {"wal", no_argument, 0, 0},  {0, 0, 0, 0},
    };

    char *image = NULL;
    bool generate = false;
    bool reload = false;
    bool wal_comp = false;

    int c;
    while ((c = getopt_long(argc, argv, "vhi:rw", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'v':
            // printf("Version: %s.%s\n", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
            printf("%s version: %d.%d\n", argv[0], PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
            return EXIT_SUCCESS;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'i':
            image = optarg;
            generate = true;
            break;
        case 'r':
            reload = true;
            break;
        case 'w':
            wal_comp = true;
            break;
        default:
            return EXIT_FAILURE;
        }
    }

    if (optind < argc)
    {
        printf("Unknown options: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");

        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 1)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    config_t config;
    config_init(&config, image);

    if (generate)
    {
        make_dirs(config);
        generate_themes(config);
    }
    if (reload)
    {
        for (size_t i = 0; i < config.reload_commands_size; i++)
        {
            exec_command(config.reload_commands[i].command, config.reload_commands[i].ignore_error, NULL, 0);
        }
    }
    if (wal_comp)
    {
        wal_compatibility(config);
    }

    config_free(&config);

    return EXIT_SUCCESS;
}
