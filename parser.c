#include "shell.h"

/*
 * The parser turns a raw input line into a pipeline_t.
 * It works in two passes:
 *   1) tokenize()   - split the line into words and operator tokens,
 *                     handling quotes and $VARIABLE expansion.
 *   2) parse_line() - walk the tokens and fill in commands, the
 *                     redirection targets, and the background flag.
 */

/* Read a $NAME (or ${NAME}) starting at *pp and append its value to buf. */
static void expand_var(const char **pp, char *buf, int *bi)
{
    const char *p = *pp;
    char name[256];
    int  ni = 0;

    p++; /* skip '$' */
    if (*p == '{') {
        p++;
        while (*p && *p != '}' && ni < (int)sizeof(name) - 1)
            name[ni++] = *p++;
        if (*p == '}')
            p++;
    } else {
        while (*p && (isalnum((unsigned char)*p) || *p == '_') &&
               ni < (int)sizeof(name) - 1)
            name[ni++] = *p++;
    }
    name[ni] = '\0';

    const char *val = getenv(name);
    if (val)
        while (*val)
            buf[(*bi)++] = *val++;

    *pp = p;
}

/* Split the line into tokens. Operators (| < > >> &) become their own
 * tokens. Returns the number of tokens; tokens[] is NULL-terminated. */
static int tokenize(const char *line, char *tokens[])
{
    int   ntok = 0;
    const char *p = line;
    char  buf[MAX_INPUT];

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '#')   /* '#' starts a comment */
            break;

        /* operator tokens */
        if (*p == '|') { tokens[ntok++] = strdup("|"); p++; continue; }
        if (*p == '<') { tokens[ntok++] = strdup("<"); p++; continue; }
        if (*p == '&') { tokens[ntok++] = strdup("&"); p++; continue; }
        if (*p == '>') {
            if (*(p + 1) == '>') { tokens[ntok++] = strdup(">>"); p += 2; }
            else                 { tokens[ntok++] = strdup(">");  p++;   }
            continue;
        }

        /* a word: may contain quotes and variables */
        int bi = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '|' &&
               *p != '<' && *p != '>' && *p != '&') {
            if (*p == '\'') {                 /* single quotes: literal */
                p++;
                while (*p && *p != '\'')
                    buf[bi++] = *p++;
                if (*p == '\'') p++;
            } else if (*p == '"') {           /* double quotes: expand $ */
                p++;
                while (*p && *p != '"') {
                    if (*p == '$') expand_var(&p, buf, &bi);
                    else           buf[bi++] = *p++;
                }
                if (*p == '"') p++;
            } else if (*p == '$') {
                expand_var(&p, buf, &bi);
            } else {
                buf[bi++] = *p++;
            }
            if (bi >= MAX_INPUT - 1) break;
        }
        buf[bi] = '\0';
        tokens[ntok++] = strdup(buf);

        if (ntok >= MAX_TOKENS - 1) break;
    }
    tokens[ntok] = NULL;
    return ntok;
}

/* Take ownership of argc strdup'd strings and return a NULL-terminated array. */
static char **build_argv(char **argv, int argc)
{
    char **a = malloc((argc + 1) * sizeof(char *));
    for (int i = 0; i < argc; i++)
        a[i] = argv[i];
    a[argc] = NULL;
    return a;
}

int parse_line(char *line, pipeline_t *pl)
{
    char *tokens[MAX_TOKENS];
    int   ntok = tokenize(line, tokens);

    memset(pl, 0, sizeof(*pl));
    if (ntok == 0)
        return 0;

    char *argv[MAX_TOKENS];
    int   argc = 0;
    command_t *cur = &pl->cmds[0];

    for (int i = 0; i < ntok; i++) {
        char *t = tokens[i];

        if (strcmp(t, "|") == 0) {
            argv[argc] = NULL;
            cur->argv = build_argv(argv, argc);
            cur->argc = argc;
            pl->ncmds++;
            cur = &pl->cmds[pl->ncmds];
            argc = 0;
            free(t);
        } else if (strcmp(t, "<") == 0) {
            free(t);
            cur->infile = tokens[++i];      /* take ownership of filename */
        } else if (strcmp(t, ">") == 0) {
            free(t);
            cur->outfile = tokens[++i];
            cur->append  = 0;
        } else if (strcmp(t, ">>") == 0) {
            free(t);
            cur->outfile = tokens[++i];
            cur->append  = 1;
        } else if (strcmp(t, "&") == 0) {
            free(t);
            pl->background = 1;
        } else {
            argv[argc++] = t;               /* a normal argument */
        }
    }

    argv[argc] = NULL;
    cur->argv = build_argv(argv, argc);
    cur->argc = argc;
    pl->ncmds++;
    return pl->ncmds;
}

void free_pipeline(pipeline_t *pl)
{
    for (int c = 0; c < pl->ncmds; c++) {
        command_t *cmd = &pl->cmds[c];
        if (cmd->argv) {
            for (int i = 0; cmd->argv[i]; i++)
                free(cmd->argv[i]);
            free(cmd->argv);
        }
        free(cmd->infile);
        free(cmd->outfile);
    }
}
