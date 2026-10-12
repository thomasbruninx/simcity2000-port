#include "test.h"
int main() {
    for (auto& t : tests()) { std::printf("[ RUN ] %s\n", t.name); t.fn(); }
    std::printf(failures() ? "%d FAILURE(S)\n" : "all %d tests passed\n", failures() ? failures() : (int)tests().size());
    return failures() ? 1 : 0;
}
