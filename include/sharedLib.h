#ifndef HANDLER_H
#define HANDLER_H

int handleRequest(int client_sockfd, const char *method, const char *uri, const char *full_request);
const char *get_mime_type(const char *path);

#endif
