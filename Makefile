PROGS = negentig scrub wbfs 
COMMON = tools.o bn.o ec.o disc_usage_table.o libwbfs.o libwbfs_unix.o wiidisc.o rijndael.o
DEFINES = -DLARGE_FILES -D_FILE_OFFSET_BITS=64
LIBS = -lcrypto

CC = gcc
CFLAGS = -Wall -W -O3 -Ilibwbfs -I.
#CFLAGS = -Wall -m32 -W  -ggdb -Ilibwbfs -I.
LDFLAGS = -m32

VPATH+=libwbfs
OBJS = $(patsubst %,%.o,$(PROGS)) $(COMMON)

all: $(PROGS)

$(PROGS): %: %.o $(COMMON) Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(COMMON) $(LIBS) -o $@

$(OBJS): %.o: %.c tools.h Makefile
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@ 

clean:
	-rm -f $(OBJS) $(PROGS)
