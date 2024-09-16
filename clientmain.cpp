#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "calcLib.h"

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
    int port = atoi(colonPos + 1);

    printf("Host %s, and port %d.\n", hostname, port);

    // Set up socket
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("ERROR: CANT CONNECT TO %s\n", hostname);
        return 1;
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        printf("ERROR: RESOLVE ISSUE\n");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("ERROR: CANT CONNECT TO %s\n", hostname);
        close(sockfd);
        return 1;
    }

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

    printf("%s", buffer);
    close(sockfd);
    return 0;
}

