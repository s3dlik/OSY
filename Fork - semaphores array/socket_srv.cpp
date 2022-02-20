#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages
#define N                       3
// debug flag
int g_debug = LOG_INFO;
sem_t *semaphores[N];
void log_msg( int t_log_level, const char *t_form, ... )
{
    const char *out_fmt[] = {
            "ERR: (%d-%s) %s\n",
            "INF: %s\n",
            "DEB: %s\n" };

    if ( t_log_level && t_log_level > g_debug ) return;

    char l_buf[ 1024 ];
    va_list l_arg;
    va_start( l_arg, t_form );
    vsprintf( l_buf, t_form, l_arg );
    va_end( l_arg );

    switch ( t_log_level )
    {
    case LOG_INFO:
    case LOG_DEBUG:
        fprintf( stdout, out_fmt[ t_log_level ], l_buf );
        break;

    case LOG_ERROR:
        fprintf( stderr, out_fmt[ t_log_level ], errno, strerror( errno ), l_buf );
        break;
    }
}

//***************************************************************************
// help

void help( int t_narg, char **t_args )
{
    if ( t_narg <= 1 || !strcmp( t_args[ 1 ], "-h" ) )
    {
        printf(
            "\n"
            "  Socket server example.\n"
            "\n"
            "  Use: %s [-h -d] port_number\n"
            "\n"
            "    -d  debug mode \n"
            "    -h  this help\n"
            "\n", t_args[ 0 ] );

        exit( 0 );
    }

    if ( !strcmp( t_args[ 1 ], "-d" ) )
        g_debug = LOG_DEBUG;
}

//***************************************************************************

void clientFunc(int socket, int n){
    int l_read;
    int l_write;
    while(1){
        
        sem_wait(semaphores[n]);
        for (int i = 0; i < 10; i++)
        {
            char buffer[128] = {0};
        
            l_read = read(socket, buffer, sizeof(buffer));
            if(l_read <=0){
                log_msg(LOG_ERROR, "client disconnected");
                close(socket);
                break;
            }
            else{
                printf("client send: %s\n", buffer);
                char buffertoSend[256]={0};
                sprintf(buffertoSend, "%d%s", i, buffer);
                write(socket, buffertoSend, strlen(buffertoSend));
                sleep(1);
            }
        }
        sem_post(semaphores[(n+1)%N]);
    }   
}


int main( int t_narg, char **t_args )
{
    if ( t_narg <= 1 ) help( t_narg, t_args );

    int l_port = 0;

    // parsing arguments
    for ( int i = 1; i < t_narg; i++ )
    {
        if ( !strcmp( t_args[ i ], "-d" ) )
            g_debug = LOG_DEBUG;

        if ( !strcmp( t_args[ i ], "-h" ) )
            help( t_narg, t_args );

        if ( *t_args[ i ] != '-' && !l_port )
        {
            l_port = atoi( t_args[ i ] );
            break;
        }
    }

    if ( l_port <= 0 )
    {
        log_msg( LOG_INFO, "Bad or missing port number %d!", l_port );
        help( t_narg, t_args );
    }

    log_msg( LOG_INFO, "Server will listen on port: %d.", l_port );

    // socket creation
    int l_sock_listen = socket( AF_INET, SOCK_STREAM, 0 );
    if ( l_sock_listen == -1 )
    {
        log_msg( LOG_ERROR, "Unable to create socket.");
        exit( 1 );
    }

    in_addr l_addr_any = { INADDR_ANY };
    sockaddr_in l_srv_addr;
    l_srv_addr.sin_family = AF_INET;
    l_srv_addr.sin_port = htons( l_port );
    l_srv_addr.sin_addr = l_addr_any;

    // Enable the port number reusing
    int l_opt = 1;
    if ( setsockopt( l_sock_listen, SOL_SOCKET, SO_REUSEADDR, &l_opt, sizeof( l_opt ) ) < 0 )
      log_msg( LOG_ERROR, "Unable to set socket option!" );

    // assign port number to socket
    if ( bind( l_sock_listen, (const sockaddr * ) &l_srv_addr, sizeof( l_srv_addr ) ) < 0 )
    {
        log_msg( LOG_ERROR, "Bind failed!" );
        close( l_sock_listen );
        exit( 1 );
    }

    // listenig on set port
    if ( listen( l_sock_listen, 1 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( 1 );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );
    sockaddr_in l_rsa;
    int l_rsa_size = sizeof(l_rsa);
    int newSocket;

    char sem1[15] = "/semaphore0";
    char sem2[15] = "/semaphore1";
    char sem3[15] = "/semaphore";

    char fullSem1[25];
    char fullSem2[25];
    char fullSem3[25];

    sprintf(fullSem1, "%s%d", sem1, l_port);
    sprintf(fullSem2, "%s%d", sem2, l_port);
    sprintf(fullSem3, "%s%d", sem3, l_port);

    semaphores[0] = sem_open(fullSem1, O_CREAT | O_RDWR, 0664);
    semaphores[1] = sem_open(fullSem2, O_CREAT | O_RDWR, 0664);
    semaphores[2] = sem_open(fullSem3, O_CREAT | O_RDWR, 0664);

    sem_init(semaphores[0], 1,1);
    sem_init(semaphores[1], 1,0);
    sem_init(semaphores[2], 1,0);
    int i =-1;
    // go!
    while ( 1 )
    {
        newSocket = accept(l_sock_listen, (sockaddr*)&l_rsa, (socklen_t*)&l_rsa_size);
        if(newSocket < 0){
            log_msg(LOG_ERROR, "Accept wasnt successfull");
            close(newSocket);
        }
        else{
            i++;
            if(fork()==0){
                close(l_sock_listen);
                clientFunc(newSocket,i);
            }
        }
    } // while ( 1 )
    close(newSocket);
    return 0;
}
