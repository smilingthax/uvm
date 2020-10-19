OBJECTS=insgen.o

CFLAGS=-Wall
#CFLAGS+=-O3

all: test_uvm
ifneq "$(MAKECMDGOALS)" "clean"
  -include $(OBJECTS:.o=.d)
endif

clean:
	$(RM) $(OBJECTS) $(OBJECTS:.o=.d) test_uvm

%.d: %.c
	@$(CC) $(CPPFLAGS) -MM -MT"$@" -MT"$*.o" -o $@ $<  2> /dev/null

test_uvm: test_uvm.c insgen.o
	$(CC) -o $@ $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

