#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define SERVER_UDP_PORT 9877   // porto do servidor VPN (destino)
#define LOCAL_UDP_PORT  9878   // porto do Prog_UDP2 (bind)
#define BUFLEN          1024

int main(void) {

    // limpar ecrã
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    // criação socket UDP
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0)
        perror("erro na criação de socket");

    // fazer bind do Prog_UDP2 em 127.0.0.1:9877
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);   
    serverAddr.sin_port = htons(LOCAL_UDP_PORT);

    // associa o socket à informação de endereço
    if (bind(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        perror("bind");

    // envio de mensagens para destino: Servidor VPN em 127.0.0.1:9876
    struct sockaddr_in vpn_server_addr;
    memset(&vpn_server_addr, 0, sizeof(vpn_server_addr));
    vpn_server_addr.sin_family = AF_INET;
    vpn_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    vpn_server_addr.sin_port = htons(SERVER_UDP_PORT);

    printf("ProgUDP2 ready to send and receive messages\nTo send a message, just type it\n\n");

    // configuração do select
    fd_set readfds;
    int maxfd = (clientSocket > STDIN_FILENO ? clientSocket : STDIN_FILENO) + 1;
    char buf[BUFLEN];

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // teclado
        FD_SET(clientSocket, &readfds); // UDP

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            perror("erro no select");
            break;
        }

        // enviar mensagem
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buf, BUFLEN, stdin)) {
                size_t n = strlen(buf) + 1; // incluir '\0' como no seu exemplo
                if (sendto(clientSocket, buf, n, 0, (struct sockaddr*)&vpn_server_addr, sizeof(vpn_server_addr)) < 0) {
                    perror("sendto");
                } else {
                    printf("Sent: %s", buf);
                    if (buf[n - 1] != '\n') printf("\n");
                    fflush(stdout);
                }
            }
        }

        // receber mensagem UDP
        if (FD_ISSET(clientSocket, &readfds)) {
            struct sockaddr_in from; 
            socklen_t flen = sizeof(from);
            int n = recvfrom(clientSocket, buf, BUFLEN - 1, 0,(struct sockaddr*)&from, &flen);
            if (n > 0) {
                buf[n] = '\0';
                printf("Received [VPN Server]: %s", buf);
            }
        }
    }

    close(clientSocket);
    return 0;
}
