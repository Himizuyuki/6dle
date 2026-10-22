#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "test.h"

static const char *filter;
static int failures_in_test;
static int tests_run;
static int tests_failed;
static long checks;

int test_check(int ok, const char *file, int line, const char *fmt, ...)
{
    checks++;
    if (ok)
        return 1;
    failures_in_test++;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "    %s:%d: check failed: ", file, line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    return 0;
}

void test_run(const char *name, TestFn fn)
{
    if (filter != NULL && strstr(name, filter) == NULL)
        return;
    failures_in_test = 0;
    clock_t start = clock();
    fn();
    double ms = 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
    tests_run++;
    if (failures_in_test)
        tests_failed++;
    printf("  %s %-52s %7.1f ms\n", failures_in_test ? "FAIL" : "ok  ", name,
           ms);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    // Optional argument: only run tests whose name contains it.
    filter = argc > 1 ? argv[1] : NULL;
    struct
    {
        const char *name;
        void (*run)(void);
    } suites[] = {
        { "words", words_tests },   { "pattern", pattern_tests },
        { "game", game_tests },     { "knowledge", knowledge_tests },
        { "expert", expert_tests }, { "nn", nn_tests },
        { "solver", solver_tests },
    };
    for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
    {
        printf("%s\n", suites[i].name);
        suites[i].run();
    }
    printf("\n%d tests, %ld checks, %d failed\n", tests_run, checks,
           tests_failed);
    return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
