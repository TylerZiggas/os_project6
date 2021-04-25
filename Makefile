GCC = gcc
CFLAGS = -Wall -g -lm

OSS_DEP = oss.c
OSS_OBJ = $(OSS_DEP:.c=.o)
OSS = oss
USER_DEP = user.c
USER_OBJ = $(USER_DEP:.c=.o)
USER = get_page
HEADER = oss.h
OBJ = shared.c

OUTPUT = $(OSS) $(USER)
all: $(OUTPUT)

$(OSS): $(OSS_OBJ)
	$(GCC) $(CFLAGS) $(OSS_OBJ) $(OBJ)  -o $(OSS)

$(USER): $(USER_OBJ)
	$(GCC) $(CFLAGS) $(USER_OBJ) $(OBJ) -o $(USER)

%.o: %.c $(HEADER)
	$(GCC) -c $(CFLAGS) $*.c -o $*.o

.PHONY: clean

clean:
	rm -f *.o $(OUTPUT) *.txt *.dat logfile
