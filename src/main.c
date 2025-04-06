#include "../include/display.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <error.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t childDead = 0;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#define PORT 8080
#define BUFFER_SIZE 1024
#define LISTEN_NUM 5
#define METHOD_SIZE 16
#define URI_SIZE 256
#define VERSION_SIZE 16

#define NUM_OF_CHILDS 4
// #define MAX_CHILD 16

void handleConnect(int sockfd);
void sigHandler(int sig);
void sigChildHandle(int sig);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-noreturn"

void handleConnect(int sockfd)
{
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len;
    static void       *libHandle;
    static time_t      lastTime = 0;
    struct stat        st;
    while(1)
    {
        int newsockfd;
        // cppcheck-suppress constVariablePointer

        char    method[METHOD_SIZE];
        char    uri[URI_SIZE];
        char    version[VERSION_SIZE];
        ssize_t totalBytesRead = 0;
        ssize_t bytesRead;

        // ssize_t    bytesWritten;
        char buffer[BUFFER_SIZE];
        int (*handle_request_func)(int, const char *, const char *, const char *) = NULL;

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr_len = sizeof(client_addr);    // Always reset before accept
        newsockfd       = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(newsockfd < 0)
        {
            perror("accept failed");
            continue;
        }

        while((bytesRead = read(newsockfd, buffer + totalBytesRead, (size_t)(BUFFER_SIZE - totalBytesRead - 1))) > 0)
        {
            totalBytesRead += bytesRead;
            buffer[totalBytesRead] = '\0';

            if(strstr(buffer, "\r\n\r\n"))
            {
                // End of headers
                break;
            }
        }

        if(bytesRead < 0)
        {
            perror("read failed");
            close(newsockfd);
            continue;
        }

        // buffer[bytesRead] = '\0';
        sscanf(buffer, "%15s %255s %15s", method, uri, version);
        printf("Method: %s\n", method);
        printf("URI: %s\n", uri);
        printf("Version: %s\n", version);

        // Step 1: Check if handler.so was updated

        if(stat("sharedLib.so", &st) == 0 && st.st_mtime != lastTime)
        {
            if(libHandle)
            {
                dlclose(libHandle);
            }

            libHandle = dlopen("./sharedLib.so", RTLD_LAZY);
            if(!libHandle)
            {
                fprintf(stderr, "dlopen error: %s\n", dlerror());
                close(newsockfd);
                continue;
            }

            lastTime = st.st_mtime;
            printf("sharedLib.so reloaded\n");
        }

        // Step 2: Get function pointer

        *(void **)(&handle_request_func) = dlsym(libHandle, "handleRequest");
        if(!handle_request_func)
        {
            fprintf(stderr, "dlsym error: %s\n", dlerror());
            close(newsockfd);
            continue;
        }

        // Step 3: Call request handler from shared library
        handle_request_func(newsockfd, method, uri, buffer);

        close(newsockfd);
    }
}

#pragma GCC diagnostic pop

void sigChildHandle(int sig)
{
    (void)sig;
    childDead = 1;
}

int main(void)
{
    int                sockfd;
    struct sockaddr_in host_addr;
    pid_t              newPID;
    int                status;
    pid_t              pid;
    struct sigaction   action;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    printf("Socket successfully created\n");

    host_addr.sin_family      = AF_INET;
    host_addr.sin_port        = htons(PORT);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr *)&host_addr, sizeof(host_addr)) != 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Bind successful\n");

    if(listen(sockfd, LISTEN_NUM) != 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d\n", PORT);

#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif

    action.sa_handler = sigChildHandle;

#if defined(__clang__)
    #pragma clang diagnostic pop
#endif

    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &action, NULL);

    for(int i = 0; i < NUM_OF_CHILDS; i++)
    {
        pid = fork();
        if(pid < 0)
        {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        else if(pid == 0)
        {
            printf("Child PID: %d\nChild number: %d\n", getpid(), i);
            handleConnect(sockfd);
            exit(EXIT_SUCCESS);
        }
    }

    while(1)
    {
        pause();

        if(childDead)
        {
            childDead = 0;

            while((pid = waitpid(-1, &status, WNOHANG)) > 0)
            {
                printf("worker (PID %d) terminated. restarting\n", pid);

                newPID = fork();
                if(newPID == 0)
                {
                    handleConnect(sockfd);
                    exit(EXIT_SUCCESS);
                }

                printf("new worker %d started\n", newPID);
            }
        }
    }

    return EXIT_SUCCESS;
}
