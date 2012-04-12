CC := gcc

srcs := $(wildcard *.c)
objs := $(patsubst %.c, %.o, $(srcs))

CFLAGS := -c -Wall -Werror -g3 -I../include \
$(shell mysql_config --cflags)

LDFLAGS := -lm 	$(shell mysql_config --libs)

$(objs): %.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

fakesource: $(objs)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: all
all: fakesource

.PHONY: clean
clean:
	$(RM) *.o
