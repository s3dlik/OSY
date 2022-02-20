#include<iostream>
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
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <semaphore.h>
#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

struct shm_data
{
  char gameBoard[6][6];
};
shm_data *g_glb_data = nullptr;
sem_t *semaphores[2];

// debug flag
int g_debug = LOG_INFO;
using namespace std;
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


void clientfunc(int socket, int clientIndex){
    int l_read;
    while(1){
        char buffer[128] = {0};
        write(socket, "Please wait\n", strlen("Please wait\n"));
        sem_wait(semaphores[clientIndex]);

        write(socket, g_glb_data->gameBoard, strlen((char*)g_glb_data->gameBoard));
        write(socket, "Zadej tah\n", strlen("Zadej tah\n"));
        l_read = read(socket, buffer, sizeof(buffer));
        if(l_read <=0){
            log_msg(LOG_INFO, "client disconnected");
            close(socket);
        }
        char *row = strtok(buffer, "-");
        printf("%s\n", row);
        char *col = strtok(NULL, "-");
        int rowIndex = 0;

        if(!strncasecmp(row, "a", strlen("c"))){
            rowIndex = 0;
        }
        else if(!strncasecmp(row, "b", strlen("b"))){
            rowIndex =1;
        }
        else if(!strncasecmp(row, "c", strlen("c"))){
            rowIndex =2;
        }
        else if(!strncasecmp(row, "d", strlen("d"))){
            rowIndex =3;
        }
        else if(!strncasecmp(row, "e", strlen("e"))){
            rowIndex =4;
        }

        g_glb_data->gameBoard[atoi(col)][rowIndex] = 'O';
        write(socket, g_glb_data->gameBoard, strlen((char*)g_glb_data->gameBoard));
        sem_post(semaphores[!clientIndex]);
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

    int l_first = 0;

    int l_fd = shm_open( "/shm_example", O_RDWR, 0660 );
    printf("%d\n", l_fd);
    if ( l_fd < 0 )
    {
        log_msg( LOG_ERROR, "Unable to open file for shared memory." );
        l_fd = shm_open( "/shm_example", O_RDWR | O_CREAT, 0660 );
        if ( l_fd < 0 )
        {
            log_msg( LOG_ERROR, "Unable to create file for shared memory." );
            exit( 1 );
        }
        ftruncate( l_fd, sizeof( shm_data ) );
        log_msg( LOG_INFO, "File created, this process is first" );
        l_first = 1;
    }

    g_glb_data = ( shm_data * ) mmap( nullptr, sizeof( shm_data ), PROT_READ | PROT_WRITE,
            MAP_SHARED, l_fd, 0 );

    if ( !g_glb_data )
    {
        log_msg( LOG_ERROR, "Unable to attach shared memory!" );
        exit( 1 );
    }
    else
        log_msg( LOG_INFO, "Shared memory attached.");

    if(l_first){
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                g_glb_data->gameBoard[i][j] = '-';
            }
            
            g_glb_data->gameBoard[i][5]='\n';
        }
    }
    semaphores[0] = sem_open("/semaphore1", O_CREAT | O_RDWR, 0660);
    semaphores[1] = sem_open("/semaphore2", O_CREAT | O_RDWR, 0660);

    sem_init(semaphores[0],1,1);
    sem_init(semaphores[1],1,0);

    //printf("%c", g_glb_data->gameBoard[0][0]);
    // go!
    sockaddr_in l_rsa;
    int l_rsa_size = sizeof(l_rsa);
    int newSocket;
    int clientCounter = -1;
    while ( 1 )
    {
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                g_glb_data->gameBoard[i][j] = '.';
            }
            
            g_glb_data->gameBoard[i][5]='\n';
        }
        newSocket = accept(l_sock_listen, (sockaddr*)&l_rsa, (socklen_t*)&l_rsa_size);
        if(newSocket < 0){
            log_msg(LOG_ERROR, "accept wasnt succesfull");
            close(newSocket);
        }
        clientCounter++;
        
        if(fork()==0){
            
            clientfunc(newSocket,clientCounter);
        }
    } // while ( 1 )
    close(newSocket);
    return 0;
}
