CC	= $(shell which cc > /dev/null 2>&1 && echo cc || echo gcc)
CFLAGS	= -O2 -Wall -Werror $(DEFS)
LDFLAGS	= -s
OBJS	= errorpf.o test.o
TARGET	= test

.PHONY:	run-test
run-test: $(TARGET)
	./$(TARGET) -v -c ansi

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

.PHONY:	clean
clean:
	rm -f test *.o a.out core

# vim: noet sw=8 sts=8
