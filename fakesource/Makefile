CC := gcc

srcs := $(wildcard *.c)
objs := $(patsubst %.c, %.o, $(srcs))

CFLAGS := -c -Wall -Werror -g3 -I../include
LDFLAGS := -lm

$(objs): %.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

fakesource: $(objs)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: all
all: fakesource

.PHONY: clean
clean:
	$(RM) *.o
