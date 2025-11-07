#ifndef LIST_VERIFICATION_H_INCLUDED
#define LIST_VERIFICATION_H_INCLUDED

#include <stdbool.h>
#include "list_info.h"

typedef enum {
    DUMP_NO   = 0,
    DUMP_TEXT = 1,
    DUMP_IMG  = 2,
} dump_mode_t;

ON_DEBUG(
error_code list_verify(list_t* list,
                       ver_info_t ver_info,
                       dump_mode_t mode,
                       const char* fmt, ...);
)

ON_DEBUG(
void list_dump(list_t* list,
                     ver_info_t ver_info,
                     bool is_visual,
                     const char* fmt, ...);
)

#endif
