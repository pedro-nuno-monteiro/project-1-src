#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define UDP_PORT         9875   // porto fixo do UDP1 (RECEBER)
#define VPN_CLIENT_PORT  9876   // destino (vpn_client)
#define BUFLEN           1024

int main(void) {

    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    // criação socket UDP
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0)
        perror("erro na criação de socket");

    // fazer bind do UDP1 em 127.0.0.1:9875 (para poder receber a qualquer momento)
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serverAddr.sin_port = htons(UDP_PORT);
    if (bind(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        perror("erro no bind UDP1");

    // envio de mensagens para destino: VPN_client em 127.0.0.1:9876
    struct sockaddr_in vpn_cli;
    memset(&vpn_cli, 0, sizeof(vpn_cli));
    vpn_cli.sin_family = AF_INET;
    vpn_cli.sin_addr.s_addr = inet_addr("127.0.0.1");
    vpn_cli.sin_port = htons(VPN_CLIENT_PORT);

    printf("ProgUDP1 ready to send and receive messages\nTo send a message, just type it\n\n");

    // configuração do select
    fd_set readfds;
    int maxfd = (clientSocket > STDIN_FILENO ? clientSocket : STDIN_FILENO) + 1;
    char buf[BUFLEN];

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // teclado
        FD_SET(clientSocket, &readfds); // UDP

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            perror("erro na função select");
            break;
        }

        // Enviar texto escrito no terminal → vpn_client:9876
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buf, BUFLEN, stdin)) {
                size_t n = strlen(buf);
                // opcional: incluir '\0' — então usa n+1
                if (sendto(clientSocket, buf, n, 0, (struct sockaddr*)&vpn_cli, sizeof(vpn_cli)) < 0) {
                    perror("sendto");
                } else {
                    printf("Sent: %s", buf);
                    if (buf[n - 1] != '\n') printf("\n");
                    
                }
            }
        }

        // Receber mensagens (vindas do vpn_client)
        if (FD_ISSET(clientSocket, &readfds)) {
            struct sockaddr_in from; 
            socklen_t flen = sizeof(from);
            int n = recvfrom(clientSocket, buf, BUFLEN - 1, 0, (struct sockaddr*)&from, &flen);
            if (n > 0) {
                buf[n] = '\0';
                if (buf[n-1] != '\n') printf("\n");
                printf("Received [VPN Server]: %s", buf);
                fflush(stdout);
            }
        }
    }

    close(clientSocket);
    return 0;
}
