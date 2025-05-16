CC = gcc
#CFLAGS = -Wall -Wextra -std=c99
LDFLAGS =

# Source files
SRCS = usm_bibio.c usm_test.c  
HEADERS = usm_entete.h

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
EXEC = usm_test

# Targets
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(EXEC)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)

cl:
	rm -f $(OBJS)