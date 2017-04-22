#include "Socket.h"
#include "string.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>


Socket::Socket()
{
	m_sock = -1;
	memset ( &m_addr, 0, sizeof ( m_addr ) );

}

Socket::~Socket()
{
	if (is_valid())
		::close (m_sock);
}

bool Socket::create()
{
	m_sock = socket (AF_INET, SOCK_STREAM, 0 );

	if ( ! is_valid() )
		return false;


	// TIME_WAIT - argh
	//int on = 1;
	//if ( setsockopt ( m_sock, SOL_SOCKET, SO_REUSEADDR, ( const char* ) &on, sizeof ( on ) ) == -1 )
		//return false;

	return true;
}

bool Socket::MakeSocketReuseable ()
{
	int on = 1;
    if (setsockopt ( m_sock, SOL_SOCKET, SO_REUSEADDR, ( const char* ) &on, sizeof ( on ) ) == -1 )
        return false;

	return true;
}


bool Socket::bind ( const int port )
{

	if ( ! is_valid() )
        return false;
    

	m_addr.sin_family = AF_INET;
	m_addr.sin_addr.s_addr = INADDR_ANY;
	m_addr.sin_port = htons ( port );

	int bind_return = ::bind (m_sock, ( struct sockaddr * ) &m_addr, sizeof ( m_addr ) );

	if ( bind_return == -1 ) 
		return false;

	return true;
}


bool Socket::listen() const
{
	if ( ! is_valid() )
        return false;

	int listen_return = ::listen ( m_sock, MAXCONNECTIONS );


	if ( listen_return == -1 )
        return false;
    
	return true;
}


bool Socket::accept ( Socket& new_socket ) const
{
	int addr_length = sizeof ( m_addr );
	new_socket.m_sock = ::accept ( m_sock, (sockaddr *)&m_addr, (socklen_t *)&addr_length);

	if ( new_socket.m_sock <= 0 )
		return false;
	
    return true;
}


int Socket::send (char *buf, int length)
{
	return ::send (m_sock, buf, length, MSG_NOSIGNAL);    
}


int Socket::recv (char *buffer, int len)
{

    int status = ::recv (m_sock, buffer, len, 0);

    if (status == -1) {
	std::cout << "status == -1   errno == " << errno << "  in Socket::recv\n";
	return 0;
    } else if (status == 0) {
	return 0;
    } else {
	return status;
    }
}



bool Socket::connect (const std::string host, const int port)
{
	if (!is_valid()) return false;

	m_addr.sin_family = AF_INET;
	m_addr.sin_port = htons (port);

	int status = inet_pton (AF_INET, host.c_str(), &m_addr.sin_addr);

	if (errno == EAFNOSUPPORT) return false;

	status = ::connect (m_sock, (sockaddr*)&m_addr, sizeof (m_addr));

	if (status == 0)
		return true;
	
	return false;
}

void Socket::set_non_blocking (const bool b)
{

	int opts;

	opts = fcntl (m_sock, F_GETFL);

	if (opts < 0)
		return;

	if (b) {
		opts = (opts | O_NONBLOCK);
	} else {
		opts = (opts & ~O_NONBLOCK);
	}

	fcntl (m_sock, F_SETFL, opts);

}

