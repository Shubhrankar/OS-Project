CC      = gcc
CFLAGS  = -Wall -Wextra -O2
OBJS    = main.o parser.o executor.o builtins.o
TARGET  = myshell

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c shell.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
