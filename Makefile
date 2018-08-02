all: libm2d.a example

.SUFFIXES: .o .c
#CFLAGS = -Wall -g -I. -Ofast -flto -fwhole-program #-DNDEBUG
CFLAGS = -g -I. -Ofast #-flto -fwhole-program #-DNDEBUG
LDLIBS = #-flto -fwhole-program

.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

headers = m2d.h

libm2d_a_objs = m2d.o $(common_objs)

#ui: CXXFLAGS += $(shell pkg-config $(DEPS) --cflags)
#ui: LDLIBS += $(shell pkg-config $(DEPS) --libs)

$(libm2d_a_objs): $(headers)

libm2d.a: $(libm2d_a_objs)
	$(AR) rcs libm2d.a $(libm2d_a_objs)

example_objs = example.o $(common_objs)

#example: CFLAGS += $(shell pkg-config $(DEPS) --cflags)
example: LDLIBS += -L.

$(example_objs): $(headers)

example: $(example_objs) libm2d.a
	$(CC) $(CFLAGS) $(example_objs) -o $@ $(LDLIBS) -lm2d


clean:
	rm -f $(libm2d_a_objs) libm2d.a example $(example_objs)
