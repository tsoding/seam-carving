#define NOB_IMPLEMENTATION
#include "nob.h"

void usage(int exitstatus)
{
    printf("USAGE\n");
    printf("  ./nob file_path\n\n");
    printf("  file_path      the path to your image file\n\n");
    printf("OPTIONS\n");
    printf("  -h, --help     Display this message and exit.\n\n");

    exit(exitstatus);
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cc");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(&cmd, "-O3");
    nob_cmd_append(&cmd, "-o", "main", "main.c");
    nob_cmd_append(&cmd, "-lm");
    if (!nob_cmd_run_sync(cmd)) return 1;

    cmd.count = 0;
    nob_cmd_append(&cmd, "./main");

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(0);
    }
    if (argc != 2) {
        usage(1);
    }

    nob_cmd_append(&cmd, argv[1]);
    if (!nob_cmd_run_sync(cmd)) return 1;

    return 0;
}
