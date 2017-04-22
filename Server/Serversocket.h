#ifndef ServerSocket_class
#define ServerSocket_class

#include "Socket.h"
#include <errno.h>
#include <pthread.h>
#include <iostream>
#include <list>

#define CLIENT_COUNT 500
#define BUFFER_SIZE 1024

class ServerSocket;

struct ThreadInput {
    Socket *socket;
    ServerSocket *server;
};

struct Peer {
    std::string ip_address;
    int port;
};

struct RFC {
    int RFC_number;
    std::string Title;
    std::string HostName;
};

class ServerSocket : private Socket
{
 public:

  	ServerSocket ( int port );
  	ServerSocket (){};
  	virtual ~ServerSocket();

  	bool  Start ();

  	void Accept ();
    bool ParseRequest (char *buffer, int length, Socket *socket, std::string *ip);
	std::string AddRFCAndPeer (int pos, std::string buffer, int headercount, Socket *socket);
	std::string LookupRFCAndPeer (int pos, std::string buffer, int headercount, Socket *socket);
	std::string ListRFCAndPeer (std::string buffer, int headercount, Socket *socket);
	RFC* ParseP2SHeaderValues (std::string buffer, int loopcount, Peer **peer);
	void FindPeersAndSend (RFC *rfcval, Peer *peer, Socket *socket);
	bool Send (std::string buffer, Socket *socket);
	bool ValidateVersion (std::string buffer);
	void RemoveEntries(std::string ip);
	
	volatile int threadcount;

    std::list <RFC*> RFC_list;
    std::list <Peer*> Peer_list;
	pthread_mutex_t RFC_lock;
	pthread_mutex_t Peer_lock;
	std::string version;

private:
    Socket listening_socket;
    pthread_t threadids[CLIENT_COUNT];

};


#endif

