OBJS=$(foreach file, $(notdir $(addsuffix .o, $(basename $(wildcard ../*.cc)))), .objs/$(file))
LOCALOBJS=$(foreach file, $(notdir $(addsuffix .o, $(basename $(wildcard *.cc)))), .objs/$(file))
TARGETS=$(basename $(wildcard *.cc))
SRCS=$(wildcard ../*.cc *.cc)
VPATH=..:.
#CXXFLAGS=-ggdb -Werror -Wall -Wextra -Wconversion -Wno-unused-parameter -Woverloaded-virtual -Wshadow -D_GNU_SOURCE -DHAVE_LIBSSL -DHAVE_IPV6 -DHAVE_C_ARES -D__DEBUG__
CXXFLAGS=-pthread -ggdb -Werror -Wall -Wextra -Wconversion -Wno-unused-parameter -Woverloaded-virtual -Wshadow -D_GNU_SOURCE -DHAVE_LIBSSL -DHAVE_IPV6 -D__DEBUG__
TESTBINS=GetWebPage SendTest ReceiveTest UnixSocket

INCLUDES=-I.. -I.
LIBS=-lssl -lcrypto -lcares -lcurl -ldl

#INCLUDES=-I.. -I. -Ic:/OpenSSL/include
#LIBS=-Lc:/OpenSSL/lib/MinGW -lws2_32 -leay32 -lssleay32

ifneq (.objs,$(wildcard .objs))
$(shell mkdir .objs)
endif

all: ReceiveTest.pem ReceiveTest6.pem $(OBJS) $(LOCALOBJS) $(TARGETS)

depend: .depend

.depend:
	g++ -MM $(CXXFLAGS) $(SRCS) $(INCLUDES) | sed -e 's/^\([^[:space:]]\)/.objs\/\1/' -e 's/^.objs\/#.*//' >.depend

.objs/%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

%: .objs/%.o $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(OBJS) $(LIBS)

tags: $(SRCS) $(wildcard ../*.h *.h)
	/usr/bin/exuberant-ctags -R --c++-kinds=+p --fields=+iaS --extra=+q ..

ReceiveTest.pem:
	openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 3650 -nodes -subj "/C=XX/ST=FO/L=FOOOOOO/O=Fooo/OU=Foo/CN=127.0.0.1"
	cat key.pem cert.pem > ReceiveTest.pem
ReceiveTest6.pem:
	openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 3650 -nodes -subj "/C=XX/ST=FO/L=FOOOOOO/O=Fooo/OU=Foo/CN=::1"
	cat key.pem cert.pem > ReceiveTest6.pem

test: $(TESTBINS)
	@for i in $(TESTBINS); do \
		echo "Running $$i ..."; \
		./$$i >$$i.out 2>$$i.err || exit 1; \
	done

clean:
	rm -rf .objs core.* core $(TARGETS) *.out *.err .depend RevieveTest.pem

-include .depend
