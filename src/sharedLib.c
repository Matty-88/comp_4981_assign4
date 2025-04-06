#include <stdio.h>
#include <ndbm.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../include/sharedLib.h"

#define PATH_SIZE 1024
#define BUFFER_SIZE 1024
#define KEY_SIZE 64

const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if(!ext)
    {
        return "application/octet-stream";
    }

    // Map file extensions to MIME types
    if(strcmp(ext, ".html") == 0)
    {
        return "text/html";
    }
    if(strcmp(ext, ".css") == 0)
    {
        return "text/css";
    }
    if(strcmp(ext, ".js") == 0)
    {
        return "application/javascript";
    }
    if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    if(strcmp(ext, ".png") == 0)
    {
        return "image/png";
    }
    if(strcmp(ext, ".gif") == 0)
    {
        return "image/gif";
    }
    if(strcmp(ext, ".swf") == 0)
    {
        return "application/x-shockwave-flash";
    }

    return "application/octet-stream";
}

int handleRequest(int client_sockfd, const char *method, const char *uri, const char *full_request)
{
    char filepath[PATH_SIZE];
    char buffer[BUFFER_SIZE];
    char keyStr[KEY_SIZE];
    datum key;
    const char *conflictMessage;
    const char *not_supported;
    datum value;
    char header[BUFFER_SIZE];
    const char *mime_type;
    ssize_t bytes;
    const char *success_msg;
    char *content_length_str;
    const char *bad_request;
    struct stat file_stat;
    int content_length;
    char *body;
    const char *error_msg;
    int fd;
    DBM *db;
    char dbname[] = "postRequests";

    if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0)
    {
        if (strcmp(uri, "/") == 0)
            snprintf(filepath, sizeof(filepath), "www/index.html");
        else
            snprintf(filepath, sizeof(filepath), "www%s", uri);

        fd = open(filepath, O_RDONLY);
        if (fd < 0)
        {
            const char *not_found = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
                                    "<html><body><h1>404 Not Found</h1></body></html>";
            write(client_sockfd, not_found, strlen(not_found));
            return -1;
        }

        mime_type = get_mime_type(filepath);

		// Get file size for Content-Length

		if (stat(filepath, &file_stat) < 0)
		{
    		const char *not_found = "HTTP/1.0 404 Not Found\r\n"
                            "Content-Type: text/html\r\n\r\n"
                            "<html><body><h1>404 Not Found</h1></body></html>";
    		write(client_sockfd, not_found, strlen(not_found));
    		return -1;
}

		// Dynamically build the header

		snprintf(header, sizeof(header),
         "HTTP/1.0 200 OK\r\n"
         "Content-Type: %s\r\n"
         "Content-Length: %ld\r\n"
         "Connection: close\r\n\r\n",
         mime_type, file_stat.st_size);




        write(client_sockfd, header, strlen(header));

        if (strcmp(method, "GET") == 0)
        {
            while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
                write(client_sockfd, buffer, (size_t)bytes);
        }

        close(fd);
        return 0;
    }
    else if (strcmp(method, "POST") == 0)
    {
        printf("Handling POST\n");

        body = strstr(full_request, "\r\n\r\n");
        if (body)
        {
            body += 4; // Skip delimiter

            // Extract Content-Length from headers
            content_length_str = strstr(full_request, "Content-Length:");
            content_length = 0;
            if (content_length_str)
            {
                sscanf(content_length_str, "Content-Length: %d", &content_length);
            }

            if (content_length <= 0)
            {
                bad_request = "HTTP/1.0 411 Length Required\r\n\r\n";
                write(client_sockfd, bad_request, strlen(bad_request));
                return -1;
            }

            printf("Body to store: \"%.*s\"\n", content_length, body);

            // Open DB
            db = dbm_open(dbname, O_RDWR | O_CREAT, 0644);
            if (!db)
            {
                perror("dbm_open failed");
                error_msg = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
                write(client_sockfd, error_msg, strlen(error_msg));
                return -1;
            }

            // Create key
            snprintf(keyStr, sizeof(keyStr), "%ld", time(NULL));
            key.dptr = keyStr;
            key.dsize = (int)strlen(keyStr);

            // Store correctly with content length
            value.dptr = body;
            value.dsize = content_length;

            printf("Value size being stored: %d\n", value.dsize);
			printf("Value raw bytes:\n");
			for (int i = 0; i < value.dsize; i++) {
    		printf("%02x ", (unsigned char)value.dptr[i]);
			}
			printf("\n");

            if (dbm_store(db, key, value, DBM_INSERT) != 0)
            {
                conflictMessage = "HTTP/1.0 409 Conflict\r\n\r\n";
                write(client_sockfd, conflictMessage, strlen(conflictMessage));
            }
            else
            {
                success_msg = "HTTP/1.0 200 OK\r\n\r\ndata stored\n";
                write(client_sockfd, success_msg, strlen(success_msg));
            }

            dbm_close(db);
            return 0;
        }
        else
        {
            bad_request = "HTTP/1.0 400 Bad Request\r\n\r\n";
            write(client_sockfd, bad_request, strlen(bad_request));
            return -1;
        }
    }
    else
    {
        not_supported = "HTTP/1.0 405 Bad method\r\n\r\n";
        write(client_sockfd, not_supported, strlen(not_supported));
        return -1;
    }
}
