
APPNAME ?= jpegqs
CFLAGS ?= -Wall -O2 -fopenmp -DAPPNAME=$(APPNAME)
LIBS ?= -ljpeg -lm
SRCS = src

PROGS = $(APPNAME) $(APPNAME)-static
PLIBS = lib$(APPNAME).a lib$(APPNAME).so.0

.PHONY: clean all

all: $(PLIBS) $(PROGS)

clean:
	rm -rf  $(PLIBS) $(PROGS) $(SRCS)/*.o

lib$(APPNAME).a: $(SRCS)/idct.o $(SRCS)/libjpegqs.o
	$(AR) rcs $@ $^

lib$(APPNAME).so.0: $(SRCS)/idct.o $(SRCS)/libjpegqs.o
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LIBS) -s

$(APPNAME): lib$(APPNAME).so.0 $(SRCS)/jpegqs.o
	$(CC) $(CFLAGS) $^ -o $@ -s

$(APPNAME)-static: $(SRCS)/jpegqs.c lib$(APPNAME).a
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS) -s
