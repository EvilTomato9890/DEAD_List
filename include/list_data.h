#ifndef LIST_DATA_H_INCLUDED
#define LIST_DATA_H_INCLUDED
#define VERIFY_DEBUG
#include "error_handler.h"
#include "stdio.h"
#include <stdlib.h>

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
    const char* creation_file; 
    const char* creation_func;
    int   creation_line;
};

struct list_t {
    node_t* arr;
    size_t  capacity;
    size_t  size;

    int head;
    int tail;
    int free_head;
    ON_DEBUG (
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

error_code list_init(list_t* list, size_t capacity ON_DEBUG(, ver_info_t ver_info));

error_code list_dest(list_t* list);

int list_insert(list_t* list, int insert_index, double val);
int list_insert_auto(list_t* list, int insert_index, double val);
int list_insert_before(list_t* list, int insert_index, double val);

error_code list_remove(list_t* list, int remove_index); 
error_code list_remove_auto(list_t* list, int remove_index); //todo: int в ретурне

error_code list_dump(list_t* list, ver_info_t ver_info, bool is_visual_dump);
#endif



//calloc для маленьких объектов делает тоже самое, что и мы, собирает их в 1 маленький массив и если нужно расширяет его пока может. 