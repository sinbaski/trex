CC := g++
srcdir := src
objdir := obj

USE_FAKE_SOURCE := 0
DAEMONIZE := 0
REAL_TRADE := 1
CURFEW_AFT_5 := 0
MATLAB_ROOT := /usr/local/MATLAB/R2012a
#LD_LIBRARY_PATH=$(MATLAB_ROOT)/bin/glnxa64

srcs := $(wildcard $(srcdir)/*.c)
objs := $(notdir $(patsubst %.c, %.o, $(srcs)))
objs := $(addprefix $(objdir)/, $(objs))

CFLAGS := -c -Wall -Werror -Iinclude -g3 \
$(shell pkg-config --cflags glib-2.0) \
$(shell mysql_config --cflags) \
-I$(MATLAB_ROOT)/extern/include \
-DUSE_FAKE_SOURCE=$(USE_FAKE_SOURCE) \
-DDAEMONIZE=$(DAEMONIZE) \
-DREAL_TRADE=$(REAL_TRADE) \
-DCURFEW_AFT_5=$(CURFEW_AFT_5)

LDFLAGS := \
-L$(MATLAB_ROOT)/bin/glnxa64 \
-L$(MATLAB_ROOT)/sys/os/glnxa64 \
-Wl,-rpath $(MATLAB_ROOT)/bin/glnxa64 \
-Wl,-rpath $(MATLAB_ROOT)/sys/os/glnxa64 \
-lcurl -Wl,-Bsymbolic-functions \
-leng -lmx \
$(shell pkg-config --libs glib-2.0) -lm\
$(shell mysql_config --libs) \


$(objs): $(objdir)/%.o: $(srcdir)/%.c
	@if [ ! -e $(objdir) ]; then mkdir $(objdir); fi
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: fakesource
fakesource:
	make -C fakesource all

.PHONY: intraday-test
intraday-test: $(objs)
	$(CC) $(filter %.o, $^) $(LDFLAGS) -o $@

.PHONY: all
all: intraday fakesource

.PHONY: clean
clean:
	$(RM) $(objdir)/*.o
