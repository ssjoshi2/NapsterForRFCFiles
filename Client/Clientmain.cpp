#include "Clientsocket.h"
#include <string>
#include <iostream>

int main (int argc, char *argv[] )
{
	if (argc != 3) {
		std::cout<<"\nPlease provide server ip address and self ip address";

		return 0;
	}
    std::string serverip (argv[1]);
	std::string ip (argv[2]);
    ClientSocket client (60000, ip);
    client.Start(serverip);
    return 0;
}

