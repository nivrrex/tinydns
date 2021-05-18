CC = gcc
CFLAGS = -std=gnu99  -O2
SRCS = main.o cache.o config.o parse.o log.o help.o
OBJS = $(SRCS:.c=.o)
MAIN = tinydns

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -s -o $(MAIN) $(OBJS)

clean:
	$(RM) *.o $(MAIN)
