#pragma once
#include <cstdio>
#include <vector>
#include <functional>
struct TestCase { const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& tests() { static std::vector<TestCase> t; return t; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, std::function<void()> f) { tests().push_back({n, f}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()
#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures(); } } while (0)
