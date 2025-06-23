TARGET = aesdsocket

# Sử dụng compiler từ Buildroot (BUILDROOT sẽ cung cấp biến $(CC))
CC ?= gcc

CFLAGS ?= -Wall -Werror -g

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
