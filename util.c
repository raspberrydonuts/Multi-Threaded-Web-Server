#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

static int master_fd = -1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// questions:
// how do i test request contain at least two strings, and that its' GET, and contains .. // and size 1023
// only need mutex for accepting connection? so then why does
// the string parsing need to be threadsafe? like when i use strtok is this not ok?
// max size of GET response header = 2048? Where do I incorporate this check?
// where to see i'm returning results?


/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - YOU MUST CALL THIS EXACTLY ONCE (not once per thread,
     but exactly one time, in the main thread of your program)
     BEFORE USING ANY OF THE FUNCTIONS BELOW
   - if init encounters any errors, it will call exit().
************************************************/
// sets up a socket for receiving client connection requests on the port provided
// newly created socket should be used by all dispatch threads for accepting client connection requests
void init(int port) {
    int sockfd;
    struct sockaddr_in my_addr;
    int enable = 1;

    // create the socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Can't create socket\n");
        exit(1);
    }

    // bind socket, make address reusable
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // set reuse option to shut down and restart without waiting for
    // the standard timeout
    if (setsockopt(sockfd,SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        perror("Can't set socket option\n");
        exit(1);
    }

    // bind socket
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
        perror("Could not bind\n");
        exit(1);
    }

    // set up queue for incoming connection requeusts
    if ((listen(sockfd, 20)) == -1) {
        perror("Could not find incoming connection request\n");
        exit(1);
    }
    master_fd = sockfd;
    printf("reached end of init\n");
}

/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
     DO NOT use the file descriptor on your own -- use
     get_request() instead.
   - if the return value is negative, the request should be ignored.
***********************************************/
// return new socket descriptor for communicating
int accept_connection(void) {
    if (pthread_mutex_lock(&mutex) < 0) {
        printf("Failed to lock mutex\n");
        exit(1);
    }

    int new_sd;
    struct sockaddr_in client_addr;
    socklen_t addr_size;
    addr_size = sizeof(client_addr);

    // give channel to requester
    new_sd = accept(master_fd, (struct sockaddr *)&client_addr, &addr_size);
    if (new_sd < 0) {
        perror("Failed to accept socket\n");
        exit(1);
    }

    if (pthread_mutex_unlock(&mutex) < 0) {
        printf("Failed to unlock mutex\n");
        exit(1);
    }
    printf("reached end of accept connection\n");
    return new_sd;
}

/**********************************************
 * get_request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        from where you wish to get a request
      - filename is the location of a character buffer in which
        this function should store the requested filename. (Buffer
        should be of size 1024 bytes.)
   - returns 0 on success, nonzero on failure. You must account
     for failures because some connections might send faulty
     requests. This is a recoverable error - you must not exit
     inside the thread that called get_request. After an error, you
     must NOT use a return_request or return_error function for that
     specific 'connection'.
************************************************/
// parse request, copy requested file path into filename output parameter
int get_request(int fd, char *filename) {
    char buf[2048];
    char *tempFileName;
    FILE *stream = fdopen(fd, "r");
    if (stream == NULL) {
        printf("Couldn't open fd\n");
        close(fd);
        return -1;
    }
    if (fgets(buf, 2048, stream) == NULL) {
        printf("Fgets failed\n");
        close(fd);
        return -1;
    }

    // check to make sure that there's at least two strings
    // how to test this?
    int strCount = 0;
    for (int i = 0; i < strlen(buf); i++) {
        printf("%c\n", buf[i]);
        if (buf[i] == ' ') {
            strCount++;
        }
    }
    if (strCount < 2) {
        printf("count is %d\n", strCount);
        printf("First line of request has less than 2 strings\n");
        close(fd);
        return -1;
    }

    printf("%s\n", buf);
    char *token;
    char delim[2] = " ";\
    // is it ok to use this
    token = strtok(buf, delim);
    printf("%s\n", token);


    // check that it's a GET request
    // how to test?
    if (strcmp(token, "GET") != 0) {
        printf("Not a GET request\n");
        close(fd);
        return -1;
    }
    tempFileName = strtok(NULL, delim);

    // check to make sure file length is 1023 bytes max
    if (strlen(tempFileName) > 1023) {
        printf("File length more than 1023 bytes\n");
        close(fd);
        return -1;
    }

    // check to make sure no ".." or "//"
    if (strstr(tempFileName, "..") != NULL || strstr(tempFileName, "//") != NULL) {
        printf(".. or // detected\n");
        close(fd);
        return -1;
    }
    strncpy(filename, tempFileName, 1024);
    printf("%s\n", filename);

    return 0;
}

/**********************************************
 * return_result
   - returns the contents of a file to the requesting client
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the result of a request
      - content_type is a pointer to a string that indicates the
        type of content being returned. possible types include
        "text/html", "text/plain", "image/gif", "image/jpeg" cor-
        responding to .html, .txt, .gif, .jpg files.
      - buf is a pointer to a memory location where the requested
        file has been read into memory (the heap). return_result
        will use this memory location to return the result to the
        user. (remember to use -D_REENTRANT for CFLAGS.) you may
        safely deallocate the memory after the call to
        return_result (if it will not be cached).
      - numbytes is the number of bytes the file takes up in buf
   - returns 0 on success, nonzero on failure.
************************************************/
int return_result(int fd, char *content_type, char *buf, int numbytes) {
    printf("fd is %d\n", fd);
    FILE *stream = fdopen(fd, "w");
    if (stream == NULL) {
        printf("Couldn't open fd in return_result\n");
        close(fd);
        return -1;
    }
    if (fprintf(stream, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %d\nConnection: Close\n\n", content_type, numbytes) < 0) {
        printf("Failed to print to stream\n");
        close(fd);
        return -1;
    }
    if (fflush(stream) < 0) {
        printf("Failed to flush stream\n");
        close(fd);
        return -1;
    }
    if (write(fd, buf, numbytes) < 0) {
        printf("Failed to write result to client\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/**********************************************
 * return_error
   - returns an error message in response to a bad request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the error
      - buf is a pointer to the location of the error text
   - returns 0 on success, nonzero on failure.
************************************************/
int return_error(int fd, char *buf) {
    FILE *stream = fdopen(fd, "w");
    if (stream == NULL) {
        printf("Couldn't open fd in return_error\n");
        close(fd);
        return -1;
    }
    if (fprintf(stream, "HTTP/1.1 404 Not Found\nContent-Length: %ld\nConnection: Close\n\n", strlen(buf)) < 0) {
        printf("Failed to print to stream\n");
        close(fd);
        return -1;
    }
    if (fflush(stream) < 0) {
        printf("Failed to flush stream\n");
        close(fd);
        return -1;
    }
    if (write(fd, buf, strlen(buf)) < 0) {
        printf("Failed to write error to client\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}
