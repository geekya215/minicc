CFLAGS=-std=c11 -g -fno-common

minicc: main.o
	$(CC) -o minicc main.o $(LDFLAGS)

test: minicc
	./test.sh

clean:
	rm -f minicc *.o *~ tmp*

.PHONY: test clean
