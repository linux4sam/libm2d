all: libm2d.a filltest copytest blendtest roptest

.SUFFIXES: .o .c
#CFLAGS = -Wall -g -I. -Ofast -flto -fwhole-program #-DNDEBUG
CFLAGS = -Wall -g -I. -Ofast #-flto -fwhole-program #-DNDEBUG
LDLIBS = #-flto -fwhole-program

.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

headers = m2d.h

libm2d_a_objs = m2d.o $(common_objs)

$(libm2d_a_objs): $(headers)

libm2d.a: CFLAGS += $(shell pkg-config cairo libdrm --cflags)
libm2d.a: LDLIBS += $(shell pkg-config cairo libdrm --libs)

libm2d.a: $(libm2d_a_objs)
	$(AR) rcs libm2d.a $(libm2d_a_objs)

filltest_objs = filltest.o $(common_objs)

filltest: LDLIBS += -L.

$(filltest_objs): $(headers)

filltest: CFLAGS += $(shell pkg-config cairo libdrm --cflags)
filltest: LDLIBS += $(shell pkg-config cairo libdrm --libs)

filltest: $(filltest_objs) libm2d.a
	$(CC) $(CFLAGS) $(filltest_objs) -o $@ $(LDLIBS) -lm2d

copytest_objs = copytest.o $(common_objs)

copytest: LDLIBS += -L.

$(copytest_objs): $(headers)

copytest: CFLAGS += $(shell pkg-config cairo libdrm --cflags)
copytest: LDLIBS += $(shell pkg-config cairo libdrm --libs)

copytest: $(copytest_objs) libm2d.a
	$(CC) $(CFLAGS) $(copytest_objs) -o $@ $(LDLIBS) -lm2d

blendtest_objs = blendtest.o $(common_objs)

blendtest: LDLIBS += -L.

$(blendtest_objs): $(headers)

blendtest: CFLAGS += $(shell pkg-config cairo libdrm --cflags)
blendtest: LDLIBS += $(shell pkg-config cairo libdrm --libs)

blendtest: $(blendtest_objs) libm2d.a
	$(CC) $(CFLAGS) $(blendtest_objs) -o $@ $(LDLIBS) -lm2d

roptest_objs = roptest.o $(common_objs)

roptest: LDLIBS += -L.

$(roptest_objs): $(headers)

roptest: CFLAGS += $(shell pkg-config cairo libdrm --cflags)
roptest: LDLIBS += $(shell pkg-config cairo libdrm --libs)

roptest: $(roptest_objs) libm2d.a
	$(CC) $(CFLAGS) $(roptest_objs) -o $@ $(LDLIBS) -lm2d

clean:
	rm -f $(libm2d_a_objs) libm2d.a \
		filltest $(filltest_objs) \
		copytest $(copytest_objs) \
		blendtest $(blendtest_objs) \
		roptest $(roptest_objs)
