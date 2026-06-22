#ifndef SHELL_H
#define SHELL_H

/* Enable POSIX functions like strdup() and setenv() */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_TOKENS 256
#define MAX_CMDS   64
#define MAX_INPUT  4096
#define MAX_HISTORY 200

/* One command inside a pipeline, e.g. the "grep foo" in "cat x | grep foo" */
typedef struct {
    char **argv;     /* NULL-terminated argument vector for execvp() */
    int    argc;
    char  *infile;   /* file for input  redirection (<),  or NULL    */
    char  *outfile;  /* file for output redirection (>/>>), or NULL   */
    int    append;   /* 1 if >> (append), 0 if > (truncate)          */
} command_t;

/* A whole command line: one or more commands joined by pipes (|),
 * optionally ending in & to run in the background. */
typedef struct {
    command_t cmds[MAX_CMDS];
    int       ncmds;
    int       background;
} pipeline_t;

/* parser.c */
int  parse_line(char *line, pipeline_t *pl);
void free_pipeline(pipeline_t *pl);

/* executor.c */
int  execute_pipeline(pipeline_t *pl);

/* builtins.c */
int  is_builtin(const char *cmd);
int  run_builtin(command_t *cmd);
void history_add(const char *line);
void history_print(void);
void history_free(void);

#endif /* SHELL_H */
