CC := gcc

srcdir := src
objdir := obj

srcs := $(wildcard $(srcdir)/*.c)
objs := $(notdir $(patsubst %.c, %.o, $(srcs)))
objs := $(addprefix $(objdir)/, $(objs))

CFLAGS := -c -Wall -Werror -g3 $(shell pkg-config --cflags glib-2.0)
LDFLAGS := -lcurl -Wl,-Bsymbolic-functions $(shell pkg-config --libs glib-2.0)


$(objs): $(objdir)/%.o: $(srcdir)/%.c
	@if [ ! -e $(objdir) ]; then mkdir $(objdir); fi
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: compile
compile: $(objs)

.PHONY: intraday
intraday: $(objs)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: all
all: intraday

.PHONY: clean
clean:
	$(RM) $(objdir)/*.o
