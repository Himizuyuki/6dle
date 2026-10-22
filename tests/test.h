#ifndef TEST_H
#define TEST_H

// Minimal test framework: no dependency beyond libc.
//   CHECK*   record a failure and keep going
//   REQUIRE  record a failure and leave the current test

#include <stdio.h>
#include <string.h>

typedef void (*TestFn)(void);

void test_run(const char *name, TestFn fn);
int test_check(int ok, const char *file, int line, const char *fmt, ...);

#define RUN(fn) test_run(#fn, fn)

#define CHECK(cond) test_check(!!(cond), __FILE__, __LINE__, "%s", #cond)

#define REQUIRE(cond)                                                          \
    do                                                                         \
    {                                                                          \
        if (!test_check(!!(cond), __FILE__, __LINE__, "%s", #cond))            \
            return;                                                            \
    } while (0)

#define CHECK_INT(a, b)                                                        \
    do                                                                         \
    {                                                                          \
        long long a_ = (long long)(a);                                         \
        long long b_ = (long long)(b);                                         \
        test_check(a_ == b_, __FILE__, __LINE__, "%s == %s (%lld vs %lld)",    \
                   #a, #b, a_, b_);                                            \
    } while (0)

#define CHECK_STR(a, b)                                                        \
    do                                                                         \
    {                                                                          \
        const char *a_ = (a);                                                  \
        const char *b_ = (b);                                                  \
        test_check(strcmp(a_, b_) == 0, __FILE__, __LINE__,                    \
                   "%s == %s (\"%s\" vs \"%s\")", #a, #b, a_, b_);             \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                  \
    do                                                                         \
    {                                                                          \
        double a_ = (double)(a);                                               \
        double b_ = (double)(b);                                               \
        test_check(a_ - b_ <= (tol) && b_ - a_ <= (tol), __FILE__, __LINE__,   \
                   "%s ~= %s (%g vs %g)", #a, #b, a_, b_);                     \
    } while (0)

// Paths baked in by the Makefile so tests run from any directory.
#define FIXTURE(name) SIXDLE_ROOT "/tests/data/" name

// One function per test file, called from tests/main.c.
void words_tests(void);
void pattern_tests(void);
void game_tests(void);
void knowledge_tests(void);
void expert_tests(void);
void nn_tests(void);
void solver_tests(void);

#endif // TEST_H
