# myshell — a small Unix shell in C

A command-line shell written in C that demonstrates the core process- and
file-management features of a Unix operating system: command execution,
pipelines, I/O redirection, signal handling, and background jobs.

## Features

- **Run external programs** — anything on your `PATH` (`ls`, `grep`, `gcc`, …)
- **Built-in commands** — `cd`, `pwd`, `echo`, `export`, `unset`, `history`, `help`, `exit`
- **Pipelines** — chain any number of commands: `cat file | grep foo | wc -l`
- **I/O redirection** — `>` (overwrite), `>>` (append), `<` (input)
- **Background jobs** — end a command with `&`; finished jobs are reported at the next prompt
- **Signal handling** — `Ctrl-C` interrupts the running command but does **not** kill the shell
- **Quoting** — single quotes (literal) and double quotes (with variable expansion)
- **Environment variables** — `$VAR` and `${VAR}` expansion; `export NAME=value`
- **Comments** — anything after `#` is ignored
- **Colored prompt** showing the current directory
- Graceful exit on `Ctrl-D` (end of input)

## Build & run

```sh
make
./myshell
```

`make clean` removes the build artifacts.

## Usage examples

```sh
myshell:myshell$ echo "hello $USER"
myshell:myshell$ ls -l | grep ".c" | wc -l
myshell:myshell$ sort < names.txt > sorted.txt
myshell:myshell$ echo "log entry" >> app.log
myshell:myshell$ sleep 5 &
myshell:myshell$ export GREETING=hi
myshell:myshell$ echo $GREETING
myshell:myshell$ history
myshell:myshell$ exit
```

## Project structure

| File         | Responsibility |
|--------------|----------------|
| `shell.h`    | Shared data structures (`command_t`, `pipeline_t`) and function prototypes |
| `main.c`     | The read–parse–execute loop, prompt, signal setup, background reaping |
| `parser.c`   | Tokenizing (quotes, `$VAR` expansion) and building a `pipeline_t` |
| `executor.c` | Running a pipeline: pipes, redirection, `fork`/`exec`, waiting |
| `builtins.c` | The built-in commands and the in-memory history buffer |
| `Makefile`   | Build rules (`-Wall -Wextra`, warning-free) |

The flow for one line of input is:

```
input line ──► parser.c ──► pipeline_t ──► executor.c ──► fork/exec children
```

## Operating-system concepts demonstrated

This is the part to talk about in interviews — each feature maps to a real OS idea:

- **Process creation** — `fork()` duplicates the shell; `execvp()` replaces the
  child's image with the requested program; the parent uses `waitpid()` to
  collect the exit status.
- **Why some built-ins can't be children** — `cd`, `export`, and `exit` change
  the shell's own state, so they run in the shell process. Running `cd` in a
  child would change only the child's directory, which then exits.
- **File descriptors and redirection** — `open()` returns a descriptor and
  `dup2()` points `STDIN`/`STDOUT` at a file, so a program writing to "standard
  output" actually writes to the file without knowing it.
- **Inter-process communication via pipes** — `pipe()` creates a connected
  read/write pair; the executor wires each command's output descriptor to the
  next command's input descriptor with `dup2()`, then closes the unused ends so
  readers see end-of-file correctly.
- **Signals** — a `SIGINT` handler (installed with `sigaction`, no `SA_RESTART`)
  lets `Ctrl-C` interrupt a blocking read and redraw the prompt; children reset
  `SIGINT` to the default so the key still stops the running command.
- **Zombie processes** — background children are reaped with a non-blocking
  `waitpid(-1, …, WNOHANG)` at each prompt, so finished jobs don't linger as
  zombies. Doing this at the prompt (rather than in a `SIGCHLD` handler) avoids
  racing the foreground `waitpid()`.

## Possible extensions

- Arrow-key history and tab completion using GNU `readline` (`-lreadline`)
- Sequencing and conditionals: `;`, `&&`, `||`
- A proper `jobs` / `fg` / `bg` job-control system with process groups
- Globbing (`*.c`) and tilde (`~`) expansion
- Here-documents (`<<`)
