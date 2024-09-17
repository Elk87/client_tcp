#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "calcLib.h"

// Uncomment the next line to enable debug mode
// #define DEBUG

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <hostname>:<port>\n", argv[0]);
        return 1;
    }

    // Parse hostname and port
    char *input = argv[1];
    char *colonPos = strchr(input, ':');
    if (colonPos == NULL) {
        printf("ERROR: Invalid input format. Expected <hostname>:<port>\n");
        return 1;
    }

    *colonPos = '\0';
    char *hostname = input;
    char *portStr = colonPos + 1;

    printf("Host %s, and port %s.\n", hostname, portStr);

    struct addrinfo hints, *res, *rp;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    int status = getaddrinfo(hostname, portStr, &hints, &res);
    if (status != 0) {
        printf("ERROR: RESOLVE ISSUE (%s)\n", gai_strerror(status));
        return 1;
    }

    // Try to connect to one of the addresses returned by getaddrinfo
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break; // Success

        close(sockfd);
    }

    if (rp == NULL) {
        printf("ERROR: CANT CONNECT TO %s\n", hostname);
        freeaddrinfo(res);
        return 1;
    }

#ifdef DEBUG
    char addrstr[INET6_ADDRSTRLEN];
    void *addr;
    if (rp->ai_family == AF_INET) { // IPv4
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
        addr = &(ipv4->sin_addr);
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
        addr = &(ipv6->sin6_addr);
    }
    inet_ntop(rp->ai_family, addr, addrstr, sizeof(addrstr));
    printf("Connected to %s:%s local %s\n", hostname, portStr, addrstr);
#endif

    freeaddrinfo(res);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Read supported protocols from server
    int n = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (n <= 0) {
        printf("ERROR: Reading from socket\n");
        close(sockfd);
        return 1;
    }

    if (strstr(buffer, "TEXT TCP 1.0") == NULL) {
        printf("ERROR: MISSMATCH PROTOCOL\n");
        close(sockfd);
        return 1;
    }

    // Send OK response to server
    n = write(sockfd, "OK\n", 3);
    if (n < 0) {
        printf("ERROR: Writing to socket\n");
        close(sockfd);
        return 1;
    }

    // Read operation from server
    memset(buffer, 0, BUFFER_SIZE);
    n = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (n <= 0) {
        printf("ERROR: Reading from socket\n");
        close(sockfd);
        return 1;
    }

    printf("ASSIGNMENT: %s", buffer);

    // Parse operation and compute result
    char op[5];
    double val1, val2, result;
    sscanf(buffer, "%s %lf %lf", op, &val1, &val2);

    if (strcmp(op, "add") == 0) result = (int)(val1 + val2);
    else if (strcmp(op, "sub") == 0) result = (int)(val1 - val2);
    else if (strcmp(op, "mul") == 0) result = (int)(val1 * val2);
    else if (strcmp(op, "div") == 0) result = (int)(val1 / val2);
    else if (strcmp(op, "fadd") == 0) result = val1 + val2;
    else if (strcmp(op, "fsub") == 0) result = val1 - val2;
    else if (strcmp(op, "fmul") == 0) result = val1 * val2;
    else if (strcmp(op, "fdiv") == 0) result = val1 / val2;
    else {
        printf("ERROR: Invalid operation\n");
        close(sockfd);
        return 1;
    }

#ifdef DEBUG
    printf("Calculated the result to %.8g\n", result);
#endif

    // Send the result to server
    snprintf(buffer, BUFFER_SIZE, "%.8g\n", result);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        printf("ERROR: Writing to socket\n");
        close(sockfd);
        return 1;
    }

    // Read the server's response
    memset(buffer, 0, BUFFER_SIZE);
    n = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (n <= 0) {
        printf("ERROR: Reading from socket\n");
        close(sockfd);
        return 1;
    }

    if (strstr(buffer, "OK") != NULL) {
        printf("OK (myresult=%.8g)\n", result);
    } else {
        printf("%s", buffer);
    }

    close(sockfd);
    return 0;
}
