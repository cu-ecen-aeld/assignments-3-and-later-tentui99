TARGET = aesdsocket

CC := $(TARGET_CC)
CFLAGS ?= -Wall -Werror -g

all: $(TARGET)

$(TARGET): aesdsocket.c
	@echo "CC=$(CC)"
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
