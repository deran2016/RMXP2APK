#pragma once
#include <ruby.h>

/* Match Lunetile's builtin initialization before evaluating code. */
static int ruby_probe_init_builtins(void) {
    char program[] = "ruby-probe", no_gems[] = "--disable-gems", expression[] = "-e", empty[] = "";
    char *options[] = {program, no_gems, expression, empty, NULL};
    int status = 0;
    void *node = ruby_options(4, options);
    return ruby_executable_node(node, &status) && ruby_exec_node(node) == 0 ? 0 : 1;
}
