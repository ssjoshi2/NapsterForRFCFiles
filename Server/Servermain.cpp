#include "Serversocket.h"
#include <string>
#include <iostream>

int main (int argc, char *argv[] )
{
    ServerSocket server (7734);
    server.Start();
    return 0;
}

