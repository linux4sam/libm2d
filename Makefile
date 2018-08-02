all: libm2d.a filltest copytest

.SUFFIXES: .o .c
#CFLAGS = -Wall -g -I. -Ofast -flto -fwhole-program #-DNDEBUG
CFLAGS = -g -I. -Ofast #-flto -fwhole-program #-DNDEBUG
LDLIBS = #-flto -fwhole-program

.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

headers = m2d.h

libm2d_a_objs = m2d.o $(common_objs)

$(libm2d_a_objs): $(headers)

libm2d.a: $(libm2d_a_objs)
	$(AR) rcs libm2d.a $(libm2d_a_objs)

filltest_objs = filltest.o $(common_objs)

filltest: LDLIBS += -L.

$(filltest_objs): $(headers)

filltest: $(filltest_objs) libm2d.a
	$(CC) $(CFLAGS) $(filltest_objs) -o $@ $(LDLIBS) -lm2d

copytest_objs = copytest.o $(common_objs)

copytest: LDLIBS += -L.

$(copytest_objs): $(headers)

copytest: CFLAGS += $(shell pkg-config cairo libdrm --cflags)
copytest: LDLIBS += $(shell pkg-config cairo libdrm --libs)

copytest: $(copytest_objs) libm2d.a
	$(CC) $(CFLAGS) $(copytest_objs) -o $@ $(LDLIBS) -lm2d

clean:
	rm -f $(libm2d_a_objs) libm2d.a \
		filltest $(filltest_objs) \
		copytest $(copytest_objs)
