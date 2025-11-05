#include <string.h>
#include "logger.h"
#include "error_handler.h"
#include "handle_input.h"
#include "list_data.h"

static error_code choose_input_type_and_process(list_t* list, int argc, char* argv[]);

int main(int argc, char* argv[]) {
    logger_initialize_stream(nullptr);
    LOGGER_DEBUG("Program started");
    error_code error = 0;

    list_t list = {};
    error |= list_init(&list, 5 ON_DEBUG(, VER_INIT));

    error |= choose_input_type_and_process(&list, argc, argv);
    list_dump(&list, VER_INIT, true);
    error |= list_dest(&list);
    LOGGER_DEBUG("programm ended");
    RETURN_IF_ERROR(error);
    return 0;
}

static error_code choose_input_type_and_process(list_t* list, int argc, char* argv[]) {
    error_code error = 0;

    if(argc < 2) {
        LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
        return ERROR_INCORRECT_ARGS;
    }
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
            list->dump_file = fopen(argv[4], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_file_input(list, argv[2], argv[3]);
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else {
        LOGGER_ERROR("Unknown arg: %s", argv[1]);
        return ERROR_INCORRECT_ARGS;
    }
}