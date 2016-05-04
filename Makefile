
all:
	gcc socketlib.c   test_server.c   -o  server   -lpthread
	gcc socketlib.c   test_client.c   -o  client   -lpthread

clean:
	-rm -rf  ./client  ./server