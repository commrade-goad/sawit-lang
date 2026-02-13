#ifndef ARENA_H
#define ARENA_H

// @TODO: add way to flag that mem size is free to overwrite.
#include <stdlib.h>

#define ARENA_DEFAULT_SIZE 4096

typedef struct ArenaNode {
    struct ArenaNode *next;
    size_t offset;
    size_t cap;
    char *data;
} ArenaNode;

typedef struct {
    ArenaNode *head;
    ArenaNode *current;
} Arena;

int arena_init(Arena *a, size_t size);
int arena_deinit(Arena *a);
int arena_reset(Arena *a);
char *arena_alloc(Arena *a, size_t size);

#ifdef ARENA_IMPLEMENTATION

int arena_init(Arena *a, size_t size) {
    if (!a) return -1;
    if (size == 0) size = ARENA_DEFAULT_SIZE;
    a->head = (ArenaNode *)malloc(sizeof(ArenaNode));
    if (!a->head) return -1;
    *a->head = (ArenaNode){
        .next = NULL,
            .offset = 0,
            .cap = size,
            .data = (char *)malloc(size)
    };
    if (!a->head->data) {
        free(a->head);
        return -1;
    }
    a->current = a->head;

    return 0;
}

int arena_deinit(Arena *a) {
    if (!a) return -1;
    ArenaNode *node = a->head;
    while (node) {
        ArenaNode *next = (ArenaNode *)node->next;
        free(node->data);
        free(node);
        node = next;
    }
    a->head = a->current = NULL;
    return 0;
}

int arena_reset(Arena *a) {
    if (!a) return -1;
    ArenaNode *node = a->head;
    while (node) {
        node->offset = 0;
        node = (ArenaNode *)node->next;
    }
    a->current = a->head;
    return 0;
}

char *arena_alloc(Arena *a, size_t size) {
    if (!a || size == 0) return NULL;

    ArenaNode *node = a->current;

    size = (size + 7) & ~7;
    if (node->offset + size <= node->cap) {
        char *ptr = node->data + node->offset;
        node->offset += size;
        return ptr;
    }

    size_t new_cap = (size > ARENA_DEFAULT_SIZE) ? size : ARENA_DEFAULT_SIZE;
    ArenaNode *new_node = (ArenaNode *)malloc(sizeof(ArenaNode));
    if (!new_node) return NULL;

    new_node->data = (char *)malloc(new_cap);
    if (!new_node->data) {
        free(new_node);
        return NULL;
    }

    new_node->next = NULL;
    new_node->offset = size;
    new_node->cap = new_cap;

    node->next = new_node;
    a->current = new_node;

    return new_node->data;
}

#endif /* ARENA_IMPLEMENTATION */
#endif /* ARENA_H */
