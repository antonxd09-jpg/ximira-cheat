CC      = clang
CFLAGS  = -O2 -Wall -Wextra -std=c11 -Isrc
LDFLAGS = -lncurses -lm

SRCS = src/main.c src/hanto.c src/esp.c src/visuals.c src/menu.c
OBJS = $(SRCS:.c=.o)
BIN  = esp_demo

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
$(CC) $(CFLAGS) -c $< -o $@

clean:
rm -f $(OBJS) $(BIN)
