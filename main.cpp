#include "headers.h"

using namespace std;

void exit_handler(int sig);
void exit_with(const char * mess);
void init_server_socket();
void start_main_loop();

int server_socket_fd;

vector < Client * > clients;
vector < Downloader * > downloaders;

map < string , Buffer * > cache;

int port_to_bind;

int main( int argc, char** argv )
{
    cout << "Starting proxy!" << endl;

    signal(SIGINT, exit_handler);
    signal(SIGKILL, exit_handler);
    signal(SIGTERM, exit_handler);
    signal(SIGABRT, exit_handler);

    struct rlimit rl;

    getrlimit (RLIMIT_AS, &rl);
    rl.rlim_cur = 1024 * 1024 * 1024;
    rl.rlim_max = 1024 * 1024 * 1024;
    setrlimit (RLIMIT_AS, &rl);

    if (argc != 2)
        exit_with("Usage: ./a.out port_to_bind");

    port_to_bind = atoi(argv[1]);

    cout << "Initing server socket." << endl;

    init_server_socket();

    cout << "Starting main loop." << endl;

    start_main_loop();

    return 0;
}

void start_main_loop()
{
    void handle_new_connection();

    fd_set to_read_desc;
    fd_set to_write_desc;

    int max_fd;

    for(;;)
    {
//        cout << "Initing descriptors sets." << endl;
        int i = 0;
        FD_ZERO ( &to_read_desc );
        FD_ZERO ( &to_write_desc );

        FD_SET ( server_socket_fd, &to_read_desc );
        max_fd = server_socket_fd;
        ++i;

        for ( auto client : clients)
        {
            if ( ! client -> is_closing() )
            {
                if ( ! client -> is_request_complete())
                {
                    FD_SET ( client -> get_socket_fd(), &to_read_desc );
                    max_fd = max( max_fd, client -> get_socket_fd() );
                    ++i;
                }
                else
                {
                    FD_SET ( client -> get_socket_fd(), &to_write_desc );
                    max_fd = max( max_fd, client -> get_socket_fd() );
                    ++i;
                }
            }
        }

        for ( auto downloader : downloaders )
        {
            if ( ! downloader -> is_closing() )
            {
                if ( downloader -> is_request_sent() )
                {
                    FD_SET ( downloader -> get_socket_fd(), &to_read_desc );
                    max_fd = max( max_fd, downloader -> get_socket_fd() );
                    ++i;
                }
                else
                {
                    FD_SET ( downloader -> get_socket_fd(), &to_write_desc );
                    max_fd = max( max_fd, downloader -> get_socket_fd() );
                    ++i;
                }
            }
        }

//        cout << "Starting select with " << i << " fd's." << endl;

        int select_result = select ( max_fd + 1, &to_read_desc, &to_write_desc, NULL, 0);

        if ( select_result == 0 )
        {
            cout << "Select returned 0." << endl;
            continue;
        }
        if ( select_result < 0 )
            exit_with ( "select error." );

//        cout << "Select done well." << endl;

        if ( FD_ISSET ( server_socket_fd,  &to_read_desc ) )
            handle_new_connection();

//        cout << "Working with opened connections." << endl;

//      cout << "RECEIVING REQUESTS" << endl;
        for ( auto client : clients)
        {
            if ( ! client -> is_closing() && FD_ISSET ( client -> get_socket_fd(), &to_read_desc) )
            {
                client -> do_receive();
                if ( client -> is_request_complete() )
                {
                    if ( ! client -> is_request_good() )
                        client -> send_bad_request();
                    else
                    {
                        char * uri = client -> get_uri();
                        string s_uri(uri);
                        Buffer * buffer;

                        if ( ! cache.count( s_uri ) )
                        {
                            buffer = new Buffer;
                            cache [s_uri] = buffer;
                            Downloader * downloader = new Downloader ( uri, buffer );
                            downloaders.push_back ( downloader );
                            client -> toss_request_to_downloader ( downloader );

                        }
                        else
                            buffer = cache [s_uri];

                      client -> set_read_from ( buffer );

                    }
                }
            }
        }
//      cout << "SENDING REQUESTS" << endl;
        for ( auto downloader : downloaders )
        {
            if ( ! downloader -> is_closing() && FD_ISSET ( downloader -> get_socket_fd(), &to_write_desc) )
            {
                downloader -> do_send();
            }
        }
//      cout << "RECEIVING RESPONCES" << endl;
        for ( auto downloader : downloaders )
        {
            if ( ! downloader -> is_closing() && FD_ISSET ( downloader -> get_socket_fd(), &to_read_desc ) )
            {
                downloader -> do_receive();
            }
        }
//      cout << "SENDING RESPONCES" << endl;
        for ( auto client : clients )
        {
            if ( ! client -> is_closing() && FD_ISSET (client -> get_socket_fd(), &to_write_desc ) )
            {
                client -> read_from_cache();
            }
        }

//      cout << "CLEANING" << endl;

        for ( auto it = clients.begin(); it != clients.end(); )
        {
            if ( (*it) -> is_closing() )
            {
                delete * it;
                * it = NULL;
                it = clients.erase(it);
            }
            else
                ++it;
        }

        for ( auto it = downloaders.begin(); it != downloaders.end(); )
        {
            if ( (*it) -> is_closing() )
            {
                delete * it;
                * it = NULL;
                it = downloaders.erase(it);
            }
            else
                ++it;
        }

    }
}

void init_server_socket()
{
    server_socket_fd = socket(PF_INET, SOCK_STREAM, 0);

    if(server_socket_fd < 0)
        exit_with ( "socket error." );

    int enable = 1;
    if ( setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) )
        exit_with (  "setsockopt error." );

    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = PF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sockaddr.sin_port = htons( port_to_bind );

    int res = bind(server_socket_fd, (struct sockaddr * ) &server_sockaddr, sizeof(server_sockaddr));
    if(res < 0)
        exit_with("bind error.");

    res = listen(server_socket_fd, 1024);
    if(res)
        exit_with("listen error.");
}

void handle_new_connection()
{
    cout << "Accepting new connection." << endl;

    struct sockaddr_in client_sockaddr_in;
    int size_of_address = sizeof ( sockaddr_in );
    int client_socket_fd = accept ( server_socket_fd, ( struct sockaddr * ) &client_sockaddr_in, ( socklen_t * ) &size_of_address );

    if ( client_socket_fd < 0 )
    {
        cout << "accept error." << endl;
        cout << "CANT HANDLE CLIENT" << endl;
        return;
    }

    Client * client = new Client ( client_socket_fd );
    clients.push_back( client );
}

void exit_with(const char * mess)
{
    perror ( mess );
    cout << endl;
    exit_handler(153);
}

void exit_handler (int sig)
{
    cout << endl << "Exiting with code: " << sig << endl;

    for ( auto it = clients.begin(); it != clients.end() ; )
    {
        delete * it;
        it = clients.erase(it);
    }

    for ( auto it = downloaders.begin(); it != downloaders.end() ; )
    {
        delete * it;
        it = downloaders.erase(it);
    }

    for ( auto it = cache.begin(); it != cache.end() ; )
    {
        delete it -> second;
        it = cache.erase(it);
    }

    close(server_socket_fd);

    exit(EXIT_SUCCESS);
}
