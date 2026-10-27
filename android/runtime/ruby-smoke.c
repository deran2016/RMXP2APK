#include <ruby.h>
#include <stdio.h>
#include "ruby-probe-options.h"

/* Standalone VM smoke test. It does not load a game or implement RGSS. */
int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (ruby_setup() != 0) {
        fputs("ruby_setup failed\n", stderr);
        return 1;
    }
    if (ruby_probe_init_builtins()) {
        fputs("Ruby builtin initialization failed\n", stderr);
        ruby_cleanup(1);
        return 1;
    }
    int state = 0;
    VALUE result = rb_eval_string_protect("1 + 2", &state);
    if (state || !RB_INTEGER_TYPE_P(result) || NUM2INT(result) != 3) {
        fputs("Ruby expression evaluation failed\n", stderr);
        ruby_cleanup(1);
        return 1;
    }
    puts("Ruby expression evaluation passed");
    return ruby_cleanup(0);
}
