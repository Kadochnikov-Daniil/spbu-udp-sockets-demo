CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = pingpong

.PHONY: all clean run
all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) $<

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)
