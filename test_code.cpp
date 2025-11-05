// list_dumper.c
// Senior-style dump for circular list stored in array + free list.
// Generates append-only dump/dump.html and graph images via graphviz (dot).
// Compile: gcc -std=c11 -O2 -o list_dumper list_dumper.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>

#define POISON -1            // как сказал пользователь
#define DUMP_DIR "dump"
#define HTML_FILE DUMP_DIR "/dump.html"

typedef struct {
    int next;
    int prev;
    double val;
} node_t;

typedef struct {
    node_t *arr;     // массив capacity элементов
    int capacity;    // полный размер массива
    int size;        // сколько элементов используется логически
    int free_head;   // индекс головы однонаправленного списка свободных ячеек (в тех же ячейках)
} list_t;

/* --- Вспомогательные структуры для формирования .dot --- */
typedef struct {
    int u;
    int v;
    char *attr; // атрибуты для ребра, например "dir=both,color=gray,constraint=false"
} edge_t;

typedef struct {
    edge_t *buf;
    size_t used;
    size_t cap;
} edge_vec;

static void edge_vec_init(edge_vec *v){ v->buf = NULL; v->used = v->cap = 0; }
static void edge_vec_push(edge_vec *v, int u, int vv, const char *attr){
    if(v->used == v->cap){
        v->cap = v->cap ? v->cap * 2 : 64;
        v->buf = (edge_t*)realloc(v->buf, v->cap * sizeof(edge_t));
        if(!v->buf){ perror("realloc"); exit(2); }
    }
    v->buf[v->used].u = u;
    v->buf[v->used].v = vv;
    v->buf[v->used].attr = strdup(attr ? attr : "");
    v->used++;
}
static void edge_vec_free(edge_vec *v){
    for(size_t i=0;i<v->used;++i) free(v->buf[i].attr);
    free(v->buf);
    v->buf = NULL;
    v->used = v->cap = 0;
}

/* --- ensure dump dir exists --- */
static void ensure_dump_dir(void){
#ifdef _WIN32
    _mkdir(DUMP_DIR);
#else
    if (mkdir(DUMP_DIR, 0755) && errno != EEXIST){
        fprintf(stderr, "Не удалось создать каталог %s: %s\n", DUMP_DIR, strerror(errno));
        exit(3);
    }
#endif
}

/* --- текстовый короткий дамп: индекс | prev | next | val (val помечен POISON если соответствует) --- */
static void write_text_dump_to_html(const char *fname_html, const char *text, const list_t *lst){
    FILE *f = fopen(fname_html, "a");
    if(!f){
        fprintf(stderr, "Не могу открыть %s для дозаписи: %s\n", fname_html, strerror(errno));
        return;
    }
    fprintf(f, "<pre>\n");
    if(text && text[0]) fprintf(f, "%s\n", text);
    fprintf(f, "capacity=%d size=%d free_head=%d\n", lst->capacity, lst->size, lst->free_head);
    for(int i=0;i<lst->capacity;++i){
        const node_t *n = &lst->arr[i];
        if (n->prev == POISON && (int) (int)n->val == POISON) {
            // свободная ячейка: val и prev == POISON (в задании)
            fprintf(f, "idx:%3d | FREE (prev=POISON val=POISON) | next:%4d\n", i, n->next);
        } else {
            // обычная
            if (i == 0) // в 0-м — канарейка (спец)
                fprintf(f, "idx:%3d | prev:%4d | next:%4d | val:%g  <-- CANARY\n", i, n->prev, n->next, n->val);
            else
                fprintf(f, "idx:%3d | prev:%4d | next:%4d | val:%g\n", i, n->prev, n->next, n->val);
        }
    }
    fprintf(f, "</pre>\n<br>\n");
    fclose(f);
}

/* --- Generating .dot according to spec --- */
static void generate_dot_and_png(const list_t *lst, const char *basepath_dot, const char *basepath_png){
    // basepath_dot: dump/dump_001.dot
    // basepath_png: dump/dump_001.png
    FILE *f = fopen(basepath_dot, "w");
    if(!f){
        fprintf(stderr, "Не могу создать %s: %s\n", basepath_dot, strerror(errno));
        return;
    }

    fprintf(f, "digraph G {\n");
    fprintf(f, "    rankdir=LR; \n");
    fprintf(f, "    node [shape=record, fontsize=10];\n\n");

    // 1) создаём узлы для всех индексов
    for(int i=0;i<lst->capacity;++i){
        const node_t *n = &lst->arr[i];
        char label[256];
        if (n->prev == POISON && (int)(int)n->val == POISON) {
            // free cell
            snprintf(label, sizeof(label), "<idx>%d | <prev>prev:POISON | <next>next:%d | <val>val:POISON", i, n->next);
            fprintf(f, "    n%d [label=\"%s\", style=dashed];\n", i, label);
        } else {
            snprintf(label, sizeof(label), "<idx>%d | <prev>prev:%d | <next>next:%d | <val>val:%g", i, n->prev, n->next, n->val);
            if (i==0)
                fprintf(f, "    n%d [label=\"%s\", color=darkgoldenrod, style=filled];\n", i, label);
            else
                fprintf(f, "    n%d [label=\"%s\"];\n", i, label);
        }
    }

    // коллекция пропущенных узлов (ссылки на несуществующие индексы) будем добавлять по мере обнаружения
    edge_vec edges;
    edge_vec_init(&edges);

    // 2) невидимая последовательность по индексам (фиксирует расположение)
    for(int i=0;i+1<lst->capacity;++i){
        fprintf(f, "    n%d -> n%d [style=invis, constraint=true];\n", i, i+1);
    }
    fprintf(f, "\n");

    // 3) проходим и строим рёбра по логике:
    //    - по умолчанию: ni -> nj (dir=both, color=gray, constraint=false)
    //    - если nj.invalid -> направляем на missing_j (красный dashed)
    //    - если arr[nj].prev != i: добавляем корректирующее ребро nj -> nk (nk = arr[nj].prev) жирное чёрное; заменяем ni->nj на одностороннюю (dir=forward) и цвет меняем
    //    Аналогично проводим проверку для prev: если arr[i].prev == p и arr[p].next != i, добавляем p->q (q = arr[p].next) жирное, и меняем p->i в цвет другой.
    //
    // Для простоты реализации: мы формируем окончательные ребра в edges и затем их печатаем.

    // helper для добавления missing node: возвращает node id (negative reserved? we will emit new node missing_X)
    // но проще: при обнаружении missing сразу добавляем node missing_X в файл
    // Чтобы избежать дублирования missing node emission, используем простую временную запись
    int *missing_emitted = (int*)calloc(lst->capacity + 1024, sizeof(int)); // индекс -> bool, индексы >= capacity use hashing trick
    // if index >= capacity, we store in missing_emitted[idx % (cap+1024)] maybe collisions; but collisions unlikely. Simpler: allocate map for indices up to capacity+1000; if bigger, always re-create labels anyway.
    // To be safe, let's use a dynamic method: we'll emit missing nodes when first seen and track them in a small dynamic array keyed by requested index using linked list approach.
    // For brevity и надёжности — если index outside range, мы всегда emit node missing_X (duplication allowed — не критично).

    for(int i=0;i<lst->capacity;++i){
        int j = lst->arr[i].next;
        if (j < 0 || j >= lst->capacity){
            // next points to non-existing index
            // emit missing node and edge
            fprintf(f, "    missing_next_%d [label=\"MISSING idx:%d\", color=red, style=filled];\n", j, j);
            edge_vec_push(&edges, i, -1 - j, "dir=forward,color=red,style=dashed,constraint=false"); // encode missing as negative id: -1 - j
        } else {
            // default both-way
            // We'll decide attributes later if mismatch occurs. For now, check mismatch:
            if (lst->arr[j].prev != i){
                // mismatch: add ni->nj as forward blue and add nj -> nk (where nk = arr[j].prev) bold black
                edge_vec_push(&edges, i, j, "dir=forward,color=blue,constraint=false");
                int k = lst->arr[j].prev;
                if (k < 0 || k >= lst->capacity){
                    fprintf(f, "    missing_prev_of_%d [label=\"MISSING idx:%d\", color=yellow, style=filled];\n", j, k);
                    edge_vec_push(&edges, -1 - j, -1 - k, "dir=forward,color=black,penwidth=2,constraint=false"); // encode from missing node of j to missing k
                    // Also draw real-looking edge from n{j} to missing_prev_of_j:
                    edge_vec_push(&edges, j, -1 - k, "dir=forward,color=black,penwidth=2,constraint=false");
                } else {
                    edge_vec_push(&edges, j, k, "dir=forward,color=black,penwidth=2,constraint=false");
                }
                // highlight target node (nj) color: yellow fill - we'll emit a node attribute override below
                fprintf(f, "    n%d [style=filled, fillcolor=yellow];\n", j);
            } else {
                // OK: consistent prev; draw default bidirectional
                edge_vec_push(&edges, i, j, "dir=both,color=gray,constraint=false");
            }
        }
        // Now check prev inconsistency symmetry:
        int p = lst->arr[i].prev;
        if (p < 0 || p >= lst->capacity){
            // prev points to missing — draw missing node and edge p->i
            if (p != POISON){
                fprintf(f, "    missing_prevptr_%d [label=\"MISSING prev idx:%d\", color=orange, style=filled];\n", p, p);
                edge_vec_push(&edges, -1 - p, i, "dir=forward,color=orange,style=dashed,constraint=false");
            }
        } else {
            if (lst->arr[p].next != i){
                // inconsistency: prev claims p but p.next != i
                // color p->... pen
                int q = lst->arr[p].next;
                // change p->i to single-direction and recolor (we will add such an edge)
                edge_vec_push(&edges, p, i, "dir=forward,color=green,constraint=false");
                // add bold edge p -> q (or missing) to show wrong pointer
                if (q < 0 || q >= lst->capacity){
                    fprintf(f, "    missing_next_of_prev_%d [label=\"MISSING idx:%d\", color=red, style=filled];\n", q, q);
                    edge_vec_push(&edges, p, -1 - q, "dir=forward,color=black,penwidth=2,constraint=false");
                } else {
                    edge_vec_push(&edges, p, q, "dir=forward,color=black,penwidth=2,constraint=false");
                }
                // highlight node i (target of inconsistent prev) with another color
                fprintf(f, "    n%d [style=filled, fillcolor=lightblue];\n", i);
            }
        }
    }

    // Печатаем все edges (включая специальные negative-coded missing)
    fprintf(f, "\n    // edges\n");
    for(size_t e=0;e<edges.used;++e){
        int u = edges.buf[e].u;
        int v = edges.buf[e].v;
        char *attr = edges.buf[e].attr;
        if (u < 0){
            // missing source: encoded as -1 - idx
            int miss_idx = -1 - u;
            fprintf(f, "    missing_src_%d [label=\"MISSING idx:%d\", color=red, style=filled];\n", miss_idx, miss_idx);
            if (v < 0){
                int miss_v = -1 - v;
                fprintf(f, "    missing_src_%d -> missing_src_%d [%s];\n", miss_idx, miss_v, attr);
            } else {
                fprintf(f, "    missing_src_%d -> n%d [%s];\n", miss_idx, v, attr);
            }
        } else {
            if (v < 0){
                int miss_v = -1 - v;
                fprintf(f, "    n%d -> missing_src_%d [%s];\n", u, miss_v, attr);
            } else {
                fprintf(f, "    n%d -> n%d [%s];\n", u, v, attr);
            }
        }
    }

    fprintf(f, "}\n");
    fclose(f);

    // очистка временной памяти
    edge_vec_free(&edges);
    free(missing_emitted);
    // вызов dot через system
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dot -Tpng -o \"%s\" \"%s\"", basepath_png, basepath_dot);
    int rc = system(cmd);
    if(rc != 0){
        fprintf(stderr, "Вызов dot завершился с кодом %d (команда: %s). Убедись, что graphviz установлен и доступен в PATH.\n", rc, cmd);
    }
}

/* --- Статический счётчик дампов и родительская функция --- */
void dump(const list_t *lst, const char *text, int with_graph){
    static int dump_counter = 0;
    ensure_dump_dir();
    dump_counter++;
    char dotname[512], pngname[512];
    snprintf(dotname, sizeof(dotname), DUMP_DIR "/dump_%03d.dot", dump_counter);
    snprintf(pngname, sizeof(pngname), DUMP_DIR "/dump_%03d.png", dump_counter);

    // всегда записываем текстовую часть в HTML
    write_text_dump_to_html(HTML_FILE, text, lst);

    if(with_graph){
        generate_dot_and_png(lst, dotname, pngname);
        // вставляем ссылку на картинку в HTML (append)
        FILE *f = fopen(HTML_FILE, "a");
        if(f){
            fprintf(f, "<img src=\"%s\" alt=\"dump_%03d\"><br>\n", pngname, dump_counter);
            fprintf(f, "<br>\n");
            fclose(f);
        } else {
            fprintf(stderr, "Не могу дозаписать %s для добавления ссылки на png\n", HTML_FILE);
        }
    }
}

/* --- Упрощённые API, как просил: --- */
// только текст
void dump_text_only(const list_t *lst, const char *text){
    dump(lst, text, 0);
}
// текст + картинка
void dump_with_graph(const list_t *lst, const char *text){
    dump(lst, text, 1);
}
#define DEMO_MAIN
/* --- Небольшой тест (пример использования) --- */
#ifdef DEMO_MAIN
int main(void){
    // небольшой самопроверочный пример
    int cap = 8;
    node_t *arr = (node_t*)calloc(cap, sizeof(node_t));
    // простой цепочный список 0..4, остальные свободны
    arr[0].val = 0;
    arr[0].next = 1;
    arr[0].prev = 5;
    for(int i=1;i<cap;i++){
        arr[i].next = (i+1 < cap ? i+1 : POISON);
        arr[i].prev = (i-1 >= 0 ? i-1 : POISON);
        arr[i].val = i * 1.5;
    }
    // делаем специально несогласованность: arr[2].next = 5, но arr[5].prev != 2
    arr[2].next = 5;
    arr[5].prev = 0; // несоответствие
    // свободные ячейки: пометим 6 и 7 как свободные
    arr[6].next = 6; arr[6].prev = POISON; arr[6].val = (double)POISON;
    arr[7].next = 6; arr[7].prev = POISON; arr[7].val = (double)POISON;

    list_t lst = { .arr = arr, .capacity = cap, .size = 6, .free_head = 7 };

    dump_with_graph(&lst, "Тестовый дамп: демонстрация несогласованностей");
    dump_text_only(&lst, "Только текстовый дамп после первого");

    free(arr);
    return 0;
}
#endif