# pipes [300 баллов]
В этой задаче вам предстоит реализовать пайпы и системный вызов `pipe` внутри файла [`fs/pipe.cpp`](../fs/pipe.cpp). Тесты к задаче запускаются в пространстве пользователя. Их код можно найти в файле [`user/tests/tests_pipe.c`](../user/tests/tests_pipe.c). Обратите внимание на тест `pipe_write_no_readers`, который проверяет работу пайпов, когда у них нет читателей: в этом случае, запись в пайп должна генерировать сигнал `SIGPIPE`. Его можно отправить с помощью функции `kern::SignalSend`

Тесты для этой задачи работают в user space. Чтобы их запустить, достаточно добавить флаг `TEST_PIPE` при сборке через `caos-make`:
```
caos-make clean && caos-make -j... TEST_PIPE=1
```

Если тесты пройдут удачно, вы увидите похожий вывод:

```
==== [ pipe_multiple_writers_multiple_reader ] ====
==== [ OK: pipe_multiple_writers_multiple_reader ] ====
==== [ pipe_multiple_writers_single_reader ] ====
==== [ OK: pipe_multiple_writers_single_reader ] ====
==== [ pipe_single_writer_multiple_readers ] ====
==== [ OK: pipe_single_writer_multiple_readers ] ====
==== [ pipe_drain ] ====
==== [ OK: pipe_drain ] ====
==== [ pipe_write_no_readers ] ====
==== [ OK: pipe_write_no_readers ] ====
==== [ pipe_basic ] ====
==== [ OK: pipe_basic ] ====
All tests passed!
PANIC at sched.cpp:307 [CPU#0] [PID 1]: PID 1 exited with code 61
[0] 0xffffffff80023307 kern::DoPanic(char const*, ...)
[1] 0xffffffff80026467 sched::TaskExit(unsigned int)
[2] 0xffffffff80026892 _Sys_exit
[3] 0xffffffff80028889 DoSyscall
```