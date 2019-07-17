CGI_OBJS := params.o auth.o request.o http.o

$(PREFIX_A)libcgi.a: $(addprefix $(PREFIX_O)libcgi/, $(CGI_OBJS))
	$(ARCH)

$(PREFIX_H)libcgi.h: libcgi/libcgi.h
	$(HEADER)

all: $(PREFIX_A)libcgi.a $(PREFIX_H)libcgi.h
