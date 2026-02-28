/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "dyna.h"

/*! @uses assert. */
#include <assert.h>

/*! @uses fprintf. */
#include <stdio.h>

/*! @uses calloc, realloc, free, exit. */
#include <stdlib.h>

/*! @uses memcpy. */
#include <string.h>

/**
 * @brief create a dyna_t structure with a set item size.
 *
 * @return an allocated dynamic array.
 */
dyna_t*
dyna_create() {
    /* allocate the array and set all data to be 0. */
    dyna_t* array = calloc(1u, sizeof *array);
    array->data = 0x0;
    array->length = 0u;
    array->capacity = 0u;
    return array;
}

/**
 * @brief destroying / freeing a dynamically allocated array.
 *
 * @param array pointer to a dynamically allocated array.
 */
void
dyna_free(dyna_t* array) {
    /* assert if the array is 0x0. */
    assert(array != 0x0);

    /* free. */
    free(array->data);
    free(array);
}

/**
 * @brief push new item/data onto a dynamically allocated array.
 *
 * @param array pointer to a dynamically allocated array.
 * @param data data to be pushed onto the top of the allocated array.
 */
void
dyna_push(dyna_t* array, void* data) {
    /* assert if the array or data == 0x0. */
    assert(array != 0x0 && data != 0x0);

    /* compare length and capacity. */
    if (array->length == array->capacity) {
        size_t _capacity = array->capacity == 0u ? 16u : array->capacity * 2u;
        void** _data = realloc(array->data, sizeof(void*) * _capacity);
        if (!_data) {
            /* NOLINTNEXTLINE */
            fprintf(stderr, "lzd, dyna_push; realloc failed; could not allocate memory for push.");
            return; /* do NOT exit on failure. */
        }
        array->data = _data;
        array->capacity = _capacity;
    }
    array->data[array->length++] = data;
}

/**
 * @brief pop data out of a dynamically allocated array,
 *  while also making sure to shift down / coalesce the memory.
 *  this function does not SHRINK the allocated memory, see
 *  @ref dyna_shrink().
 *
 * @param array pointer to a dynamically allocated array.
 * @param index index at which to pop the item.
 * @return data at the specified index, popped off the array.
 */
void*
dyna_pop(dyna_t* array, size_t index) {
    /* assert if the array == 0x0. */
    assert(array != 0x0);
    if (index >= array->length)
        return 0x0;

    /* capture the element */
    void* item = array->data[index];

    /* free, shift down and then decrement length. */
    for (size_t i = index + 1u; i < array->length; i++)
        array->data[i - 1u] = array->data[i];
    array->length--;
    return item;
}

/**
 * @brief get the data at the index specified from a dynamically
 *  allocated array.
 *
 * @param array the dynamically allocated array to get information from.
 * @param index the index in the array that we are to retrieve data from.
 * @return 0x0 if the index is out of bounds or the data at a specified index in the array.
 */
void*
dyna_get(dyna_t* array, size_t index) {
    /* assert if the array == 0x0. */
    assert(array != 0x0);

    /* if the index is out of bounds. */
    if (index >= array->length)
        return 0x0;
    return array->data[index];
}

/**
 * @brief shrinks the array to the length via realloc.
 *
 * @param array the dynamically allocated array to be shrinked.
 */
void
dyna_shrink(dyna_t* array) {
    /* assert if the array == 0x0. */
    assert(array != 0x0);

    /* do a realloc down where capacity = length. */
    void** _data = realloc(array->data, sizeof(void*) * array->length);
    if (!_data) {
        /* NOLINTNEXTLINE */
        fprintf(stderr, "lzd, dyna_shrink; realloc failed; could not allocate memory for shrink.");
        exit(EXIT_FAILURE); /* exit on failure. */
    }
    array->data = _data;
    array->capacity = array->length;
}

/**
 * @brief make a new dynamic array given an array of data.
 *
 * @param data the list of data to be copied into a dynamic array.
 * @param length the length of the list of data.
 * @return an allocated dyna_t structure with all data copied over.
 */
dyna_t*
dyna_make(void** data, size_t length) {
    /* allocate the array and set all data to be 0. */
    dyna_t* array = calloc(1u, sizeof *array);
    array->data = calloc(length, sizeof(void*));
    array->length = length;
    array->capacity = length;

    /* copy and return. */
    /* NOLINTNEXTLINE */
    memcpy(array->data, data, length);
    return array;
}