#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include "calcLib.h"
#include <cmath>

// Uncomment the next line to enable debug mode
// #define DEBUG

#define BUFFER_SIZE 1024
#define BACKLOG 5 // Número de clientes que pueden estar en cola

// Función para establecer un tiempo de espera en el socket
int set_timeout(int sockfd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    // Establecer tiempo de espera para lectura
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        return -1;
    }
    // Establecer tiempo de espera para escritura
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO failed");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <IP|DNS>:<PORT>\n", argv[0]);
        return 1;
    }

    // Parsear IP/DNS y Puerto
    char *input = argv[1];
    char *colonPos = strchr(input, ':');
    if (colonPos == NULL) {
        printf("ERROR: Invalid input format. Expected <IP|DNS>:<PORT>\n");
        return 1;
    }

    *colonPos = '\0';
    char *hostname = input;
    char *portStr = colonPos + 1;

    printf("Server will listen on %s:%s\n", hostname, portStr);

    struct addrinfo hints, *res, *rp;
    int listen_sock;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // Para uso con bind

    int status = getaddrinfo(hostname, portStr, &hints, &res);
    if (status != 0) {
        printf("ERROR: RESOLVE ISSUE (%s)\n", gai_strerror(status));
        return 1;
    }

    // Crear y enlazar el socket
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_sock == -1) continue;

        // Permitir reutilización de la dirección
        int opt = 1;
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            close(listen_sock);
            continue;
        }

        if (bind(listen_sock, rp->ai_addr, rp->ai_addrlen) == 0) break; // Éxito

        close(listen_sock);
    }

    if (rp == NULL) {
        printf("ERROR: Failed to bind to %s:%s\n", hostname, portStr);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    // Escuchar en el socket
    if (listen(listen_sock, BACKLOG) < 0) {
        perror("listen failed");
        close(listen_sock);
        return 1;
    }

#ifdef DEBUG
    printf("Server is listening on %s:%s\n", hostname, portStr);
#endif

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("accept failed");
            continue; // No detener el servidor por errores en aceptar
        }

#ifdef DEBUG
        char addrstr[INET6_ADDRSTRLEN];
        void *addr;
        if (client_addr.ss_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop(client_addr.ss_family, addr, addrstr, sizeof(addrstr));
        printf("Accepted connection from %s\n", addrstr);
#endif

        // Establecer tiempo de espera de 5 segundos
        if (set_timeout(client_sock, 5) < 0) {
            close(client_sock);
            continue;
        }

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);

        // Enviar protocolo soportado
        const char *protocol = "TEXT TCP 1.0\n\n";
        if (write(client_sock, protocol, strlen(protocol)) < 0) {
            perror("Error sending protocol");
            close(client_sock);
            continue;
        }

#ifdef DEBUG
        printf("Sent protocol: %s", protocol);
#endif

        // Leer respuesta del cliente
        int n = read(client_sock, buffer, BUFFER_SIZE - 1);
        if (n <= 0) {
            printf("ERROR: Reading from socket\n");
            close(client_sock);
            continue;
        }

#ifdef DEBUG
        printf("Received from client: %s", buffer);
#endif

        if (strcmp(buffer, "OK\n") != 0) {
            printf("ERROR: MISSMATCH PROTOCOL\n");
            close(client_sock);
            continue;
        }

        // Generar asignación aleatoria
        char *op = randomType();
        char msg[1450];
        memset(msg, 0, sizeof(msg));

        double fv1, fv2;
        int iv1, iv2;

        if (op[0] == 'f') { // Operación de punto flotante
            fv1 = randomFloat();
            fv2 = randomFloat();
            snprintf(msg, sizeof(msg), "%s %8.8g %8.8g\n", op, fv1, fv2);
        } else { // Operación entera
            iv1 = randomInt();
            iv2 = randomInt();
            // Evitar división por cero
            if (strcmp(op, "div") == 0 && iv2 == 0) iv2 = 1;
            snprintf(msg, sizeof(msg), "%s %d %d\n", op, iv1, iv2);
        }

#ifdef DEBUG
        printf("Generated assignment: %s", msg);
#endif

        // Enviar asignación al cliente
        if (write(client_sock, msg, strlen(msg)) < 0) {
            perror("Error sending assignment");
            close(client_sock);
            continue;
        }

      // Leer respuesta del cliente
memset(buffer, 0, BUFFER_SIZE);
n = read(client_sock, buffer, BUFFER_SIZE - 1);
if (n <= 0) {
    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // El tiempo de espera expiró
        printf("ERROR: Timeout occurred\n");
        write(client_sock, "ERROR TO\n", 9);  // Enviar mensaje de timeout
    } else if (n == 0) {
        // El cliente cerró la conexión
        printf("Client closed the connection\n");
    } else {
        // Otro error ocurrió
        printf("ERROR: Reading result from client\n");
    }
    close(client_sock);
    continue;
}

#ifdef DEBUG
        printf("Received result from client: %s", buffer);
#endif

        // Calcular resultado esperado
        double expected_result;
        if (op[0] == 'f') { // Operación de punto flotante
            if (strcmp(op, "fadd") == 0) expected_result = fv1 + fv2;
            else if (strcmp(op, "fsub") == 0) expected_result = fv1 - fv2;
            else if (strcmp(op, "fmul") == 0) expected_result = fv1 * fv2;
            else if (strcmp(op, "fdiv") == 0) expected_result = fv1 / fv2;
            else { // Operación inválida
                printf("ERROR: Invalid operation type\n");
                write(client_sock, "ERROR\n", 6);
                close(client_sock);
                continue;
            }
        } else { // Operación entera
            if (strcmp(op, "add") == 0) expected_result = iv1 + iv2;
            else if (strcmp(op, "sub") == 0) expected_result = iv1 - iv2;
            else if (strcmp(op, "mul") == 0) expected_result = iv1 * iv2;
            else if (strcmp(op, "div") == 0) expected_result = iv1 / iv2;
            else { // Operación inválida
                printf("ERROR: Invalid operation type\n");
                write(client_sock, "ERROR\n", 6);
                close(client_sock);
                continue;
            }
        }

        // Parsear resultado del cliente
        double client_result = atof(buffer);

        int is_correct = 0;
        if (op[0] == 'f') { // Comparación de punto flotante
            double diff = fabs(expected_result - client_result);
            if (diff < 0.0001) is_correct = 1;
        } else { // Comparación entera
            if ((int)expected_result == (int)client_result) is_correct = 1;
        }

        if (is_correct) {
            if (write(client_sock, "OK\n", 3) < 0) {
                perror("Error sending OK");
            }
        } else {
            if (write(client_sock, "ERROR\n", 6) < 0) {
                perror("Error sending ERROR");
            }
        }

        close(client_sock);
    }

    close(listen_sock);
    return 0;
}
