CFLAGS=-std=c11 -g -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

minicc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): minicc.h

test: minicc
	./test.sh

clean:
	rm -f minicc *.o *~ tmp*

.PHONY: test clean
