SRC = baghand.c
DEBUG ?= -DDEBUG -g -Og
CFLAGS = 
CC ?= cc

baghand: $(SRC)
	$(CC) $(SRC) $(DEBUG) $(CFLAGS) -o $@
