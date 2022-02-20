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
#include "pthread.h"
#include <semaphore.h>
#include <algorithm>
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

int array[N];



int globCounter=0;
int msgCounter =0;
sem_t sem;

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
void * communication(void * arg){
    int client = *((int*)arg);
    arg = NULL;
    int l;
    bool tmp = true;
    sem_wait(&sem);
    if(globCounter < N){             
        for (int i = 0; i < N; i++)
        {
            if(array[i] == -1){
                printf("array before: %d\n", array[i]);
                array[i] = client;
                printf("array after: %d\n", array[i]);
                globCounter++;
                printf("globcounter: %d\n", globCounter);
                break;
            }
        }
    }
    sem_post(&sem);
    while (1)
    {
        char buffer[128] = {0};
        l = read(client, buffer, sizeof(buffer));
        if(l<=0){
            log_msg(LOG_INFO, "client disconnected!");
            printf("globcounter: %d\n", globCounter);
            globCounter--;
            printf("globcounter: %d\n", globCounter);
            for (int i = 0; i < N; i++)
            {
                if(array[i] == client){
                    array[i] = -1;
                }
            }
            break;
        }
        else{
            char * firstWord = strtok(buffer, " ");
            msgCounter++;
            printf("msgcounter: %d\n", msgCounter);
            if(!strncasecmp(firstWord, "LOCK", strlen("LOCK"))){
                tmp = false;
                msgCounter = 0;
                
            }
            if(!strncasecmp(firstWord, "UNLOCK", strlen("UNLOCK"))){
                tmp = true;
            }
            if(msgCounter == 5)
                tmp = true;
            if(tmp){
                if(!strncasecmp(buffer, "UNLOCK", strlen("UNLOCK"))){}
                else{
                    char buffWithID[256] = {0};
                    sprintf(buffWithID, "%d %s", client, buffer);
                    for (int i = 0; i < N; i++)
                    {
                        if(array[i] != -1 && array[i] != client){
                            write(array[i], buffWithID, strlen(buffWithID));
                        }
                    }
                }
            }
            /*
            char * firstWord = strtok(buffer, " ");
            printf("msgcnt: %d\n", msgCounter);
            if(!strncasecmp(firstWord, "LOCK", strlen("LOCK"))){
                char buffWithID[256] = {0};
                bool tmp = true;
                sprintf(buffWithID, "%d %s", client, buffer);
                msgCounter++;
                log_msg(LOG_INFO, "locked messages to other");
                while(tmp){
                    if(msgCounter > 5){
                        tmp = false;
                    }
                }
            }
            else if(!strncasecmp(firstWord, "UNLOCK", strlen("UNLOCK"))){
                char buffWithID[256] = {0};
                sprintf(buffWithID, "%d %s", client, buffer);
                for (int i = 0; i < N; i++)
                {
                    if(array[i] != -1 && array[i] != client){
                        write(array[i], buffWithID, strlen(buffWithID));
                    }
                }
            }
            else{
                char buffWithID[256] = {0};
                sprintf(buffWithID, "%d %s", client, buffer);
                for (int i = 0; i < N; i++)
                {
                    if(array[i] != -1 && array[i] != client){
                        write(array[i], buffWithID, strlen(buffWithID));
                    }
                }
            }
            
            msgCounter=0;*/
        }
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
    int l_rsa_len = sizeof(l_rsa);
    int newSocket;
    sem_init(&sem,0,1);
    std::fill_n(array, N, -1);
    // go!
    while ( 1 )
    {
        newSocket = accept(l_sock_listen, (sockaddr*) &l_rsa, (socklen_t*) &l_rsa_len);
        if(newSocket < 0){
            log_msg(LOG_ERROR, "new client wasnt accepted");
            exit(0);
        }
        else{
            for (int i = 0; i < N; i++)
            {
                printf("array: %d\n", array[i]);
            }
            
            sem_wait(&sem);
            

                pthread_t th1;
                int *client = (int*)malloc(sizeof(int));
                *client = newSocket;

                pthread_create(&th1, NULL, communication, client);
                //pthread_detach(th1);
            
            else{
                log_msg(LOG_INFO, "already full clients, closing connection");
                close(newSocket);
            }
            sem_post(&sem);        
        }
    }
    close(newSocket);
    
    return 0;
}
