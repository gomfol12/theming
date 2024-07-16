#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "util.h"

typedef struct
{
    void **items;
    size_t size;
    size_t capacity;
} vector_t;

vector_t *vector_init(size_t);
void vector_insert(vector_t *, void *);
void vector_set(vector_t *, size_t, void *);
void vector_remove(vector_t *, size_t);
void vector_free(vector_t *);

vector_t *vector_init(size_t item_size)
{
    vector_t *vector = safe_malloc(sizeof(vector_t));
    vector->items = safe_calloc(0, item_size);
    vector->size = 0;
    vector->capacity = 0;

    return vector;
}

void vector_insert(vector_t *vector, void *item)
{
    if (vector->size == vector->capacity)
    {
        vector->capacity = vector->capacity == 0 ? 10 : (size_t)((double)vector->capacity * 1.5);
        vector->items = safe_realloc(vector->items, vector->capacity * sizeof(item));
    }

    vector->items[vector->size++] = item;
}

void vector_set(vector_t *vector, size_t index, void *item)
{
    if (index >= vector->size)
    {
        die("Index out of bounds");
    }

    free(vector->items[index]);
    vector->items[index] = item;
}

void vector_remove(vector_t *vector, size_t index)
{
    for (size_t i = index; i < vector->size - 1; i++)
    {
        vector->items[i] = vector->items[i + 1];
    }

    vector->size--;
}

void vector_free(vector_t *vector)
{
    for (size_t i = 0; i < vector->size; i++)
    {
        free(vector->items[i]);
    }

    free(vector->items);
    free(vector);
}
