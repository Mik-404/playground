#ifndef COMPILER_H
#define COMPILER_H

typedef struct {
    char name[256];
    void *addr;
} symbol_t;

#ifdef __cplusplus
extern "C" {
#endif

int compile(const char *expression, const symbol_t **symbol_table, void *opcodes_out);

#ifdef __cplusplus
}
#endif

#endif  // COMPILER_H
