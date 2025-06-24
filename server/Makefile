TARGET = aesdsocket

CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g

all: $(TARGET)

$(TARGET): aesdsocket.c
	@echo ">>> Using compiler: $(CC)"
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)

.PHONY: all clean

