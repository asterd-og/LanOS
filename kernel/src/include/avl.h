#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct avl_dup_t {
    struct avl_dup_t *next;
    struct avl_dup_t *prev;
    uint64_t id;
    uint64_t data[3];
} avl_dup_t;

typedef struct avl_t {
    uint64_t key;
    struct avl_t *left;
    struct avl_t *right;
    avl_dup_t *dup;
    int64_t depth;
    uint64_t data[3];
} avl_t;

avl_t *avl_insert_node(avl_t *root, uint64_t key, void *data);
avl_t *avl_delete_node(avl_t *root, uint64_t key, uint64_t id);
avl_t *avl_search(avl_t *node, uint64_t key);
void avl_print_tree(avl_t *root, int indent, bool last);
