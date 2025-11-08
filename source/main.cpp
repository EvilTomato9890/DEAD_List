
#include <string.h>
#include "logger.h"
#include "error_handler.h"
#include "handle_input.h"
#include "list_operations.h"
#include "list_verification.h"

static error_code choose_input_type_and_process(list_t* list, int argc, char* argv[]);

int main(int argc, char* argv[]) {
    logger_initialize_stream(nullptr);
    LOGGER_DEBUG("Program started");
    error_code error = 0;

    list_t list = {};
    error |= list_init(&list, 5 ON_DEBUG(, VER_INIT));

    error |= choose_input_type_and_process(&list, argc, argv);

    error |= list_dest(&list);
    LOGGER_DEBUG("programm ended");
    LOGGER_DEBUG("ERROR_CODE=%ld", (error));
    return 0;
}

static error_code choose_input_type_and_process(list_t* list, int argc, char* argv[]) {
    error_code error = 0;

    if(argc < 2) {
        LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
        return ERROR_INCORRECT_ARGS;
    }
    ON_DEBUG(
        if(argc < 3) {
            LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
            return ERROR_INCORRECT_ARGS;
        }
    )
    if(strcmp(argv[1], "-ic") == 0) {
        ON_DEBUG(
            list->dump_file = fopen(argv[2], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_interactive_console_input(list);
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else if(strcmp(argv[1], "-if") == 0) {
        ON_DEBUG(
            list->dump_file = fopen(argv[2], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_interactive_file_input(list);
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else if(strcmp(argv[1], "-f")  == 0) {
        if(argc < 4) {
            LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
            return ERROR_INCORRECT_ARGS;
        }
        ON_DEBUG(
            if(argc < 5) {
                LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
                return ERROR_INCORRECT_ARGS;
            }
            list->dump_file = fopen(argv[4], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_file_input(list, argv[2], argv[3]);


        list_dump(list, VER_INIT, false, "Clear dump"); //todo: не вставлять

        int insert_idx1 = 0;
        int insert_idx2 = 1;
        int insert_idx3 = 2;
        list_insert_after(list, insert_idx1, 10.0);
        list_insert_after(list, insert_idx2, 20.0);
        list_insert_after(list, insert_idx3, 30.0);
        list_dump(list, VER_INIT, true, "AFTER inserts at idx1: %d, idx2: %d, idx3: %d", insert_idx1, insert_idx2, insert_idx3);

        
        list_linearize(list);
        list_dump(list, VER_INIT, true, "After linearize");

        int insert_idx = 1;
        list_insert_after(list, insert_idx, 40.0);
        list_dump(list, VER_INIT, true, "AFTER insert index %d", insert_idx);

        list_shrink_to_fit(list, false);
        list_dump(list, VER_INIT, true, "After shrink_to_fit");


        list_remove(list, 5);
        list_remove(list, 6);
        list_remove(list, 1);
        list_dump(list, VER_INIT, true, "After removes");
        list_insert_after(list, 11, 15.0);
        list_insert_after(list, 11, 22.0);
        list->arr[3].prev = 150; // to trigger error in dump
        list_dump(list, VER_INIT, true, "Corrupted dump");

        list->arr[8].next = 5;
        list_insert_auto(list, 1, 42.0);
        
        list->arr[3].next = 150;
        list_dump(list, VER_INIT, true, "Corrupted dump 2");

        list->arr[0].next = 3;
        list_dump(list, VER_INIT, true, "Corrupted dump 3");

        list->arr[list->capacity].val = 0;
        list_remove(list, 1);


//Замер времени изолировать на 1 ядре
//Опциии компилятора
//Замер времени, теммпературы.
//Сравнение отдельных операций
// README

//



/*
        list_insert_after(list, 0, 55.0);
        list_insert_after(list, 0, 65.0);
        list_insert_after(list, 0, 75.0);
        list_remove(list, 2);
        list_remove_auto(list, 4);
        list_remove(list, 3);
        list_dump(list, VER_INIT, true, "After more operations");

        list->arr[11].prev = 150;
        list_dump(list, VER_INIT, true, "Corrupted dump free list");
*/
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else {
        LOGGER_ERROR("Unknown arg: %s", argv[1]);
        return ERROR_INCORRECT_ARGS;
    }
}

