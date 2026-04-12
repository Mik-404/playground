# page-allocator [400 баллов]
Сейчас в HellOS реализован элементарный first-fit аллокатор физических фреймов. Вам нужно реализовать вместо него buddy allocator. Реализация аллокатора состоит из описания трёх функций в файле [`mm/page_alloc.cpp`](../mm/page_alloc.cpp): 1. `mm::Page* DoAllocPage(size_t order, mm::AllocFlags flags)` выделяет `1 << order` последовательных фреймов в физической памяти, флаги можно не учитывать.
2. `void DoFreePage(mm::Page* page)` освобождает страницу по её дескриптору.
3. `void DoInitArea(mm::PageAllocArea* area)` вызывается кодом загрузки и добавляет в аллоктор регион памяти (area), описывыемый структурой `mm::PageAllocArea`. При необходимости в эту структуру можно добавлять новые поля.

Функции-обёртки вокруг общего интерфейса аллокатора находятся в файле [`mm/page_alloc_common.cpp`](../mm/page_alloc_common.cpp)

Тесты для этой задачи компилируются с помощью флага `TEST_PAGE_ALLOC_BASIC` и `TEST_PAGE_ALLOC_FRAGMENTATION`.
