COMPILER=arm-linux-gnueabi-gcc
CXXCOMPILER=arm-linux-gnueabi-g++
QEMU=qemu-arm
SYSROOT=/usr/arm-linux-gnueabi/
GDB_PORT=4321

all: test_simple

# чтоб проше тестить
run: FLAGS=-ggdb
run: eval-expression
	$(QEMU) -L $(SYSROOT) eval-expression '$(EXPRESSION)'

run-gdb: FLAGS=-ggdb
run-gdb: eval-expression
	$(QEMU) -L $(SYSROOT) -g $(GDB_PORT) eval-expression "$(EXPRESSION)"

eval-expression: eval-expression.o compiler.o
	$(CXXCOMPILER) $(FLAGS) eval-expression.o compiler.o -o eval-expression

# а тут тесты
test_simple: test
	$(QEMU) -L $(SYSROOT) test

test: test_simple.o compiler.o
	$(CXXCOMPILER) test_simple.o compiler.o -o test

%.o: %.c
	$(COMPILER) $(FLAGS) -c $< -o $@

%.o: %.cpp
	$(CXXCOMPILER) $(FLAGS) -c $< -o $@

clean:
	rm *.o test eval-expression

.PHONY: all run run-gdb clean

