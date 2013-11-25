CC=gcc
CFLAG=-I./include

all: hs 

hs: src/main.c src/hash.c src/hmsg_handle.c src/dlog.c
	$(CC) $^ -o $@ $(CFLAG)
	ln -sf hs hmsg_monitor

.PHONY: clean

clean:
	${RM} *.o hs hmsg_monitor
