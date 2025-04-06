#include <avl.h>
#include <pmm.h>
#include <vmm.h>
#include <log.h>
#include <stdio.h>
#include <string.h>

#define MAX(a,b) (((a)>(b))?(a):(b))
#define DEPTH(x) (x?x->depth:0)
#define BALANCE(x) (x?(DEPTH(x->left)-DEPTH(x->right)):0)
#define KEY(x) (x->key)

avl_t *avl_new_node(uint64_t key, void *data) {
    avl_t *avl = HIGHER_HALF((avl_t*)pmm_request());
    avl->key = key;
    avl->left = NULL;
    avl->right = NULL;
    avl->depth = 1;
    avl->dup = NULL;
    memcpy(avl->data, data, sizeof(vma_region_t));
    return avl;
}

avl_dup_t *avl_new_dup(avl_dup_t *head, void *data) {
    avl_dup_t *dup = HIGHER_HALF((avl_dup_t*)pmm_request());
    dup->id = (head ? head->prev->id + 1 : 1);
    dup->prev = dup;
    dup->next = dup;
    if (head) {
        dup->prev = head->prev;
        dup->next = head;
        head->prev->next = dup;
        head->prev = dup;
    }
    memcpy(dup->data, data, sizeof(vma_region_t));
    return dup;
}

avl_t *avl_get_min_val_node(avl_t *root) {
    if (!root || !root->left) return root;
    return avl_get_min_val_node(root->left);
}

avl_t *avl_rotate_left(avl_t *z) {
    avl_t *y = z->right;
    avl_t *t2 = y->left;
    y->left = z;
    z->right = t2;
    z->depth = 1 + MAX(DEPTH(z->left), DEPTH(z->right));
    y->depth = 1 + MAX(DEPTH(y->left), DEPTH(y->right));
    return y;
}

avl_t *avl_rotate_right(avl_t *z) {
    avl_t *y = z->left;
    avl_t *t3 = y->right;
    y->right = z;
    z->left = t3;
    z->depth = 1 + MAX(DEPTH(z->left), DEPTH(z->right));
    y->depth = 1 + MAX(DEPTH(y->left), DEPTH(y->right));
    return y;
}

avl_t *avl_insert_node(avl_t *root, uint64_t key, void *data) {
    if (!root)
        return avl_new_node(key, data);
    if (key < KEY(root))
        root->left = avl_insert_node(root->left, key, data);
    else if (key > KEY(root))
        root->right = avl_insert_node(root->right, key, data);
    else {
        avl_dup_t *dup = avl_new_dup(root->dup, data);
        if (!root->dup) root->dup = dup;
        return root;
    }

    root->depth = 1 + MAX(DEPTH(root->left), DEPTH(root->right));

    int64_t balance_factor = BALANCE(root);

    if (balance_factor > 1) {
        if (key < KEY(root->left))
            return avl_rotate_right(root);
        else {
            root->left = avl_rotate_left(root->left);
            return avl_rotate_right(root);
        }
    }

    if (balance_factor < -1) {
        if (key > KEY(root->right))
            return avl_rotate_left(root);
        else {
            root->right = avl_rotate_right(root->right);
            return avl_rotate_left(root);
        }
    }

    return root;
}

avl_t *avl_delete_node(avl_t *root, uint64_t key, uint64_t id) {
    if (!root)
        return root;
    else if (KEY(root) > key)
        root->left = avl_delete_node(root->left, key, id);
    else if (KEY(root) < key)
        root->right = avl_delete_node(root->right, key, id);
    else {
        if (id > 0) {
            uint64_t it = 1;
            avl_dup_t *dup = root->dup;
            if (!dup) return root;
            do {
                if (it == id) {
                    dup->prev->next = dup->next;
                    dup->next->prev = dup->prev;
                    pmm_free(PHYSICAL((void*)dup));
                    if (id == 1)
                        root->dup = NULL;
                    LOG_INFO("Deleted duplicate.\n");
                    break;
                }
                it++;
                dup = dup->next;
                if (dup == root->dup)
                    break;
            } while (1);
            return root;
        }
        avl_t *temp = NULL;
        if (!root->left || !root->right) {
            temp = root->left ? root->left : root->right;
            if (!temp) {
                temp = root;
                root = NULL;
            } else
                *root = *temp;
            pmm_free(PHYSICAL(temp));
        } else {
            temp = avl_get_min_val_node(root->right);
            root->key = temp->key;
            root->right = avl_delete_node(root->right, KEY(temp), id);
        }
    }
    if (!root)
        return root;
    root->depth = 1 + MAX(DEPTH(root->left), DEPTH(root->right));
    int64_t balance_factor = BALANCE(root);
    if (balance_factor > 1) {
        if (BALANCE(root->left) >= 0) return avl_rotate_right(root);
        else {
            root->left = avl_rotate_left(root->left);
            return avl_rotate_right(root);
        }
    }
    if (balance_factor < -1) {
        if (BALANCE(root->right) <= 0) return avl_rotate_left(root);
        else {
            root->right = avl_rotate_right(root->right);
            return avl_rotate_left(root);
        }
    }
    return root;
}

avl_t *avl_search(avl_t *node, uint64_t key) {
    if (!node) return NULL;
    if (KEY(node) == key) return node;
    if (KEY(node) > key) return avl_search(node->left, key);
    return avl_search(node->right, key);
}

void avl_print_tree(avl_t *root, int indent, bool last) {
    if (!root) return;
    printf("%*s", indent, "");
    int temp_indent = indent;
    if (last) {
        printf("R----");
        indent += 5;
    } else {
        printf("L----");
        indent += 4;
    }
    printf("0x%llx%s\n", KEY(root), (root->dup ? " (Has duplicates.)" : ""));
    if (root->left && !last) {
        printf("%*s", temp_indent, "");
        printf("|");
        indent = temp_indent - 1;
    }
    avl_print_tree(root->left, indent, false);
    if (root->right && !last) {
        printf("%*s", temp_indent, "");
        printf("|");
        indent = temp_indent - 1;
    }
    avl_print_tree(root->right, indent, true);
}
