#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "compiler.h"


int16_t mini16(int16_t a, int16_t b) {
    return (a < b) ? a : b;
}

int16_t maxi16(int16_t a, int16_t b) {
    return (a > b) ? a : b;
}

int16_t if_statement(int16_t a, int16_t b, int16_t x, int16_t y) {
    return (a < b) ? x : y;
}

int main(int argc, char **argv) {
    const size_t FUNCTION_SIZE = 4 * 1024;  // 4Kb

    if (argc != 2) {
        dprintf(
            STDERR_FILENO,
            "Usage:\n"
            "\t%s expression\n",
            argv[0]
        );
        return 1;
    }

    const char *expression = argv[1];

    void *function_opcodes = mmap(
        /* addr = */ NULL,
        /* length = */ FUNCTION_SIZE,
        /* prot = */ PROT_READ | PROT_WRITE,
        /* flags = */ MAP_PRIVATE | MAP_ANON,
        /* fd = */ -1,
        /* offset */ 0
    );

    if (function_opcodes == MAP_FAILED) {
        perror("mmap");
        return 2;
    }

    int values[] = {42, -11, 314, -8};
    symbol_t symbols[] = {
        // variables
        (symbol_t){.name="A", .addr=&values[0]},
        (symbol_t){.name="ELEVEN", .addr=&values[1]},
        (symbol_t){.name="PI100", .addr=&values[2]},
        (symbol_t){.name="X", .addr=&values[3]},

        // functions
        (symbol_t){.name="min", .addr=mini16},
        (symbol_t){.name="max", .addr=maxi16},
        (symbol_t){.name="if", .addr=if_statement}
    };

    symbol_t **table = (symbol_t**)malloc(sizeof(symbol_t**) * (sizeof(symbols) + 1));
    int i;
    for (i = 0; i < sizeof(symbols) / sizeof(symbol_t); ++i) {
        table[i] = symbols + i;
    }
    table[i] = NULL;

    int compilation_succsess = compile(expression, (const symbol_t**)table, function_opcodes);

    if (compilation_succsess != 0) {
        dprintf(
            STDERR_FILENO,
            "compilation failed with code: %d\n",
            compilation_succsess
        );
        return 3;
    }

    if (mprotect(function_opcodes, FUNCTION_SIZE, PROT_EXEC) == -1) {
        perror("cannot change protection");
        return 4;
    }

    int (*compiled_expression)(void) = function_opcodes;
    int expression_value = compiled_expression();

    printf("%d\n", expression_value);

    free(table);

    return 0;
}
