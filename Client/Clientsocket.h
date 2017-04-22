#ifndef ClientSocket_class
#define ClientSocket_class

#include "Socket.h"
#include <errno.h>
#include <pthread.h>
#include <iostream>
#include <list>

#define PEER_COUNT 500
#define BUFFER_SIZE 1024

class ClientSocket;

/**
 * Structure as input for threads
 */
struct ThreadInput {
    Socket *socket;
    ClientSocket *client;
};


class ClientSocket : private Socket {

public:

    //Constructor and destructor
    ClientSocket    (int port, std::string ip);
  	ClientSocket    (){};
    virtual ~ClientSocket();

    //Begin client
  	void        Start (std::string serverip);

    //Multiple client functions
	std::string CreateLookupMsg ();
	std::string CreateGetMsg (std::string *rfc, std::string *title);
	std::string CreateAddMsg ();
	std::string CreateListMsg ();

    //Helper methods
	std::string GetCurrentTime ();
	bool        CheckStatus (std::string msg);
	std::string GetIPAndPort (std::string msg, int *port, int tryval);
	std::string GetLastModifiedTime (std::string fname);

    //Peer to peer methods
	bool        DownloadRFCFile (std::string rfc, std::string ip, int porti, std::string title, Socket *serversocket);
	bool        ParseDownloadRequest (std::string recvstring, Socket *socket);
	
    //Send/Recv methods
	std::string Recv (Socket *socket);
	void        Send (Socket *socket, std::string msg);
	void        SendFailureMsg (std::string msg, Socket *socket);

    //Public variables
	int         threadcount;
	std::string version;
	std::string OS;
    pthread_t   threadids[PEER_COUNT];
	std::string selfip;
	int         listeningport;
	std::string port_string;

private:
    Socket listening_socket;

};


#endif

