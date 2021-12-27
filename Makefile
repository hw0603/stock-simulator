CC = cc
TARGETS = chat_server chat_client

all: $(TARGETS)

chat_server: chat_server.c
	$(CC) -o $@ $^ -lpthread

chat_client: chat_client.c
	$(CC) -o $@ $^ -lpthread

server: server.c
	$(CC) -o $@ $^ -lpthread -lm

client: client.c
	$(CC) -o $@ $^ -lpthread -lncurses -lrt

clean:
	rm -f *.o *.a
	rm -f $(TARGETS)