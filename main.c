#include "shell.h"

/*
 * myshell - main read / parse / execute loop.
 *
 * Signal design:
 *   - The shell ignores nothing permanently, but installs a handler for
 *     SIGINT (Ctrl-C) that just prints a newline. Without SA_RESTART the
 *     blocked read() is interrupted, so we redraw the prompt instead of
 *     killing the shell. Child processes reset SIGINT to the default in
 *     the executor, so Ctrl-C still stops the running command.
 *   - Background children are reaped at the top of each loop iteration
 *     with a non-blocking waitpid(), which avoids racing the foreground
 *     waitpid() in the executor.
 */

static void sigint_handler(int sig)
{
    (void)sig;
    ssize_t r = write(STDOUT_FILENO, "\n", 1);   /* async-signal-safe */
    (void)r;
}

/* Reap any background children that have finished, and report them. */
static void reap_background(void)
{
    int   status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        printf("[background] pid %d finished\n", (int)pid);
}

/* Print a colored prompt showing the current directory's base name. */
static void print_prompt(void)
{
    char cwd[4096];
    const char *dir = "?";
    if (getcwd(cwd, sizeof(cwd))) {
        char *base = strrchr(cwd, '/');
        dir = (base && base[1]) ? base + 1 : cwd;
    }
    /* green "myshell" + blue directory, only when writing to a terminal */
    if (isatty(STDIN_FILENO))
        printf("\033[1;32mmyshell\033[0m:\033[1;34m%s\033[0m$ ", dir);
    else
        printf("myshell:%s$ ", dir);
    fflush(stdout);
}

int main(void)
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                 /* no SA_RESTART: interrupt read() */
    sigaction(SIGINT, &sa, NULL);

    char line[MAX_INPUT];

    while (1) {
        reap_background();
        print_prompt();

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (errno == EINTR) {    /* interrupted by Ctrl-C: redraw */
                clearerr(stdin);
                continue;
            }
            printf("\n");            /* Ctrl-D / end of input */
            break;
        }

        line[strcspn(line, "\n")] = '\0';   /* strip trailing newline */
        if (line[0] == '\0')
            continue;

        history_add(line);

        pipeline_t pl;
        char copy[MAX_INPUT];
        strncpy(copy, line, sizeof(copy));
        copy[sizeof(copy) - 1] = '\0';

        if (parse_line(copy, &pl) > 0)
            execute_pipeline(&pl);

        free_pipeline(&pl);
    }

    history_free();
    return 0;
}
