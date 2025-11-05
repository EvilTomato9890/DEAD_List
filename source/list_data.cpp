#include "list_data.h"
#include "logger.h"
#include "asserts.h"
#include "error_handler.h"
#include <stdlib.h>
#include <time.h>

static const int   MIN_LIST_SIZE    = 3;
static const int   POISON           = -1;
static const float REDUCTION_FACTOR = 4;
static const float GROWTH_FACTOR    = 2;
       const float CANARY_NUM       = 10101; //TODO: fff

//==============================================================================

static error_code normalize_capacity(list_t* list);
static error_code list_recalloc(list_t* list, size_t new_capacity);
static void list_dump_graphviz(list_t* list, const char* filename);
static error_code list_verify(list_t* list, ver_info_t ver_info, bool is_visual_dump);
static void list_dump_text(list_t* list, ver_info_t ver_info, int dump_cnt);
static inline bool is_free(node_t* node);
//==============================================================================

static inline bool is_free(node_t* node) {
    return node->prev == POISON || node->val == POISON;
}


static error_code normalize_capacity(list_t* list) {
    LOGGER_DEBUG("Normalize_size started");

	HARD_ASSERT(list != nullptr, "List is nullptr");

	error_code error = 0;
	if(list->size + 1 > list->capacity) {                                                                                                                               //TODO: lest.free_head
		LOGGER_DEBUG("Old capacity = %lu", list->capacity);
		LOGGER_DEBUG("Trying to realloc list for %lu elemets", (size_t)(list->capacity * GROWTH_FACTOR));
		error = list_recalloc(list, list->capacity * GROWTH_FACTOR); 
		RETURN_IF_ERROR(error);

		LOGGER_DEBUG("Reallocation complete");
		LOGGER_DEBUG("NEW CAPACITY = %lu", list->capacity);

	}
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, false);
    )
	return error;
}

static void init_free_list(list_t* list, size_t start_index, size_t end_index) {
    for(size_t i = start_index; i < end_index - 1; i++) {
        list->arr[i].next = i + 1;
        list->arr[i].prev = POISON;
        list->arr[i].val = POISON;
    }
    list->arr[end_index - 1].next = -1;  
    list->arr[end_index - 1].prev = POISON;
    list->arr[end_index - 1].val = POISON;
}

static error_code list_recalloc(list_t* list, size_t new_capacity) {
    LOGGER_DEBUG("list_recalloc started");

    HARD_ASSERT(list != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "Arr is nullptr");

    error_code error = 0;
    if(new_capacity == 0) LOGGER_WARNING("Realloc_size is null");

    size_t new_capacity_bytes = (new_capacity ON_DEBUG(+ 1)) * sizeof(node_t);
    LOGGER_DEBUG("Trying to realloc %lu bytes", new_capacity_bytes);
    
    node_t* new_block = (node_t*)realloc(list->arr, new_capacity_bytes);
    if(new_block == nullptr) {
        LOGGER_ERROR("Reallocation failed");
        error |= ERROR_MEM_ALLOC;
        return error;
    }
    LOGGER_DEBUG("Reallocation complete");

    list->arr = new_block;
    
    ON_DEBUG(
        LOGGER_DEBUG("Canary reinitialize started");
        new_block[0].val            = CANARY_NUM;
        new_block[new_capacity].val = CANARY_NUM;
    )

    LOGGER_DEBUG("Initializing new free cells");
    if(list->free_head == -1) {
        list->free_head = list->capacity;
        init_free_list(list, list->capacity, new_capacity);
    } else {
        int last_free = list->free_head;                                                                                                                                                                                                     //todo: добавить free_tail
        while (list->arr[last_free].next != -1) {
            last_free = list->arr[last_free].next;
        }
        list->arr[last_free].next = list->capacity;
        init_free_list(list, list->capacity, new_capacity);
    }

    list->capacity = new_capacity;
    return error;
}

//==============================================================================

error_code list_init(list_t* list_return, size_t capacity ON_DEBUG(, ver_info_t ver_info)) {
    LOGGER_DEBUG("List_init started");

    HARD_ASSERT(list_return != nullptr, "List is nullptr");
    
    error_code error = 0;
    if(capacity < MIN_LIST_SIZE) {
        LOGGER_INFO("Capacity normalize");
        capacity = MIN_LIST_SIZE;
    }

    list_t list = {};
	LOGGER_DEBUG("Trying to calloc %lu bytes", (capacity + 1 ON_DEBUG(+ 1)) * sizeof(node_t));
	list.arr = (node_t*)calloc(capacity + 1 ON_DEBUG(+ 1), sizeof(node_t));
	if(list.arr == nullptr) {
		LOGGER_DEBUG("Memory allocation failed");
		error |= ERROR_MEM_ALLOC;
		return error;
	}

    LOGGER_DEBUG("Filling the array");
	for(size_t i = 0; i < capacity - 1; i++) {
        list.arr[i].next = i + 1;
		list.arr[i].prev = POISON;
        list.arr[i].val  = POISON;
	}
    list.arr[capacity - 1].next = -1;
    list.arr[capacity - 1].prev = POISON;
    list.arr[capacity - 1].val  = POISON;

    list.head      = 0;
    list.tail      = 0;
    list.free_head = 1;
	list.capacity  = capacity;
    list.size      = 1;
	ON_DEBUG(
        list.arr[capacity].val = CANARY_NUM;
		list.ver_info          = ver_info;
	)
    list.arr[0].next       = 0;
    list.arr[0].prev       = 0;
    list.arr[0].val        = CANARY_NUM;
    list.arr[capacity].val = CANARY_NUM;
	*list_return = list;
    ON_DEBUG(
        error |= list_verify(&list, VER_INIT, false);
    )
    return error;
}

error_code list_dest(list_t* list) {
    HARD_ASSERT(list      != nullptr, "list is nullptr");
    HARD_ASSERT(list->arr != nullptr, "arr is nullptr");

    error_code error = 0;

    free(list->arr);
    list->arr = nullptr;
    return error;
}

//==============================================================================


int list_insert_auto(list_t* list, int insert_index, double val) {
    int current_index = list->head;
    for (int i = 0; i < insert_index - 1; i++) {
        current_index = list->arr[current_index].next;
    }
    return list_insert(list, current_index, val);
}

error_code list_remove_auto(list_t* list, int remove_index) {
    int physical_index = list->head;
    for (int i = 0; i < remove_index; i++) {
        physical_index = list->arr[physical_index].next;
    }
    return list_remove(list, physical_index);
}

int list_insert(list_t* list, int insert_index, double val) {
    LOGGER_DEBUG("list_insert started");
    
    HARD_ASSERT(list != nullptr, "List is nullptr");

    if(insert_index < 0 || (size_t)insert_index >= list->capacity) {
        LOGGER_ERROR("Incorrect index");
        return -1;
    } 
    
    int physical_index = list->free_head;
    if(physical_index == -1) {
        LOGGER_ERROR("Cells ended");
        return -1;
    }
    list->free_head = list->arr[physical_index].next; 

    int next_index = list->arr[insert_index].next;        
    list->arr[physical_index].val  = val;
    list->arr[physical_index].next = next_index;
    list->arr[physical_index].prev = insert_index;
    
    list->arr[insert_index].next   = physical_index;
    list->arr[next_index].prev     = physical_index;

    list->head = list->arr[0].next; 
    list->tail = list->arr[0].prev;

    list->size++;
    error_code error = 0;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, false);
    )
    error |= normalize_capacity(list);
    if(error != 0) {
        LOGGER_ERROR("Normalize or verify error");
        return -1;
    }
    
    LOGGER_DEBUG("Inserted value %lf at position %d, physical index: %d", 
                 val, insert_index, physical_index);
    return physical_index;
}

int list_insert_before(list_t* list, int insert_index, double val) {
    HARD_ASSERT(list != nullptr, "List is nullptr");
    
    LOGGER_DEBUG("List_insert_before started");

    error_code error = 0;

    ON_DEBUG(
        error |= list_verify(list, VER_INIT, false);
    )
    int current = list->arr[insert_index].prev;
    int phs_idx = list_insert(list, current, val);

    if(phs_idx > 0) {
        LOGGER_ERROR("Wrong_insert");
        error |= ERROR_INSERT_FAIL;
    }
    return error;
}

error_code list_remove(list_t* list, int remove_index) {
    HARD_ASSERT(list != nullptr, "List is nullptr");

    LOGGER_DEBUG("list_remove started");

    error_code error = 0;
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, true);
    )
    RETURN_IF_ERROR(error);
    
    HARD_ASSERT(list != nullptr, "List is nullptr");

    if(remove_index < 0 || (size_t)remove_index >= list->capacity) {
        LOGGER_ERROR("Incorrect index");
        return ERROR_INCORRECT_INDEX;
    } 
    remove_index++;
    if(is_free(&(list->arr[remove_index]))) {
        LOGGER_ERROR("Deleting deleted node");
        return ERROR_INCORRECT_INDEX;
    }

    int prev_index = list->arr[remove_index].prev;
    int next_index = list->arr[remove_index].next;
        
    list->arr[prev_index].next = next_index;
    list->arr[next_index].prev = prev_index;

    list->tail = list->arr[0].prev;
    list->head = list->arr[0].next;
    
    list->arr[remove_index].next = list->free_head;
    list->arr[remove_index].prev = POISON;
    list->arr[remove_index].val = POISON;
    list->free_head = remove_index;
    
    list->size--;
    LOGGER_DEBUG("Removed element at physical index: %d", 
                 remove_index);
    ON_DEBUG(
        error |= list_verify(list, VER_INIT, false);
    )
    return error;
}


error_code list_swap(list_t* list, int first_idx, int second_idx) {
    HARD_ASSERT(list != nullptr, "List is nullptr");
    HARD_ASSERT(list->arr != nullptr, "Arr is nullptr");

    error_code error = 0;
    error |= list_verify(list, VER_INIT, 0);
    RETURN_IF_ERROR(error);

    if(first_idx == 0 || second_idx == 0) {
        LOGGER_ERROR("Cant swap 0 idx");
        return ERROR_INCORRECT_INDEX;
    }

    node_t* first_elem  = &list->arr[first_idx];
    node_t* second_elem = &list->arr[second_idx];

    if(!is_free(first_elem) && !is_free(second_elem)) {
        list->arr[first_elem->next].prev = second_idx;
        list->arr[first_elem->prev].next = second_idx;


        node_t temp_node = *first_elem;
        *first_elem      = *second_elem;
        *second_elem     = temp_node;

    } else if(!is_free(second_elem)) {
        list->arr[second_elem->next].prev = first_idx;
        list->arr[second_elem->prev].next = first_idx;

        *first_elem = *second_elem;
        second_elem->val  = POISON;
        second_elem->prev = POISON;
        second_elem->next = POISON;

    } else {
        list->arr[first_elem->next].prev = second_idx;
        list->arr[first_elem->prev].next = second_idx;

        *second_elem = *first_elem;
        first_elem->val  = POISON;
        first_elem->prev = POISON;
        first_elem->next = POISON;
    }
    error |= list_verify(list, VER_INIT, 0);
    return error;
}


//============================================================================== 

static error_code list_verify(list_t* list, ver_info_t ver_info, bool is_visual_dump) {

    error_code error = 0;
    if(list->arr == nullptr) {
        LOGGER_ERROR("Arr is nullptr");
        error |= ERROR_NULL_ARG;
        list_dump(list, ver_info, false);
        return error;
    }
    if(list->size > list->capacity) {
        LOGGER_ERROR("Size above capacity: %d > %d", list->size, list->capacity);
        error |= ERROR_BIG_SIZE;
    }
    if(list->arr[0].val != CANARY_NUM || list->arr[list->capacity].val != CANARY_NUM) {
        LOGGER_ERROR("Canary changed");
        error |= ERROR_CANARY;
        list_dump(list, ver_info, is_visual_dump);
        return error;
    }
    bool* temp_arr = (bool*)calloc(list->capacity, sizeof(bool));
    if(temp_arr == nullptr) {
        LOGGER_ERROR("Calloc failed");
        list_dump(list, ver_info, false);
        return ERROR_MEM_ALLOC;
    }

    int current = list->head;
    int elem_cnt = 0;
    while (current < (int)list->capacity && current > 0 && current != 0 && elem_cnt <= (int)list->capacity ) {
        temp_arr[current] = true;

        int next = list->arr[current].next;
        if(next >= (int)list->capacity || next < 0)     {
            LOGGER_ERROR("Next is not in arr");                                      
            error |= ERROR_INVALID_STRUCTURE;
            break;
        }
        if(list->arr[next].prev != current) {
            LOGGER_ERROR("Desynchronization between prev and next in %d idx", next); 
            error |= ERROR_INVALID_STRUCTURE;
        }
        elem_cnt++;
        current = next;
    }
    if(elem_cnt > list->capacity) {
        LOGGER_ERROR("Loop detected");
        error |= ERROR_INVALID_STRUCTURE;
    }
    current = list->free_head;
    while (current != -1 && current < (int)list->capacity) {    
        temp_arr[current] = true;
        if(is_free(&(list->arr[current]))) {
            LOGGER_ERROR("Not empty free elem");
            error |= ERROR_NON_ZERO_ELEM;
        }
        current = list->arr[current].next;
    }

    for(size_t i = 1; i < list->capacity; i++) {
        if(!temp_arr[i]) {
            LOGGER_ERROR("Missend elements at idx: %d", i);
            error |= ERROR_MISSED_ELEM;
        }
    }
    free(temp_arr);
    list_dump(list, ver_info, is_visual_dump);
    return error;
}

error_code list_dump(list_t* list, ver_info_t ver_info, bool is_visual_dump) {                                                                                                                                     //TODO: добавить донаты кнопку
    HARD_ASSERT(list != nullptr, "List is nullptr");
    HARD_ASSERT(list->arr != nullptr, "Arr is nullptr");

    static int dump_cnt = 0;
    system("mkdir -p dumps");

    char filename[256];
    snprintf(filename, sizeof(filename), 
             "dumps/dump_%d", dump_cnt);

    list_dump_graphviz(list, filename);
    list_dump_text(list, ver_info, dump_cnt);

    LOGGER_INFO("List dump created: %s.svg", filename);
    dump_cnt++;
    return ERROR_NO;
}

static void list_dump_text(list_t* list, ver_info_t ver_info, int dump_cnt) {
    return ;
}

enum arrow {
    arrow_bad,
    arrow_problem,
    arrow_no,
    arrow_good
};

enum color {
    color_neutral,
    color_good,
    color_bad
};

void list_dump_graphviz(list_t* list, const char* filename) {
    LOGGER_DEBUG("Dump started");
    
    if (list == NULL || filename == NULL) {
        LOGGER_ERROR("Invalid parameters");
        return;
    }
    
    char dot_filename[256];
    char svg_filename[256];
    
    snprintf(dot_filename, sizeof(dot_filename), "%s.dot", filename);
    snprintf(svg_filename, sizeof(svg_filename), "%s.svg", filename);

    FILE* dot_file = fopen(dot_filename, "w");
    if (dot_file == NULL) {
        LOGGER_ERROR("Cannot create dump file: %s", dot_filename);
        return;
    }

    size_t capacity = list->capacity;
    
    // Use dynamic allocation for larger capacities
    color* colors = (color*)calloc(capacity, sizeof(color));
    if (colors == NULL) {
        LOGGER_ERROR("Memory allocation failed for colors");
        fclose(dot_file);
        return;
    }

    // Simplified approach - track arrows in a list instead of 2D array
    fprintf(dot_file, "digraph G {\n");
    fprintf(dot_file, "    orientation=LR;\n");
    fprintf(dot_file, "    rankdir=LR;\n");
    fprintf(dot_file, "    node [shape=rect, style=\"rounded,filled\", fontsize=12];\n");

    // First pass: create all nodes
    for (size_t i = 0; i < capacity; i++) {
        node_t node = list->arr[i];
        const char* fill_color = "lightgray";
        const char* style = "";
        const char* border_color = "black";

        // Determine node color based on content
        if (i == 0) {
            fill_color = "lightblue";  // Head node
        } else if (node.val == POISON) {
            fill_color = "lightcoral";  // Poisoned/free node
        } else {
            fill_color = "lightgreen";  // Valid data node
        }

        // Special styling for important nodes
        if (i == list->arr[0].next) {
            border_color = "blue";
        } else if (i == list->arr[0].prev) {
            border_color = "orange"; 
        } else if (i == list->free_head) {
            border_color = "green";
        }

        fprintf(dot_file,
            "    node%lu [fillcolor=%s, color=%s, penwidth=3, label=\"index: %lu\\nnext: %lu\\nvalue: %d\\nprev: %lu\"];\n",
            i, fill_color, border_color, i, node.next, node.val, node.prev);
    }

    // Create invisible edges for layout
    fprintf(dot_file, "    { rank=same; ");
    for (size_t i = 0; i < capacity; i++) {
        fprintf(dot_file, "node%lu", i);
        if (i < capacity - 1) fprintf(dot_file, " -> ");
    }
    fprintf(dot_file, " [style=invis, weight=100]; }\n");

    // Second pass: create next pointers (green for valid, red for broken)
    for (size_t i = 0; i < capacity; i++) {
        node_t node = list->arr[i];
        
        if (node.next < capacity && node.next != 0) {
            node_t next_node = list->arr[node.next];
            const char* edge_color = "green";
            const char* style = "";
            
            // Check if next pointer is consistent
            if (next_node.prev == i) {
                edge_color = "green";
            } else {
                edge_color = "red";
                style = "penwidth=3";
            }
            
            fprintf(dot_file, "    node%lu -> node%lu [color=%s, %s];\n", 
                    i, node.next, edge_color, style);
        }
    }

    // Third pass: create prev pointers (blue)
    for (size_t i = 0; i < capacity; i++) {
        node_t node = list->arr[i];
        
        if (node.prev < capacity && node.prev != 0) {
            fprintf(dot_file, "    node%lu -> node%lu [color=blue, style=dashed];\n", 
                    i, node.prev);
        }
    }

    // Add free list chain
    size_t free_current = list->free_head;
    while (free_current < capacity && free_current != 0) {
        node_t free_node = list->arr[free_current];
        if (free_node.next < capacity && free_node.next != 0) {
            fprintf(dot_file, "    node%lu -> node%lu [color=gray, style=dotted];\n", 
                    free_current, free_node.next);
        }
        free_current = free_node.next;
    }

    // Add special nodes with connections
    fprintf(dot_file, "    null_elem [shape=ellipse, fillcolor=lightgray, style=filled, label=\"NULL\"];\n");
    fprintf(dot_file, "    null_elem -> node0 [color=brown];\n");

    fprintf(dot_file, "    free_head [shape=ellipse, fillcolor=lightgray, style=filled, label=\"Free Head\"];\n");
    fprintf(dot_file, "    free_head -> node%lu [color=green];\n", list->free_head);

    fprintf(dot_file, "    list_head [shape=ellipse, fillcolor=lightblue, style=filled, label=\"List Head\"];\n");
    fprintf(dot_file, "    list_head -> node%lu [color=blue];\n", list->arr[0].next);

    fprintf(dot_file, "    list_tail [shape=ellipse, fillcolor=orange, style=filled, label=\"List Tail\"];\n");
    fprintf(dot_file, "    list_tail -> node%lu [color=orange];\n", list->arr[0].prev);

    fprintf(dot_file, "}\n");
    
    free(colors);
    fclose(dot_file);

    // Convert to SVG
    char command[512];
    snprintf(command, sizeof(command), "dot -Tsvg \"%s\" -o \"%s\"", dot_filename, svg_filename);
    int result = system(command);
    
    if (result != 0) {
        LOGGER_WARNING("Graphviz conversion failed");
    } else {
        LOGGER_DEBUG("Dump completed successfully: %s", svg_filename);
    }
}       