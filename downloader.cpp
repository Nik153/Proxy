#include "headers.h"

Downloader::Downloader( char * uri, Buffer * buffer )
{
    this -> uri = uri;
    this -> request = NULL;
    this -> page = buffer;
    this -> request_sent = false;
    this -> sending_offset = 0;
    this -> buff = ( char * ) malloc ( BUFF_SIZE + 1 );
    buff[BUFF_SIZE] = '\0';
    this -> to_close = false;
    this -> first = true;

    char * host_end = strstr ( uri , "/" );
    int host_len;
    if ( host_end )
        host_len = host_end - uri;
    else
        host_len = strlen ( uri );

    char * host = ( char * ) malloc ( host_len + 1 );
    strncpy ( host, uri, host_len );
    host[host_len] = '\0';

    struct hostent * host_info = gethostbyname ( host );

    free ( host );
    host = NULL;

    if ( ! host_info )
    {
        perror ( "gethostbyname error." );
        to_close = true;
    }

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);

    if ( socket_fd < 0 )
    {
        perror ( "socket error." );
        to_close = true;
    }

    struct sockaddr_in dest_addr;

    dest_addr.sin_family = PF_INET;
    dest_addr.sin_port = htons ( DEFAULT_PORT );
    memcpy ( &dest_addr.sin_addr, host_info -> h_addr, host_info -> h_length );

    if (-1 == connect ( socket_fd, ( struct sockaddr * ) &dest_addr, sizeof ( dest_addr ) ) )
    {
        perror ( "connect." );
        to_close = true;
    }

    std::cout << "CREATING DOWNLOADER" << std::endl;
}

Downloader::~Downloader()
{
    if ( request )
    {
        delete request;
        request = NULL;
    }

    if ( uri )
    {
        free(uri);
        uri = NULL;
    }

    if ( buff )
    {
        free ( buff );
        buff = NULL;
    }
    page = NULL;
    close ( socket_fd );

    std::cout << "DESTROYING DOWNLOADER" << std::endl;
}

void Downloader::set_request ( Buffer * request )
{
    this -> request = request;
}

void Downloader::do_receive()
{

    int received = recv ( socket_fd, buff, BUFF_SIZE, 0 );
    buff[received] = '\0';

    switch ( received )
    {
    case -1:
        perror ( "recv error." );
        to_close = true;
        break;
    case 0:
        std::cout << "RECEIVEING COMPLETE!" << std::endl;
        to_close = true;
        page -> set_complete();
        break;
    default:
        page -> write_to_buffer( buff, received );
        break;
    }

}

bool Downloader::is_closing()
{
    return to_close;
}

int Downloader::get_socket_fd()
{
    return socket_fd;
}

bool Downloader::is_request_sent()
{
    return request_sent;
}

void Downloader::do_send()
{
    if ( first )
    {
        std::cout << "ASDASDASDASFAWDFASDAWDASDAWDASGS" << std::endl;

        first = false;
        request -> transform_uri();
    }


    int got = request -> get_from_buffer( sending_offset, buff, BUFF_SIZE );
    buff[got] = '\0';

    int sent = send ( socket_fd, buff, got, 0 );

    switch (sent)
    {
    case -1:
        perror ( "send." );
        to_close = true;
        break;
    case 0:
        std::cout << "SERVER BLOCKED" << std::endl;
        request_sent = true;
        break;
    default:
        sending_offset += got;
        if ( sending_offset >= request -> get_size() )
            request_sent = true;
        break;
    }

}
