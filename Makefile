APPNAME ?= jpegqs
MPFLAGS ?= -fopenmp
CFLAGS ?= -Wall -O3 $(MPFLAGS) -DAPPNAME=$(APPNAME)
LDFLAGS ?= -ljpeg -lm -s
SRCS = src
OBJS = $(SRCS)/idct.o $(SRCS)/libjpegqs.o
OBJB = $(SRCS)/jpegqs.o

ifneq ($(shell uname -m), i386)
    CFLAGS += -fPIC
endif

PROGS = $(APPNAME) $(APPNAME)-static
PLIBS = lib$(APPNAME).a lib$(APPNAME).so.0

.PHONY: clean all

all: $(PLIBS) $(PROGS)

clean:
	rm -rf $(PLIBS) $(PROGS) $(OBJS) $(OBJB)

lib$(APPNAME).a: $(OBJS)
	$(AR) rcs $@ $^

lib$(APPNAME).so.0: $(OBJS)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)

$(APPNAME): $(OBJB) lib$(APPNAME).so.0
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(APPNAME)-static: $(OBJB) lib$(APPNAME).a
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
