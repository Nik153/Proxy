#include "headers.h"

Buffer::Buffer()
{
    content = ( char * ) malloc ( BUFF_SIZE + 1 );
    content[BUFF_SIZE] = '\0';
    capacity = empty_space = BUFF_SIZE;
    filled_space = 0;
    complete = false;

    std::cout << "CREATING BUFFER" <<std::endl;
}

Buffer::~Buffer()
{
    if ( content )
    {
        free ( content );
        content = NULL;
    }
    std::cout << "DESTROYING BUFFER" <<std::endl;
}

void Buffer::write_to_buffer ( char * message, int message_len )
{
    if ( message_len > empty_space )
        reallocate ( message_len );

    memcpy ( content + filled_space, message, message_len );

    empty_space -= message_len;
    filled_space += message_len;
}

int Buffer::get_from_buffer ( int offset, char * buff, int buff_len )
{
    if ( offset > filled_space )
        return 0;
    if ( buff_len > filled_space - offset )
        buff_len = filled_space - offset;
    memcpy ( buff, content + offset, buff_len);
    return buff_len;
}

void Buffer::reallocate ( int space_needed )
{
    do {
        empty_space += capacity;
        capacity *= 2;
    } while ( space_needed > empty_space );
    content = ( char * ) realloc ( content, capacity + 1 );
    content[capacity] = '\0';
}

bool Buffer::is_request_good()
{
    return  strstr ( content, "GET" ) && strstr ( content, "HTTP/1.0")  ;
}

char * Buffer::get_uri()
{
    char * uri_start = strstr ( content, "http://" ) + strlen ( "http://" );
    char * uri_end = strstr ( content, "HTTP/1.0" );
    int uri_len = uri_end - uri_start;
    char * uri = ( char * ) malloc ( uri_len + 1 );
    strncpy ( uri, uri_start, uri_len );
    uri[uri_len] = '\0';


    return uri;
}

bool Buffer::is_request_complete()
{
    if (strstr(content,"\r\n\r\n"))
    {
        complete = true;
        return true;
    }
    return false;
}

bool Buffer::is_complete()
{
    return complete;
}

void Buffer::set_complete()
{
    complete = true;
}

int Buffer::get_size()
{
    return filled_space;
}

void Buffer::transform_uri()
{
//    std::cout << "lol" << std::endl;
    char * url_start = strstr ( content, "http://" );
    char * host_end = strstr ( url_start + 7, "/");
//    std::cout << url_start << std::endl;
//    std::cout << host_end << std::endl;
    int len = strlen ( host_end );
    memmove ( url_start, host_end, len );
    url_start [len] = '\0';
//    std::cout << content << std::endl;
    return;
}
