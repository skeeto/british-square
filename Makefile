.POSIX:
CC      = cc
CFLAGS  = -O3 -march=native -Wall -Wextra -Wno-unused-function
LDFLAGS = -s
LDLIBS  =

bsquare: bsquare.c
	$(CC) $(CFLAGS) $(OPTS) $(LDFLAGS) -o $@ bsquare.c $(LDIBS)

clean:
	rm -f bsquare bsquare.exe
