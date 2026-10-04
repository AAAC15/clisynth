CC = gcc
CFLAGS = -Wall -Wextra -O3
LIBS = -lm
TARGET = clisynth
SRC = clisynth.c
NAME = jajalolxd

PREFIX = /usr/local/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

install: all
	install -m 755 $(TARGET) $(PREFIX)

uninstall:
	rm -f $(PREFIX)/$(TARGET)
