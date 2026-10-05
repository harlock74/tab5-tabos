#include <basic/c64.h>
#include <basic/engine.h>
#include <basic/runtime.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--c64-profile") != 0)) {
        fputs("usage: basic [--c64-profile]\n", stderr);
        return 2;
    }
    if (!basic_runtime_init()) {
        fputs("basic: input initialization failed\n", stderr);
        return 1;
    }
    basic_c64_set_profile(argc == 2);
    basic_runtime_write("TabBASIC. Ctrl+C: break; Ctrl+Q: exit.\n");
    const int status = basic_engine_run();
    basic_runtime_shutdown();
    return status;
}
