#include "Serversocket.h"
#include <stdio.h>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include <map>

#define OK "200 OK"
#define BAD "400 Bad Request"
#define NOTFOUND "404 Not Found"
#define BADVERSION "505 P2P-CI Version Not Supported"

ServerSocket::ServerSocket ( int port )
{
	threadcount = 0;
	version = "P2P-CI/1.0";

	if (!listening_socket.create()){
		printf ( "\nCould not create server socket, error = %d", errno);
		return;
	}

	if (!listening_socket.MakeSocketReuseable ()) {
		printf ("\nCould not make socket reuseable, error = %d\n", errno);
		return;
	}

	if ( !listening_socket.bind ( port ) ){
		printf ("\nCould not bind to port, error = %d", errno);
		return;
	}

	if (!listening_socket.listen()){
		printf ("Could not listen to socket, error = %d", errno);
		return;
	}
}

ServerSocket::~ServerSocket()
{
}

void ServerSocket::Accept ()
{
	Socket accepted_socket;
	if ( !listening_socket.accept (accepted_socket) )
		printf ( "\nCould not accept socket, error = %d", errno);
}


RFC* ServerSocket::ParseP2SHeaderValues (std::string buffer, int loopcount, Peer **peer)
{

	RFC *rfcval = (RFC*)new RFC;
    *peer = (Peer*)new Peer;
	int position = 0;
	int startpos = 0;

	while (loopcount > 0) {
        //Check header value
        position = buffer.find_first_of('\n', startpos);
        if (buffer[position+1] == 'H') {
            std::string header (buffer, position+1, 4);
            if (header == std::string ("Host")) {

                int endpos = buffer.find_first_of ('\r', position);
                std::string ip (buffer, position+7, endpos - position - 7);
                rfcval->HostName = ip;
                (*peer)->ip_address = ip;
            } else {
                std::cout << "\nExpected Host, Got: "<<header<<std::endl;
                delete rfcval;
                delete *peer;
                return NULL;
            }
        } else if (buffer[position+1] == 'P') {
            std::string porthead (buffer, position+1, 4);
            if (porthead == std::string ("Port")) {

                int endpos = buffer.find_first_of ('\r', position);
                std::string port (buffer, position+7, endpos - position - 7);
                //sscanf (port.c_str(), "%d", &(peer->port));
                //peer->port = itoa(port.c_str());
                std::istringstream ss(port);
                ss >> (*peer)->port;
            } else {
                std::cout <<"\nExpected Port, Got :"<< porthead<<std::endl;
                delete rfcval;
                delete *peer;
                return NULL;
            }

        } else if (buffer[position+1] == 'T') {
            std::string titlehead (buffer, position+1, 5);
            if (titlehead == std::string ("Title")) {

                int endpos = buffer.find_first_of ('\r', position);
                std::string title (buffer, position+8, endpos - position - 8);
                rfcval->Title = title;
            } else {
                std::cout<< "\nExpected Title, Got :"<< titlehead<<std::endl;
                delete rfcval;
                delete *peer;
                return NULL;
            }
		}
        startpos = position +1;
        --loopcount;
    }

	return rfcval;

}

std::string ServerSocket::AddRFCAndPeer (int pos, std::string buffer, int headercount, Socket *socket)
{
	int position;
	RFC *rfcval = NULL;
	Peer *peer = NULL;
    std::ostringstream p;
	std::string address;
	std::string empty;

	position = buffer.find_first_of(' ', pos+5);
	std::string rfcnum (buffer, pos+5, position-pos-5);
	std::istringstream ss(rfcnum);
	rfcval = ParseP2SHeaderValues (buffer, headercount, &peer);
    if (rfcval == NULL) {
	    std::string output ("");
        output += version + " " + BAD + " " + "\r\n\r\n";
        printf ("\nInvalid request, sending failure response to client\n");        
        if (!Send (output, socket)) printf ("\nSend failed with error = %d", errno);
        return empty;
    }
	ss >> rfcval->RFC_number;

	address = peer->ip_address;
	//std::cout<<"\nADD -> IP = "<< peer->ip_address <<"\tport = " << peer->port<<"\tRFCno = "<<rfcval->RFC_number<<"\tTitle = "<<rfcval->Title<<std::endl;

	//prepare output & send
    std::string output ("");
    output += version + " " + OK + "\r\n";
    output += "\r\n";
	p << peer->port;
    output += "RFC " + rfcnum + " " + rfcval->Title + " " + rfcval->HostName + " " + p.str() + "\r\n";
	output += "\r\n";


	if (!Send(output, socket)) {
		printf ("\nADD: Send failed with error = %d", errno);
		free(peer);
		free(rfcval);
		return empty;
	} else {
		
		printf ("\nSuccessfuly added RFC and Peer info to list. Sending  OK response\n");
		
		//add to list
    	pthread_mutex_lock(&RFC_lock);
    	RFC_list.push_front (rfcval);
    	pthread_mutex_unlock(&RFC_lock);

    	pthread_mutex_lock(&Peer_lock);
    	Peer_list.push_front (peer);
    	pthread_mutex_unlock(&Peer_lock);
		return address;
	}
}

std::string ServerSocket::LookupRFCAndPeer (int pos, std::string buffer, int headercount, Socket *socket)
{
    int position;
    RFC *rfcval = NULL;
	Peer *peer = NULL;
	std::string address;
	std::string empty;

    position = buffer.find_first_of(' ', pos+5);
    std::string rfcnum (buffer, pos+5, position-pos-5);
    std::istringstream ss(rfcnum);
    rfcval = ParseP2SHeaderValues (buffer, headercount, &peer);
    if (rfcval == NULL) {
        std::string output ("");
        output += version + " " + BAD + " " + "\r\n\r\n";
        printf ("\nInvalid request, sending failure response to client\n");
        if (!Send (output, socket)) printf ("\nSend failed with error = %d", errno);
        return empty;
    }
    ss >> rfcval->RFC_number;

	address = peer->ip_address;

	//Now perform look up and respond.
	FindPeersAndSend (rfcval, peer, socket);
	free(rfcval);
	free(peer);

	printf ("\nSuccessfuly served Look up request\n"); 
	return address;
}

void ServerSocket::FindPeersAndSend (RFC *rfcval, Peer *peer, Socket *socket)
{
	std::list<RFC*>::iterator rit;
	std::list<Peer*>::iterator pit;
	RFC *rfc = NULL;
	std::string output ("");
	output += version + " ";
	bool addedone = false;
    
	output += (std::string)OK + "\r\n\r\n";

	//"RFC " + rfcval->RFC_number + " " + rfcval->Title;
	//Loop on the list and find if RFC is present
	pthread_mutex_lock(&RFC_lock);
	for (rit = RFC_list.begin() ; rit != RFC_list.end(); ++rit) {
		//printf ("\nin RFC loop\n");
    	if ((*rit)->RFC_number == rfcval->RFC_number) {
			rfc = *rit;
            //find all the peersand add to response
            pthread_mutex_lock(&Peer_lock);
            for (pit = Peer_list.begin() ; pit != Peer_list.end(); ++pit) {
            //printf ("\nIn peer loop\n");
                std::ostringstream p;
                std::ostringstream s;
            
                if (rfc->HostName == (*pit)->ip_address){
                    addedone = true;
                    //Add to buffer
                    s << rfc->RFC_number;
                    p << (*pit)->port;
                    output += "RFC " + s.str() + " " + rfc->Title + " " + (*pit)->ip_address + " " + p.str() + "\r\n";
                    break;
                }
            }
            pthread_mutex_unlock(&Peer_lock);
		}
	}
	pthread_mutex_unlock(&RFC_lock);

	if (addedone == false) {
		printf ("\nRFC not found in list, sending failure response\n");		
		output.clear();
		output += version + " " + NOTFOUND + " " + "\r\n\r\n";
	}else{
		output += "\r\n";
	}

	//printf ("\nSending response to client\n");

	//Send the response
	if (!Send (output, socket)) printf ("\nSend failed with error = %d", errno);
}

bool ServerSocket::Send (std::string buffer, Socket *socket)
{
	//Send the response
    int retval = 0;
    int tosend = buffer.length();
    int totalsent = 0;
    char *buff = (char*)calloc(1, buffer.length()+1);
    std::strcpy (buff, buffer.c_str());
    while (1) {
        retval = socket->send (buff+totalsent, tosend-totalsent);
        if (retval == -1){
            free(buff);
            return false;
        }
        totalsent += retval;
        if (totalsent == tosend) break;
    }

	free(buff);
	return true;
}

std::string ServerSocket::ListRFCAndPeer (std::string buffer, int headercount, Socket *socket)
{
	//No need to parse method as we need all DB entries.
	//Why do we need to store Host and Port in this case??? to print may be

	RFC *rfcval = NULL;
	Peer *peer = NULL;
	std::string output ("");
    output += version + " ";
    bool addedone = false;
	std::string empty;
	
	rfcval = ParseP2SHeaderValues (buffer, headercount, &peer);
    if (rfcval == NULL) {
        std::string output ("");
        output += version + " " + BAD + " " + "\r\n\r\n";
        printf ("\nInvalid request, sending failure response to client\n");
        if (!Send (output, socket)) printf ("\nSend failed with error = %d", errno);
        return empty;
    }

	//Now send response
	//Make a map of ip to port
	std::map<std::string,int> ipportmap;
	std::list<Peer*>::iterator it;

	//printf("\nPreparing list\n");
	pthread_mutex_lock(&Peer_lock);
	for (it = Peer_list.begin() ; it != Peer_list.end(); ++it) {
		ipportmap[(*it)->ip_address] = (*it)->port;
	}
	pthread_mutex_unlock(&Peer_lock);
		
	output += (std::string)OK + "\r\n\r\n";
	std::list<RFC*>::iterator rit;
   
	//std::cout << "\nLooping through " << RFC_list.size() << " elements\n"; 
	pthread_mutex_lock(&RFC_lock);
	int count;
	for (rit = RFC_list.begin(), count = 0; rit != RFC_list.end(); ++rit, count++) {
        addedone = true;
    	std::ostringstream p;
    	std::ostringstream s;
        //Add to buffer
        s << (*rit)->RFC_number;
		//get port
		p << ipportmap.find((*rit)->HostName)->second;
        output += "RFC " + s.str() + " " + (*rit)->Title + " " + (*rit)->HostName + " " + p.str() + "\r\n";            
	}
        
	output += "\r\n";

    pthread_mutex_unlock(&RFC_lock);
	
	//printf ("\nSending response to client\n");

    //Send the response
    if (!Send (output, socket)) printf ("\nSend failed with error = %d", errno);
	std::string address = peer->ip_address;
	free(peer);
	free(rfcval);
    printf("\nSuccessfuly served List request\n");
	return address;
}

bool ServerSocket::ValidateVersion (std::string buffer)
{
	//find 3rd space if ADD or LOOKUP. Else 2nd space for LIST
	int pos = 0;
	int endpos = 0;
	int loopcount = 0;

	pos = buffer.find_first_of(' ');
	
	std::string method (buffer, 0, pos);
	//std::cout<<"\nMethod = "<<method<<"\n";
	if (method == "LIST") {
		loopcount = 2;
	} else {
		loopcount = 3;
	}

	pos = 0;
	for (int i = 0; i< loopcount; ++i){
		pos = buffer.find_first_of (' ', pos+1);
	}
	
	endpos = buffer.find_first_of ('\r');
	std::string ver (buffer, pos+1, version.length());

	if (ver == version) return true;

	std::cout <<"Version "<<ver<<" not supported. Server running on version "<<version<<"\n";
	return false;
	
}


bool ServerSocket::ParseRequest(char *buffer, int buflen, Socket *socket, std::string *ip)
{
	int line = 0;
	int pos = 0;
	bool badreq = false;
	std::string address;

    //Check if we received full request. i.e., 5 new lines
	for (int i = 0; i<buflen; ++i) {
		if (buffer[i] == '\n') line++;
	} 
	
	if (line == 5 || (line == 4 && buffer[1] == 'I')) {

		//TODO: Validate version
		
		if (!ValidateVersion (buffer)) {
			std::string response ("");
            response += version + " "  + BADVERSION + "\r\n";
            response += "\r\n";
            if (!Send (response, socket)) printf ("\nSend failed with error = %d", errno);
			return true;
		}

		//parse and handle request
		//std::cout << std::endl << buffer;
		std::string request (buffer);
		std::cout << "\nReceived following request:\n" << request;
		pos = request.find_first_of (' ');
		std::string method (request, 0, pos);
		if (pos != std::string::npos) {
			switch (pos) {
				case 3:	if (method != "ADD") {
							badreq = true;
						}
						
						address = AddRFCAndPeer (3, request, 3, socket);
						printf ("\nADD Done\n");
						break;

				case 6:	if (method != "LOOKUP") {
                            badreq = true;
                        }

						address = LookupRFCAndPeer (6, request, 3, socket);
						printf ("\nLOOKUP Done\n");
						break;

				case 4:	if (method != "LIST") {
                            badreq = true;
                        }

						address = ListRFCAndPeer (request, 2, socket);
						printf ("\nLIST Done\n");
						break;
				default: badreq = true;
			}
		} else {
			badreq = true;
		}

		if (badreq == true) {
			std::string response ("");
			response += version + " "  + BAD + "\r\n";
			response += "\r\n";
			if (!Send (response, socket)) printf ("\nSend failed with error = %d", errno);
		}
		*ip = address;
		return true;
	}

	return false;
}

static void *ThreadFunc (void *input)
{
    Socket *client;
    client = ((ThreadInput*)input)->socket;
    int retval;
    int totallen = 0;
    int recvlen = 100;
	std::string ip;

	char *recvstring;
	recvstring = (char*)calloc(1, BUFFER_SIZE*sizeof(char));
	printf ("\nNew client connected, receiving request\n");
    while (1) {
		retval = client->recv((recvstring + totallen), recvlen);
		if (!retval) {
	    	printf ("\nRecv failed, closing connection and quitting thread\n");
	    	delete client;
	    	delete (ThreadInput*)input;
			free (recvstring);
     	    ((ThreadInput*)input)->server->threadcount--;
			if (!ip.empty()){
				((ThreadInput*)input)->server->RemoveEntries(ip);				
			}
	    	return NULL;
		}
		totallen += retval;
		//printf ("\nReceived request of size %d bytes\n", totallen);
		if(((ThreadInput*)input)->server->ParseRequest (recvstring, totallen, client, &ip) == true){
			free(recvstring);
			totallen = 0;
			recvstring = (char*)calloc(1, BUFFER_SIZE*sizeof(char));
		}
		printf ("\nServed the request. Going back to receive next request\n");
    }
	
    return NULL;
}

void ServerSocket::RemoveEntries(std::string ip)
{
	std::list<RFC*>::iterator rit;
    std::list<Peer*>::iterator pit;
	RFC *rfcval;
	Peer *peer;

	pthread_mutex_lock(&RFC_lock);
	rit = RFC_list.begin();
	while (rit != RFC_list.end()){
    //for (rit = RFC_list.begin() ; rit != RFC_list.end(); ++rit) {
        if ((*rit)->HostName == ip) {
            rfcval = *rit;
			delete rfcval;
			rit = RFC_list.erase(rit);
        } else {
			++rit;
		}
    }
	pthread_mutex_unlock(&RFC_lock);

	pthread_mutex_lock(&Peer_lock);
    pit = Peer_list.begin();
    while (pit != Peer_list.end()){
    //for (pit = Peer_list.begin() ; pit != Peer_list.end(); ++pit) {
        if ((*pit)->ip_address == ip) {
            peer = *pit;
            delete peer;
            pit = Peer_list.erase(pit);
        } else {
            ++pit;
        }
    }
    pthread_mutex_unlock(&Peer_lock);
}

bool ServerSocket::Start ()
{
	printf ("\nRunning Server\n");
	while (1) {
		Socket *accepted_socket;
		accepted_socket = new Socket ();
		if ( !listening_socket.accept (*accepted_socket) ) {
            printf ( "\nCould not accept socket, error = %d", errno);
			delete accepted_socket;
		    continue;
 		}
		
		if (threadcount > CLIENT_COUNT) {
		    printf ("\nMax clients reached\n");
		    delete accepted_socket;
		    continue;
		}
		ThreadInput *input = new ThreadInput;
		input->socket = accepted_socket;
		input->server = this;
		int rc;
      	rc = pthread_create(&threadids[threadcount], NULL, ThreadFunc, (void *)input);
		if (rc != 0) {
		    printf ("\n Could not create thread, error = %d", errno);
		    delete accepted_socket;
		} else {
            threadcount++;
		}
	}
}

