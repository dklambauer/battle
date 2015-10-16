CFLAGS = -g -Wall
DEPS = shell.h parser.h

shell: shell.o parser.o
	gcc $(CFLAGS) -o shell shell.o parser.o

%.o: %.c $(DEPS)
	gcc  $(CFLAGS) -c -o $@ $< 

clean:
	rm -f shell *.o
