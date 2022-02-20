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
#include <string>
#include <strings.h>
#include <sys/wait.h>
#include <semaphore.h>
#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag
int g_debug = LOG_INFO;
sem_t *sem;
char *array[50]={0};
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
void clientFunc(int socket, char header[], char footer[]){

    
    
    int l_read;
    int findHTTP;
    int findGET;
    
    char buffer[2056] = {0};
    char *tmp;
    int index =0;
    l_read = read(socket, buffer, sizeof(buffer));
    if(l_read <=0){
        log_msg(LOG_INFO, "client disconnected");
        close(socket);

    }
    else{
        std::string checkGetHTTP = buffer;
        findHTTP = checkGetHTTP.find("HTTP");
        findGET = checkGetHTTP.find("GET");

        char *firstLine = strtok(buffer, "\n"); // timto ziskat prvni radek
        char *firstWord = strtok(firstLine, "/"); // timto si vymazes z prvniho radku prvni slovo, tedy GET
        char* afterFirstWord = strtok(NULL, ""); // tady mas ulozene ls*-la*/etc HTTP/1.1
        tmp = strtok(afterFirstWord, " "); // tady mas ulozene ls*-la*/etc
        printf("tmp1: %s\n", tmp);
        char *realcommandFirst = strtok(tmp, "*"); //tady mas uz jen ls. Splitnul sis to totiz podle *, tudiz ti tu zbylo jen ls
        
        while(realcommandFirst!= NULL){ // while na to, aby sis do pole ulozil dalsi prikazy
            array[index] = realcommandFirst;
            index++;
            realcommandFirst = strtok(NULL, "*"); 
        }
    }
    
    if(findHTTP == -1 || findGET == -1){
        log_msg(LOG_INFO, "bad request, no GET or HTTP found, exiting");
        close(socket);
        exit(1);
    }

    printf("tmp: %s\n", tmp);

    for (int i = 0; i < 10; i++)
    {
        printf("arr: %s\n", array[i]);
    }
    
    write(socket, header, strlen(header));
    sem_wait(sem);
    pid_t child;
    child = fork();
    if(child==0){
        if(dup2(socket, STDOUT_FILENO)==-1){
            log_msg(LOG_ERROR, "didnt workout");
            exit(1);
        }
        if(execvp(tmp, array)==1){
            log_msg(LOG_ERROR, "exec didnt wourkout");
            exit(1);
        }
    }
    else{
        waitpid(child, nullptr, 0);
        write(socket, footer, strlen(footer));
       
    }
    sem_post(sem);
    
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

    char head[] = {"HTTP/1.1 200 OK\n"
                    "Server: OSY/1.1.1 (Ubuntu)\n"
                    "Accept-Ranges: bytes\n"
                    "Vary: Accept-Encoding\n"
                    "Content-Type: text/html\n\n"
                    "<!DOCTYPE html>\n"
                    "<html>\n"
                    "<head>\n<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\" /></head>\n"
                    "<body><h2>CMD:</h2><pre>\n"};
    char footer[]= "</pre>---</body></html>\n";

    sockaddr_in l_rsa;
    int l_rsa_size = sizeof(l_rsa);
    int newSocket;
    // go!
    sem = sem_open("/semaphore1", O_CREAT | O_RDWR, 0644);
    sem_init(sem, 1,1);

    while ( 1 )
    {
        newSocket = accept(l_sock_listen, (sockaddr*)&l_rsa, (socklen_t*)&l_rsa_size);
        if(newSocket <0){
            log_msg(LOG_INFO, "client disconnected");
            close(newSocket);
        }
        if(fork()==0){
            close(l_sock_listen);
            clientFunc(newSocket, head, footer);
            close(newSocket);
            exit(0);
        }
        else{
            close(newSocket);
        }
            
    } // while ( 1 )
    //close(newSocket);
    return 0;
}
