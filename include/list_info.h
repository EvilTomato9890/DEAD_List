#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

#include "error_handler.h"
#include <stdio.h>
#include <stdlib.h>

static const size_t MIN_LIST_SIZE    = 3;
static const int    POISON           = -1;
static const float  REDUCTION_FACTOR = 4.0f;
static const float  GROWTH_FACTOR    = 2.0f;
static const float  CANARY_NUM       = 10101.0f;


#ifdef VERIFY_DEBUG
    #define ON_DEBUG(...) __VA_ARGS__
#else
    #define ON_DEBUG(...)
#endif

#define VER_INIT ver_info_t{__FILE__, __func__, __LINE__}

struct node_t {
    int next;
    int prev;
    double val;
};

struct ver_info_t {
    const char* file;
    const char* func;
    int         line;
};

struct list_t {
    node_t* arr;
    size_t  capacity;
    size_t  size;

    int head;
    int tail;
    int free_head;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

static inline bool list_node_is_free(const node_t* node) {
    return node->prev == POISON || node->val == POISON;
}


#endif /* LIST_H_INCLUDED */