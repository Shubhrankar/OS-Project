#include "shell.h"

/*
 * Apply this command's input/output file redirections inside a child.
 * Called after the pipe fds are wired up, so an explicit < or > on a
 * command overrides the pipe for that stream.
 */
static void apply_redirection(command_t *cmd)
{
    if (cmd->infile) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) { perror(cmd->infile); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (cmd->outfile) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->outfile, flags, 0644);
        if (fd < 0) { perror(cmd->outfile); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

int execute_pipeline(pipeline_t *pl)
{
    if (pl->ncmds == 0)
        return 0;

    /* A lone built-in with no pipe runs in the shell itself, so commands
     * like 'cd' actually affect the shell. We still honor redirection by
     * temporarily swapping the shell's own stdin/stdout and restoring it. */
    if (pl->ncmds == 1 && is_builtin(pl->cmds[0].argv[0])) {
        command_t *cmd = &pl->cmds[0];
        int saved_in = -1, saved_out = -1;

        if (cmd->infile) {
            int fd = open(cmd->infile, O_RDONLY);
            if (fd < 0) { perror(cmd->infile); return 1; }
            saved_in = dup(STDIN_FILENO);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (cmd->outfile) {
            int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
            int fd = open(cmd->outfile, flags, 0644);
            if (fd < 0) {
                perror(cmd->outfile);
                if (saved_in != -1) { dup2(saved_in, STDIN_FILENO); close(saved_in); }
                return 1;
            }
            saved_out = dup(STDOUT_FILENO);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        int rc = run_builtin(cmd);

        fflush(stdout);             /* flush before restoring stdout */
        if (saved_in  != -1) { dup2(saved_in,  STDIN_FILENO);  close(saved_in);  }
        if (saved_out != -1) { dup2(saved_out, STDOUT_FILENO); close(saved_out); }
        return rc;
    }

    int   prev_fd = -1;          /* read end of the previous pipe */
    int   pipefd[2];
    pid_t pids[MAX_CMDS];

    for (int i = 0; i < pl->ncmds; i++) {
        command_t *cmd = &pl->cmds[i];
        int has_next = (i < pl->ncmds - 1);

        if (has_next && pipe(pipefd) < 0) {
            perror("pipe");
            return -1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            /* ---- child ---- */
            signal(SIGINT, SIG_DFL);    /* Ctrl-C should kill the command */

            if (prev_fd != -1) {        /* read from previous pipe stage */
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (has_next) {             /* write into the next pipe stage */
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            apply_redirection(cmd);     /* explicit < > override pipes */

            if (is_builtin(cmd->argv[0]))
                exit(run_builtin(cmd));

            execvp(cmd->argv[0], cmd->argv);
            /* execvp only returns on failure */
            fprintf(stderr, "%s: command not found\n", cmd->argv[0]);
            exit(127);
        }

        /* ---- parent ---- */
        pids[i] = pid;
        if (prev_fd != -1)
            close(prev_fd);
        if (has_next) {
            close(pipefd[1]);
            prev_fd = pipefd[0];        /* next stage reads from here */
        }
    }

    if (pl->background) {
        printf("[background] started pid %d\n", (int)pids[pl->ncmds - 1]);
        return 0;                       /* don't wait; reaped at next prompt */
    }

    int status = 0;
    for (int i = 0; i < pl->ncmds; i++)
        waitpid(pids[i], &status, 0);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 0;
}
