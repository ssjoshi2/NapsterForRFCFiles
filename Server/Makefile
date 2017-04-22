simple_server_objects = Serversocket.o Socket.o Servermain.o


all : simple_server
debug : simple_server_debug

simple_server: $(simple_server_objects)
	g++ -o simple_server $(simple_server_objects) -lpthread

simple_server_debug: $(simple_server_objects)
	g++ -g -o simple_server_debug $(simple_server_objects) -lpthread

Socket: Socket.cpp
ServerSocket: Serversocket.cpp
simple_server_main: Servermain.cpp


clean:
	rm -f *.o simple_server simple_server_debug

