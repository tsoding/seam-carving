#include <assert.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <time.h>

double get_time(void)
{
    struct timespec tp = {};
    int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(ret == 0);
    return tp.tv_sec + tp.tv_nsec*0.000000001;
}

void cc(Nob_Cmd *cmd)
{
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(cmd, "-O3");
}

bool rebuild_stb_if_needed(Nob_Cmd *cmd, const char *implementation, const char *input, const char *output)
{
    if (nob_needs_rebuild1(output, input)) {
        cmd->count = 0;
        cc(cmd);
        nob_cmd_append(cmd, implementation);
        nob_cmd_append(cmd, "-x", "c");
        nob_cmd_append(cmd, "-c");
        nob_cmd_append(cmd, "-o", output);
        nob_cmd_append(cmd, input);
        return nob_cmd_run_sync(*cmd);
    } else {
        nob_log(NOB_INFO, "%s is up to date", output);
        return true;
    }
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);
    (void) program;

    Nob_Cmd cmd = {0};

    if (!nob_mkdir_if_not_exists("./build/")) return 1;
    if (!rebuild_stb_if_needed(&cmd, "-DSTB_IMAGE_IMPLEMENTATION", "stb_image.h", "./build/stb_image.o")) return 1;
    if (!rebuild_stb_if_needed(&cmd, "-DSTB_IMAGE_WRITE_IMPLEMENTATION", "stb_image_write.h", "./build/stb_image_write.o")) return 1;

    const char *main_input = "main.c";
    const char *main_output = "./build/main";
    cmd.count = 0;
    cc(&cmd);
    nob_cmd_append(&cmd, "-o", main_output);
    nob_cmd_append(&cmd, main_input);
    nob_cmd_append(&cmd, "./build/stb_image.o");
    nob_cmd_append(&cmd, "./build/stb_image_write.o");
    nob_cmd_append(&cmd, "-lm");
    if (!nob_cmd_run_sync(cmd)) return 1;

    cmd.count = 0;
    nob_cmd_append(&cmd, main_output);
    nob_da_append_many(&cmd, argv, argc);
    double begin = get_time();
    if (!nob_cmd_run_sync(cmd)) return 1;
    nob_log(NOB_INFO, "Resizing took %lfsecs", get_time() - begin);

    return 0;
}
