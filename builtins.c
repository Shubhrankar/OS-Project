#include "shell.h"

/* ----- command history (kept in memory) ----- */
static char *history_list[MAX_HISTORY];
static int   history_count = 0;

void history_add(const char *line)
{
    /* ignore blank lines */
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0' || *s == '\n')
        return;

    if (history_count == MAX_HISTORY) {     /* buffer full: drop oldest */
        free(history_list[0]);
        memmove(&history_list[0], &history_list[1],
                (MAX_HISTORY - 1) * sizeof(char *));
        history_count--;
    }
    history_list[history_count++] = strdup(line);
}

void history_print(void)
{
    for (int i = 0; i < history_count; i++)
        printf("%5d  %s\n", i + 1, history_list[i]);
}

void history_free(void)
{
    for (int i = 0; i < history_count; i++)
        free(history_list[i]);
    history_count = 0;
}

/* ----- built-in dispatch ----- */
int is_builtin(const char *cmd)
{
    if (!cmd) return 0;
    return strcmp(cmd, "cd")     == 0 ||
           strcmp(cmd, "exit")   == 0 ||
           strcmp(cmd, "pwd")    == 0 ||
           strcmp(cmd, "help")   == 0 ||
           strcmp(cmd, "echo")   == 0 ||
           strcmp(cmd, "export") == 0 ||
           strcmp(cmd, "unset")  == 0 ||
           strcmp(cmd, "history")== 0;
}

/* Built-ins return an exit-status-like int (0 = success). 'exit' never
 * returns. Most built-ins MUST run in the shell process itself; for example
 * 'cd' in a child process would change only the child's directory. */
int run_builtin(command_t *cmd)
{
    char **argv = cmd->argv;

    if (strcmp(argv[0], "exit") == 0) {
        int code = argv[1] ? atoi(argv[1]) : 0;
        history_free();
        exit(code);
    }

    if (strcmp(argv[0], "cd") == 0) {
        const char *dir = argv[1] ? argv[1] : getenv("HOME");
        if (!dir) dir = "/";
        if (chdir(dir) != 0)
            perror("cd");
        return 0;
    }

    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)))
            printf("%s\n", cwd);
        else
            perror("pwd");
        return 0;
    }

    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i]; i++)
            printf("%s%s", argv[i], argv[i + 1] ? " " : "");
        printf("\n");
        return 0;
    }

    if (strcmp(argv[0], "export") == 0) {
        /* export NAME=VALUE */
        for (int i = 1; argv[i]; i++) {
            char *eq = strchr(argv[i], '=');
            if (eq) {
                *eq = '\0';
                setenv(argv[i], eq + 1, 1);
                *eq = '=';
            }
        }
        return 0;
    }

    if (strcmp(argv[0], "unset") == 0) {
        for (int i = 1; argv[i]; i++)
            unsetenv(argv[i]);
        return 0;
    }

    if (strcmp(argv[0], "history") == 0) {
        history_print();
        return 0;
    }

    if (strcmp(argv[0], "help") == 0) {
        printf("myshell - a small Unix shell\n\n");
        printf("Built-in commands:\n");
        printf("  cd [dir]          change directory\n");
        printf("  pwd               print working directory\n");
        printf("  echo [args...]    print arguments\n");
        printf("  export NAME=VAL   set an environment variable\n");
        printf("  unset NAME        remove an environment variable\n");
        printf("  history           show command history\n");
        printf("  help              show this message\n");
        printf("  exit [code]       leave the shell\n\n");
        printf("Features: pipes (|), redirection (< > >>),\n");
        printf("background jobs (&), quotes, and $VAR expansion.\n");
        return 0;
    }

    return 0;
}
