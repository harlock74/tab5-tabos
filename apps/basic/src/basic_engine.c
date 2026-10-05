#include <basic/c64.h>
#include <basic/engine.h>
#include <basic/extensions.h>
#include <basic/graphics.h>
#include <basic/runtime.h>
#include <setjmp.h>
#include <stdio.h>

extern int basic_core_main(int argc, char** argv);
static jmp_buf exit_boundary;
static int exit_status;

int basic_engine_run(void)
{
    basic_c64_reset();
    basic_extension_reset();
    exit_status = 0;
    if (setjmp(exit_boundary) == 0) {
        (void) basic_core_main(1, NULL);
    }
    return exit_status;
}

_Noreturn void basic_engine_exit(int status)
{
    basic_sound_close();
    basic_c64_close();
    basic_graphics_close();
    basic_extension_reset();
    exit_status = status;
    longjmp(exit_boundary, 1);
}

void basic_unsupported(void)
{
    basic_runtime_write("\n?UNSUPPORTED IN TABOS STAGE B\n");
}
