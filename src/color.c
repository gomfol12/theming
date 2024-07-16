#include "color.h"

#include <math.h>
#include <stdio.h>

#include "util.h"

static double hue_to_rgb(double, double, double);

void darken_color(RGB *color, double amount)
{
    if (amount > 1.0 || amount < 0.0)
        amount = 0;

    color->r = (unsigned int)(color->r * (1.0 - amount));
    color->g = (unsigned int)(color->g * (1.0 - amount));
    color->b = (unsigned int)(color->b * (1.0 - amount));
}

void blend_color(RGB *color1, const RGB *color2)
{
    color1->r = (unsigned int)(color1->r * 0.5 + color2->r * 0.5);
    color1->g = (unsigned int)(color1->g * 0.5 + color2->g * 0.5);
    color1->b = (unsigned int)(color1->b * 0.5 + color2->b * 0.5);
}

void lighten_color(RGB *color, double amount)
{
    if (amount > 1.0 || amount < 0.0)
        amount = 0;

    color->r = (unsigned int)(color->r + (255 - color->r) * amount);
    color->g = (unsigned int)(color->g + (255 - color->g) * amount);
    color->b = (unsigned int)(color->b + (255 - color->b) * amount);
}

void saturate_color(RGB *color, double amount)
{
    if (amount > 1.0 || amount < 0.0)
        amount = 0;

    HLS hls;
    rgb_to_hls(color, &hls);
    hls.s = amount;
    hls_to_rgb(&hls, color);
}

RGB *from_hex_string_to_RGB(const char *color)
{
    RGB *rgb = safe_malloc(sizeof(RGB));
    sscanf(color + 1, "%02x%02x%02x", &rgb->r, &rgb->g, &rgb->b); // +1 skip the #
    return rgb;
}

char *from_RGB_to_hex_string(const RGB *color)
{
    char *hex = safe_malloc(8);
    sprintf(hex, "#%02x%02x%02x", color->r, color->g, color->b);
    return hex;
}

void rgb_to_hls(const RGB *color1, HLS *color2)
{
    double r_norm = color1->r / 255.0;
    double g_norm = color1->g / 255.0;
    double b_norm = color1->b / 255.0;

    double max_val = fmax(r_norm, fmax(g_norm, b_norm));
    double min_val = fmin(r_norm, fmin(g_norm, b_norm));

    double l = (max_val + min_val) / 2.0;
    double s = 0;
    double h = 0;

    if (max_val != min_val)
    {
        double delta = max_val - min_val;
        s = (l < 0.5) ? (delta / (max_val + min_val)) : (delta / (2.0 - max_val - min_val));

        if (max_val == r_norm)
        {
            h = (g_norm - b_norm) / delta + (g_norm < b_norm ? 6 : 0);
        }
        else if (max_val == g_norm)
        {
            h = (b_norm - r_norm) / delta + 2;
        }
        else
        {
            h = (r_norm - g_norm) / delta + 4;
        }

        h /= 6.0;
    }

    color2->h = h;
    color2->l = l;
    color2->s = s;
}

void hls_to_rgb(const HLS *color1, RGB *color2)
{
    double r, g, b;

    if (color1->s == 0)
    {
        // Achromatic case (gray)
        r = g = b = color1->l;
    }
    else
    {
        double q = (color1->l < 0.5) ? (color1->l * (1 + color1->s)) : (color1->l + color1->s - color1->l * color1->s);
        double p = 2 * color1->l - q;
        r = hue_to_rgb(p, q, color1->h + 1.0 / 3);
        g = hue_to_rgb(p, q, color1->h);
        b = hue_to_rgb(p, q, color1->h - 1.0 / 3);
    }

    color2->r = (unsigned int)(r * 255);
    color2->g = (unsigned int)(g * 255);
    color2->b = (unsigned int)(b * 255);
}

static double hue_to_rgb(double p, double q, double t)
{
    if (t < 0)
        t += 1;
    if (t > 1)
        t -= 1;
    if (t < 1.0 / 6)
        return p + (q - p) * 6 * t;
    if (t < 1.0 / 2)
        return q;
    if (t < 2.0 / 3)
        return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}
