#include "Clientsocket.h"
#include <iostream>
#include <fstream>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <sstream>
#include <fstream>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>

#include <time.h>


#define OK "200 OK"
#define BAD "400 Bad Request"
#define NOTFOUND "404 Not Found"
#define READFAIL ""
#define BADVERSION "505 P2P-CI Version Not Supported"

std::string ClientSocket::GetCurrentTime ()
{
	time_t rawtime;
  	struct tm * timeinfo;

  	time (&rawtime);
  	timeinfo = localtime (&rawtime);
	return std::string (asctime(timeinfo));
}

std::string ClientSocket::GetLastModifiedTime (std::string path) 
{
	struct stat attr;
    stat(path.c_str(), &attr);
	return std::string (ctime(&attr.st_mtime));
}


void ClientSocket::SendFailureMsg (std::string msg, Socket *socket)
{
	std::string errorresponse;
    errorresponse += this->version + " " + msg + "\r\n";
    errorresponse += "\r\n";
    Send (socket, errorresponse);
}

bool ClientSocket::ParseDownloadRequest (std::string recvstring, Socket *socket)
{
	std::cout <<"\nParsing Download request from a peer\n";
	int pos, end_pos;
	int i = 0;
	for (i = 0; i<4; i++) {
		pos = recvstring.find_first_of ('\n', pos+1);
	}

	if (i != 4) return false;
	
	//Validate version
	pos = recvstring.find_first_of(' ');
	pos = recvstring.find_first_of(' ', pos+1);
	pos = recvstring.find_first_of(' ', pos+1);
	end_pos = recvstring.find_first_of ('\r', pos+1);
	std::string version (recvstring, pos+1, end_pos - pos - 1);
	if (version != this->version) {
		std::cout << "\nVersion " << version << " not supported. Current client is running on version " << this->version << "\nSending error msg to peer\n";
		//Create error response and send
		SendFailureMsg (BADVERSION, socket);
		
		return true;
	}

	//Get RFC number
	pos = recvstring.find_first_of(' ');
	end_pos = recvstring.find_first_of(' ', pos+1);
	end_pos = recvstring.find_first_of(' ', end_pos+1);
	std::string RFCnumber (recvstring, pos+1, end_pos - pos - 1);
	std::string filename;
	filename.append(RFCnumber);

	std::cout << "\nDownload request is for filename = "<< filename<<"\n";
 	
	std::string data;
	std::string respdata;


	std::ifstream rfcfile;
    rfcfile.open(filename.c_str(), std::ios::binary);

    //Read full file in memory
 	if (rfcfile.is_open()) {
        char *buffer = (char*)calloc(1, 65535);
        while(1){
            memset(buffer, 0, 65535);
            rfcfile.read(buffer, 65535);
            if(rfcfile){
			    //std::cout<<"\nRead one line. yee\n";
			    respdata.append (buffer);
            }else{
                respdata.append (buffer, rfcfile.gcount());
                break;
            }
		}

		//std::cout << "\nread file = "<<respdata<<"\n";
		//Send proper response
		std::string finalresponse;
		std::ostringstream len;
		len << respdata.length();

		finalresponse += this->version + " " + OK + "\r\n";
		finalresponse += "Date: " + GetCurrentTime() + "\r\n";
		finalresponse += "OS: " + this->OS + "\r\n";
		finalresponse += "Last-Modified: " + GetLastModifiedTime(filename) + "\r\n";
		finalresponse += "Content-Length: " + len.str() + "\r\n";
		finalresponse += "Content-Type: text/text\r\n";
		finalresponse += "\r\n";
		finalresponse += respdata;

		Send (socket, finalresponse);

	} else {
		std::cout<< "File could not be opened\n";
		SendFailureMsg (NOTFOUND, socket);
	}
	
	return true;
}


static void *ThreadFunc (void *input)
{
    Socket *peer;
    peer = ((ThreadInput*)input)->socket;
    int retval;
    int recvlen = 100;
	std::string request;
	//std::string temp;

    char *recvstring;
    recvstring = (char*)calloc(1, BUFFER_SIZE*sizeof(char));
    printf ("\nClient connected, receiving request\n");

	while (1) {
    	retval = peer->recv(recvstring, recvlen);
    	//temp = ((ThreadInput*)input)->client->Recv(peer);
   	 	if (!retval) {
        	printf ("\nRecv failed, closing connection and quitting thread\n");
        	break;
    	}
		request.append (recvstring);
		//request.append (temp);
    	//std::cout << "\nReceived request of size " << request.length() << " bytes\n";
    	if(((ThreadInput*)input)->client->ParseDownloadRequest (request, peer) == true) {
    		printf ("\nServed the download request\n");
			break;
		}
	
		memset (recvstring, 0, BUFFER_SIZE);
		//temp.clear();
	}

    delete peer;
    delete (ThreadInput*)input;
	free(recvstring);
	
	((ThreadInput*)input)->client->threadcount--;
    return NULL;
}

static void *ListeningThreadFunc (void *input)
{

    int rc;
    Socket *listening_socket = ((ThreadInput*)input)->socket;
	
	//std::cout << "Listen thread: listening socket = " << listening_socket << "\n";	
    while (1) {
        Socket *accepted_socket;
        accepted_socket = new Socket ();
        if ( !listening_socket->accept (*accepted_socket) ) {
            printf ( "\nCould not accept socket, error = %d", errno);
			delete accepted_socket;
            continue;
        }
		//std::cout << "\nGot socket\n";
        if ((((ThreadInput*)input)->client)->threadcount > PEER_COUNT) {
            printf ("\nMax peers reached\n");
            delete accepted_socket;
            continue;
        }
        ThreadInput *newinput = new ThreadInput;
        newinput->socket = accepted_socket;
        newinput->client = ((ThreadInput*)input)->client;
        int index = (((ThreadInput*)input)->client)->threadcount;
		
		//std::cout << "\nLaunching new thread\n";
        rc = pthread_create(&((((ThreadInput*)input)->client)->threadids[index]), NULL, ThreadFunc, (void *)newinput);
        if (rc != 0) {
            printf ("\n Could not create thread, error = %d", errno);
            delete accepted_socket;
        } else {
           (((ThreadInput*)input)->client)->threadcount++;
        }
    }

    return NULL;
}

ClientSocket::~ClientSocket ()
{
}

ClientSocket::ClientSocket (int port, std::string ip)
{
	struct utsname unamedata;
	int error;

	uname(&unamedata);
    OS.append (unamedata.sysname);
	OS += " OS ";
	OS.append (unamedata.release);

	threadcount = 0;
    version = "P2P-CI/1.0";
	selfip = ip;
	listeningport = 0;

    if (!listening_socket.create()){
        printf ( "\nCould not create listening socket, error = %d", errno);
        return;
    }

    if (!listening_socket.MakeSocketReuseable ()) {
        printf ("\nCould not make socket reuseable, error = %d\n", errno);
        return;
    }

	while (1){
    	if (!listening_socket.bind (port)){
			error = errno;
			if (error == 98){
				++port;
				continue;
			} else {
        		printf ("\nCould not bind to port, error = %d", errno);
        		return;
			}
    	}
		listeningport = port;
		break;
	}

	std::ostringstream ss;
    ss.clear();
	ss << listeningport;
    port_string = ss.str();

    if (!listening_socket.listen()){
        printf ("Could not listen to socket, error = %d", errno);
        return;
    }

	std::cout<< "\nListening on port "<<port_string<<"\n";
	ThreadInput *input = new ThreadInput;
    input->socket = &listening_socket;
    input->client = this;

	//std::cout << "Constructor: listening socket = " << &listening_socket << "\n";	
	int rc = 0;
    rc = pthread_create(&threadids[threadcount], NULL, ListeningThreadFunc, (void *)input);
    if (rc != 0) {
        printf ("\n Could not create thread, error = %d", errno);
    }
}

std::string ClientSocket::CreateLookupMsg ()
{
	std::string msg ("");
    std::string title, rfcnum;

	msg += "LOOKUP ";
    printf ("Enter RFC number\n");
    std::cin >> rfcnum;
    msg += "RFC " + rfcnum + " P2P-CI/1.0\r\n";
    //printf ("\nEnter ip address\n");
    //std::cin >> ip;
    msg += "Host: " + selfip + "\r\n";
    //printf ("\nEnter port number\n");
    //std::cin >> port;
    msg += "Port: " + port_string + "\r\n";
    printf ("\nEnter Title\n");
    std::cin >> title;
    msg += "Title: " + title + "\r\n";
	msg += "\r\n";

	return msg;
}

std::string ClientSocket::CreateGetMsg (std::string *rfc, std::string *titleval)
{
	std::string msg ("");
    std::string title, rfcnum;

    msg += "LOOKUP ";
    printf ("Enter RFC number\n");
    std::cin >> rfcnum;
	*rfc = rfcnum;
    msg += "RFC " + rfcnum + " P2P-CI/1.0\r\n";
    //printf ("\nEnter ip address\n");
    //std::cin >> ip;
	//*selfip = ip;
    msg += "Host: " + selfip + "\r\n";
    //printf ("\nEnter port number\n");
    //std::cin >> port;
    msg += "Port: " + port_string + "\r\n";
    printf ("\nEnter Title\n");
    std::cin >> title;
    *titleval = title;
    msg += "Title: " + title + "\r\n";
    msg += "\r\n";

    return msg;
}

std::string ClientSocket::CreateAddMsg ()
{
	std::string msg ("");
    std::string title, rfcnum;
    std::string fname ("");
    std::ifstream file;
    std::string empty ("");

	msg += "ADD ";
    printf ("Enter RFC number\n");
    std::cin >> rfcnum;
    fname += "RFC "+rfcnum;

    file.open(fname.c_str(), std::ios::binary);
    if (!file.is_open()) {
        printf("\nFile not found, not goint to send nonsense information to server\n");
        return empty;
    }
    msg += "RFC " + rfcnum + " P2P-CI/1.0\r\n";
    //printf ("\nEnter ip address\n");
    //std::cin >> ip;
    msg += "Host: " + selfip + "\r\n";
    //printf ("\nEnter port number\n");
    //std::cin >> port;
    msg += "Port: " + port_string + "\r\n";
    printf ("\nEnter Title\n");
    std::cin >> title;
    msg += "Title: " + title + "\r\n";
	msg += "\r\n";

	return msg;
}

std::string ClientSocket::CreateListMsg ()
{
	std::string msg;
    //std::string port, ip;

	msg += "LIST ALL ";
    msg += "P2P-CI/1.0\r\n";
    //printf ("\nEnter ip address\n");
    //std::cin >> ip;
    msg += "Host: " + selfip + "\r\n";
    //printf ("\nEnter port number\n");
    //std::cin >> port;
    msg += "Port: " + port_string + "\r\n";
	msg += "\r\n";

	return msg;
}


bool ClientSocket::CheckStatus (std::string msg)
{
	int pos = 0;
	int endpos = 0;
	pos = msg.find_first_of(' ', pos);
	endpos = msg.find_first_of(' ', pos+1);
	std::string status (msg, pos+1, endpos-pos-1);

	if (status != "200") return false;
	return true;
}

std::string ClientSocket::GetIPAndPort (std::string msg, int *port, int tryval)
{
	int pos = 0;
	int endpos = 0;
	std::string empty;
    int lastpos = 0;
    int clientcount = 0;
    //Get total client count
    lastpos = msg.find_last_of('\n');

    while(1){
        if (pos == lastpos) break;
        pos = msg.find_first_of('\n', pos+1);
        clientcount++;
    }
    pos = 0;
    //take off 1st two n last line to get count
    clientcount -= 3;

    if (tryval > clientcount) {
        printf ("\nTried with all clients and failed\n");
        return empty;
    }
	//extract ip and port. Go to first client
	pos = msg.find_first_of('\n', pos);
	pos = msg.find_first_of('\n', pos+1);
    int loop = tryval - 1;
    //now go ahead if requested
    for (int j = 0; j< loop; ++j) {
	    pos = msg.find_first_of('\n', pos+1);
    }

	//ip starts after second space
	pos = msg.find_first_of(' ', pos+1);
	pos = msg.find_first_of(' ', pos+1);
	pos = msg.find_first_of(' ', pos+1);
	endpos = msg.find_first_of(' ', pos+1);
	//extract ip
	std::string ip (msg, pos+1, endpos - pos - 1);
	//extract port.
	pos = endpos;
	endpos = msg.find_first_of ('\r', endpos+1);
	std::string portval (msg, pos+1, endpos-pos-1);
	std::istringstream ss(portval);
    ss >> *port;

	return ip;
}

bool ClientSocket::DownloadRFCFile (std::string rfc, std::string ip, int port, std::string titleval, Socket *serversocket) 
{
	Socket peersocket;

	std::cout <<"\nDownloading file from peer "<< ip << "\n";

	//connect to peer and get the file.
	if (!peersocket.create()) {
        printf ("\nFailed socket creation, error = %d", errno);
        return false;
    }

    if(!peersocket.connect (ip, port)) {
        printf ("\nConnection to peer failed, error = %d\n", errno);
        return false;
    }

	std::string request ("GET RFC ");
	request += rfc + " P2P-CI/1.0" + "\r\n";
	request += "Host: " + selfip + "\r\n";
	request += "OS: "+OS+"\r\n";
	request += "\r\n";

	Send (&peersocket, request);

	std::string response_header;
	response_header = Recv (&peersocket);
	
	//Validate version
	int pos, endpos;
	
	pos = response_header.find_first_of(' ');
	std::string version (response_header, 0, pos);
	if (version != this->version) {
		std::cout<< "\nVersion "<<version<<" not supported. Current peer on version "<<this->version<<"\n";
		return false;
	} 

	endpos = response_header.find_first_of(' ', pos+1);
	std::string status (response_header, pos+1, endpos-pos-1);
	if (status != "200"){	
		//std::cout <<"\nReceived Failure: \n" << response_header;
		return false;
	}

	//Extract data size and recv data(file)
    pos = 0;

    pos = response_header.find("Content-Length:");    
	pos = response_header.find_first_of(' ', pos+1);
	endpos = response_header.find_first_of('\r', pos+1);
	std::string datasize (response_header, pos+1, endpos-pos-1);
    std::istringstream ss(datasize);
	int size = 0;
    ss >> size;

    pos = 0;

	//Check how much of data you already received.
    //Go to last header line
    pos = response_header.find("Content-Type:");
    pos = response_header.find_first_of('\n', pos+1);
    //account last line
    pos += 2;
    int len = response_header.length();
    int temp_data_size = len - pos;

	char *file = NULL;
	std::string remainingdata;
	//Todo: Make sure size is non negative and not 0.
	if (size-temp_data_size > 0){
		remainingdata.append(response_header, pos+1, temp_data_size);
		file = (char*)calloc(1, size-temp_data_size+1);
		int recvd = 0;
		recvd = peersocket.recv(file, size-temp_data_size);
		/*if (recvd != size){
       	 	printf ("\nCould only rev %d bytes of file. Total size = %d\n", recvd, size);
        	free(file);
        	return false;
    	} else */
        if (recvd == 0) {
        	printf ("\nRecv of RFC file failed\n");
        	free(file);
        	return false;
    	}else { 
			remainingdata.append(file, recvd);
			free(file);
		}
	} else {
		remainingdata.append(response_header, pos+1, temp_data_size);
	}

	
	std::cout <<"\nDownload of file complete\n";	
	std::string fname = "RFC ";
	fname += rfc;
	
	//Write file to disk
	std::ofstream rfcfile;
    rfcfile.open(fname.c_str(), std::ios::binary|std::ios::out|std::ios::app);
  	if (rfcfile.is_open()) {
        rfcfile.write(remainingdata.c_str(), remainingdata.length());
        rfcfile.close();
        //Add this to server
        printf("\nAdding the same information to server\n");
        std::string msg ("");

        msg += "ADD ";
        msg += "RFC " + rfc + " P2P-CI/1.0\r\n";
        msg += "Host: " + selfip + "\r\n";
        msg += "Port: " + port_string + "\r\n";
        msg += "Title: " + titleval + "\r\n";
        msg += "\r\n";
        Send(serversocket, msg);
        Recv(serversocket);
  	} else {
		printf ("\nReceived RFC file, but could not open file and write to disk\n");
        return false;
	}

    return true;
}

void ClientSocket::Start (std::string serverip)
{
	Socket socket;
	std::string ip;
	int port;
	
	if (!socket.create()) {
		printf ("\nfailed socket creation, error = %d", errno);
		return;
	}

	if(!socket.connect (serverip, 7734)) {
		printf ("\nConnect failed, error = %d\n", errno);
		return;
	}
	
	int choice;
	std::string rfc;
	std::string msg;
	std::string title;
    int tryval = 1;
    std::string recvmsg ("");


	while (1) {
		//printf ("\nEntering while\n");
		//clrscr ();
		msg.clear();

		printf ("\nMenu");
		printf ("--------------------------------------------\n");
		printf ("1. ADD\n");
		printf ("2. LOOKUP\n");
		printf ("3. LIST\n");
		printf ("4. Get RFC\n");
		printf ("5. QUIT\n");
		printf ("--------------------------------------------\n");

		//std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		//fflush(stdout);
		//std::cin.clear();    
        //std::cin.ignore(INT_MAX);
		//std::cin >> choice;
		scanf ("%d", &choice);
		printf ("\nYou chose %d\n", choice);
		
		switch (choice) {
	
			//clrscr();

			case 1: 
					msg = CreateAddMsg ();
                    if (!msg.empty()) {
					    Send (&socket, msg);
					    Recv (&socket);
                    }
					break;
			case 2:
					msg = CreateLookupMsg (); 
					Send (&socket, msg);
					Recv (&socket);
                    break;
			case 3: 
					msg = CreateListMsg ();
					Send (&socket, msg);
					Recv (&socket);                    
                    break;
			case 4: 
					msg = CreateGetMsg (&rfc, &title);
					Send (&socket, msg);
                    recvmsg = Recv (&socket);
                    if (!CheckStatus (recvmsg)) {
                        //printf ("\nInvalid response, status check failed\n");
                        break;
                    }
                    while (1) {
					    port = 0;
					    ip.clear();
					    ip = GetIPAndPort (recvmsg, &port, tryval);
					    //std::cout << "\nIP = "<<ip<<"\n";
					    if (!ip.empty()){
						    bool val = DownloadRFCFile (rfc, ip, port, title, &socket);
                            if (val == false){
                                ++tryval;
                                printf("\nTrying with next client\n");
                            } else {
                                break;
                            }
					    } else {
                            break;
                        }
                    }
					break;
			case 5: return;
		}
	
		//	char buffer1 [] = "ADD RFC 123 P2P-CI/1.0\nHost: thishost.csc.ncsu.edu\r\nPort: 5678\r\nTitle: Requirements for IPsec Remote Access Scenarios\r\n\r\n";

		//	char buffer [] = "LOOKUP RFC 123 P2P-CI/1.0\nHost: thishost.csc.ncsu.edu\r\nPort: 5678\r\nTitle: Requirements for IPsec Remote Access Scenarios\r\n\r\n";

		
		while (getchar() != '\n')
  			continue;
	}

  	return;
}

std::string ClientSocket::Recv (Socket *socket)
{
	char *result;
    int retval;
	std::string msg;
	int pos = 0;
	bool failed = false;
   	
	result = (char*)calloc(1, 2048);

	while (1) {
    	retval = socket->recv(result, 2048);
    	if(!retval) {
        	printf ("\nrecv failed with error = %d", errno);
			failed = true;
			msg.clear();
			free (result);
			return msg;
    	}
		msg.append (result);
        bool ended = false;
        while(1){
            pos = msg.find ("\r\n", pos+1);
            if (msg.at(pos-2) == '\r' && msg.at(pos-1) == '\n') {
                ended = true;
                break;
            }
        }
		if (ended == true) break;
		memset (result, 0, 2048);
		//std::cout << msg;
	}
    
	if (!failed) {
		std::cout << "\nRESPONSE:\n" << msg << "\n";
	}
    
    free (result);
	return msg;
}

void ClientSocket::Send (Socket *socket, std::string msg)
{
	int len;
	char *buff = (char*)calloc(1, msg.length()+1);
    std::strcpy (buff, msg.c_str());

    len = socket->send (buff, strlen(buff));

	if (len == -1) printf ("\nSend failed with error = %d", errno);
	free (buff);
}
