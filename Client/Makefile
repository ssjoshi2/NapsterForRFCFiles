
simple_client_objects = Clientsocket.o Socket.o Clientmain.o


all : simple_client
debug : simple_client_debug

simple_client: $(simple_client_objects)
	g++ -o simple_client $(simple_client_objects) -lpthread

simple_client_debug: $(simple_client_objects)
	g++ -g -o simple_client_debug $(simple_client_objects) -lpthread

Socket: Socket.cpp
ClientSocket: Clientsocket.cpp
simple_client_main: Clientmain.cpp


clean:
	rm -f *.o simple_client simple_client_debug

