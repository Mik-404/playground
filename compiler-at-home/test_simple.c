#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "compiler.h"


const size_t FUNCTION_SIZE = 4 * 1024;  // 4Kb

typedef struct {
    char expression[256];
    int result;
} test_case_t;

int compilation_success = 0;

int eval(const char *expression, const symbol_t **table, void *opcodes) {
    if (mprotect(opcodes, FUNCTION_SIZE, PROT_READ | PROT_WRITE) == -1) {
        perror("cannot change protection for compilation");
        exit(4);
    }

    int compilation_success = compile(expression, table, opcodes);

    if (compilation_success != 0) {
        dprintf(
            STDERR_FILENO,
            "compilation failed with code: %d\n",
            compilation_success
        );

        compilation_success = 3;
        return 0;
    }
    compilation_success = 0;

    if (mprotect(opcodes, FUNCTION_SIZE, PROT_EXEC) == -1) {
        perror("cannot change protection for running");
        exit(5);
    }

    int (*compiled_expression)(void) = opcodes;

    return compiled_expression();
}

int16_t mini16(int16_t a, int16_t b) {
    return (a < b) ? a : b;
}

int16_t maxi16(int16_t a, int16_t b) {
    return (a > b) ? a : b;
}


int16_t if_statement(int16_t a, int16_t b, int16_t x, int16_t y) {
    return (a < b) ? x : y;
}

int16_t five() {
    return 5;
}


int main() {
    void *function_opcodes = mmap(
        /* addr = */ NULL,
        /* length = */ FUNCTION_SIZE,
        /* prot = */ PROT_NONE,  // поменяются на нужные при необходимости
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
        (symbol_t){.name="MELEVEN", .addr=&values[1]},
        (symbol_t){.name="PI100", .addr=&values[2]},
        (symbol_t){.name="X", .addr=&values[3]},

        // functions
        (symbol_t){.name="min", .addr=mini16},
        (symbol_t){.name="max", .addr=maxi16},
        (symbol_t){.name="five", .addr=five},
        (symbol_t){.name="if", .addr=if_statement}
    };

    symbol_t **table = (symbol_t**)malloc(sizeof(symbol_t**) * (sizeof(symbols) + 1));
    int i;
    for (i = 0; i < sizeof(symbols) / sizeof(symbol_t); ++i) {
        table[i] = symbols + i;
    }
    table[i] = NULL;


    test_case_t tests[] = {
        (test_case_t){.expression="2+2*2", .result=6},
        (test_case_t){.expression="2*2+2", .result=6},
        (test_case_t){.expression="(2+2)*2", .result=8},
        (test_case_t){.expression="2*(2+2)", .result=8},
        (test_case_t){.expression="2+(2*2)", .result=6},
        (test_case_t){.expression="1", .result=1},
        (test_case_t){.expression="-7", .result=-7},
        (test_case_t){.expression="(2)", .result=2},
        (test_case_t){.expression="five()*five() + five()", .result=30},
        (test_case_t){.expression="( 2 - 9 )", .result=-7},
        (test_case_t){.expression="( 1 + ( 1 + ( 1 + ( 1 + (1 + ( 1 + 2 ) ) ) ) ) )", .result=8},
        (test_case_t){.expression="4 - A * 2", .result=-80},
        (test_case_t){.expression="MELEVEN * 2 - PI100", .result=-336},
        (test_case_t){.expression="X * A", .result=-336},
        (test_case_t){.expression=" min ( 1     ,  2 ) ", .result=1},
        (test_case_t){.expression=" max ( min(1, 2),  min(3, 4)) ", .result=3},
        (test_case_t){.expression=" min ( min(-4, -3), min(-2, -1) ) ", .result=-4},
        (test_case_t){.expression="min(A * 4, MELEVEN)", .result=-11},
        (test_case_t){.expression=" if ( 1, 2, A, PI100 ) * MELEVEN", .result=-462},
        (test_case_t){
            .expression=
                " if ( "
                    "if(1, 1, 4, 10),"
                    "if(10, 2, 1, 3),"
                    "if(11, 10, 2, 2),"
                    "if(A, MELEVEN, PI100, X)"
                ") * max("
                    "min ( MELEVEN, PI100 ),"
                    "max (X * PI100, MELEVEN * PI100)"
                ")",
            .result=88
        }
    };

    for (int i = 0; i < sizeof(tests) / sizeof(test_case_t); ++i) {
        const char *expression = (const char*)tests[i].expression;
        int expected = tests[i].result;
        int result = eval(expression, (const symbol_t**)table, function_opcodes);

        if (compilation_success != 0) {
            dprintf(
                STDERR_FILENO,
                "compilation error %d at expression `%s`\n",
                compilation_success,
                expression
            );
            exit(1);
        }

        if (expected != result) {
            dprintf(
                STDERR_FILENO,
                "expected %d, but got %d at expression `%s`\n",
                expected,
                result,
                expression
            );
            exit(2);
        }
    }

    free(table);

    if (munmap(function_opcodes, FUNCTION_SIZE) != 0) {
        perror("munmap");
        return 5;
    }

    puts("we have compiler at home!");

    return 0;
}
