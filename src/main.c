#include "../include/display.h"
#include <arpa/inet.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define LISTEN_NUM 5
#define METHOD_SIZE 16
#define URI_SIZE 256
#define VERSION_SIZE 16
#define FILEPATH_SIZE 1024

int main(void)
{
    int                sockfd;
    struct sockaddr_in host_addr;
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len;

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

    while(1)
    {
        int        newsockfd;
        char       filepath[FILEPATH_SIZE];
        char       method[METHOD_SIZE];
        char       uri[URI_SIZE];
        char       version[VERSION_SIZE];
        ssize_t    bytesRead;
        ssize_t    bytesWritten;
        char       buffer[BUFFER_SIZE];
        const char resp[] = "HTTP/1.0 200 OK\r\n"
                            "Server: webserver-c\r\n"
                            "Content-type: text/html\r\n\r\n"
                            "<html>hello, world</html>\r\n";

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr_len = sizeof(client_addr);    // Always reset before accept
        newsockfd       = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(newsockfd < 0)
        {
            perror("accept failed");
            continue;
        }

        bytesRead = read(newsockfd, buffer, BUFFER_SIZE);
        if(bytesRead < 0)
        {
            perror("read failed");
            close(newsockfd);
            continue;
        }

        sscanf(buffer, "%15s %255s %15s", method, uri, version);
        printf("Method: %s\n", method);
        printf("URI: %s\n", uri);
        printf("Version: %s\n", version);

        snprintf(filepath, sizeof(filepath), "www%s", uri);

        printf("File: %s\n", filepath);

        bytesWritten = write(newsockfd, resp, strlen(resp));
        if(bytesWritten < 0)
        {
            perror("write failed");
            close(newsockfd);
            continue;
        }

        close(newsockfd);
    }

    return EXIT_SUCCESS;
}
