#pragma once

typedef struct
{
    unsigned int r;
    unsigned int g;
    unsigned int b;
} RGB;

typedef struct
{
    double h; // Hue
    double l; // Lightness
    double s; // Saturation
} HLS;

void darken_color(RGB *, double);
void blend_color(RGB *, const RGB *);
void lighten_color(RGB *, double);
void saturate_color(RGB *, double);
RGB *from_hex_string_to_RGB(const char *);
char *from_RGB_to_hex_string(const RGB *);
void rgb_to_hls(const RGB *, HLS *);
void hls_to_rgb(const HLS *, RGB *);
