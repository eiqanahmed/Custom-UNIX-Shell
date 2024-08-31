
CC := gcc
CFLAGS += -Wall -std=gnu99
DEBUG_CFLAGS := -DDEBUG -g

TARGET := shell
SRCS := shell.c parsing.c run_shell.c
OBJS := $(SRCS:.c=.o)

all: $(TARGET)

debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(TARGET)

$(TARGET): $(SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $(TARGET) $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) *.o *.so

# end
