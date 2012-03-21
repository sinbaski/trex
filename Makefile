CC := gcc
srcdir := src
objdir := obj

USE_FAKE_SOURCE := 1
DAEMONIZE := 0
REAL_TRADE := 0
CURFEW_AFT_5 = 1

srcs := $(wildcard $(srcdir)/*.c)
objs := $(notdir $(patsubst %.c, %.o, $(srcs)))
objs := $(addprefix $(objdir)/, $(objs))

CFLAGS := -c -Wall -Werror -Iinclude -g3 \
$(shell pkg-config --cflags glib-2.0) \
-I$(MATLAB_ROOT)/extern/include \
-DUSE_FAKE_SOURCE=$(USE_FAKE_SOURCE) \
-DDAEMONIZE=$(DAEMONIZE) \
-DREAL_TRADE=$(REAL_TRADE) \
-DCURFEW_AFT_5=$(CURFEW_AFT_5)

LDFLAGS := -lcurl -Wl,-Bsymbolic-functions \
	$(shell pkg-config --libs glib-2.0) -lm \
	-leng -lmx -L$(MATLAB_ROOT)/bin/glnxa64 \
	-L$(MATLAB_ROOT)/sys/os/glnxa64

$(objs): $(objdir)/%.o: $(srcdir)/%.c
	@if [ ! -e $(objdir) ]; then mkdir $(objdir); fi
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: fakesource
fakesource:
	make -C fakesource all

.PHONY: intraday
intraday: $(objs)
	$(CC) $(filter %.o, $^) $(LDFLAGS) -o $@

.PHONY: all
all: intraday fakesource

.PHONY: clean
clean:
	$(RM) $(objdir)/*.o
