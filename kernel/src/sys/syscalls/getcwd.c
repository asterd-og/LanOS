#include <sched.h>
#include <heap.h>
#include <string.h>
#include <errno.h>

uint64_t sys_getcwd(context_t *ctx) {
    char *buf = (char*)ctx->rdi;
    size_t size = (size_t)ctx->rsi;

    int count = 0;
    vnode_t *current = this_proc()->cwd;
    while (current != root_node) {
        current = current->parent;
        count++;
    }

    char **names = (char**)kmalloc(count * 8);
    int i = count - 1;
    current = this_proc()->cwd;
    int str_size = 0;
    while (i >= 0) {
        names[i--] = current->name;
        current = current->parent;
        str_size += strlen(current->name) + 1;
    }

    if (str_size + 2 > size) {
        kfree(names);
        return -ERANGE;
    }

    int offset = 1;
    char *final = (char*)kmalloc(str_size + 2); // + 2 for the first / and \0
    final[0] = '/';
    final[1] = 0;
    for (int i = 0; i < count; i++) {
        int len = strlen(names[i]);
        memcpy(final + offset, names[i], len);
        offset += len;
        final[offset] = (i == count - 1 ? 0 : '/');
        offset += 1;
    }

    memcpy(buf, final, size);

    kfree(names);
    kfree(final);
    return 0;
}
