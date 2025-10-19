#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

/* inicializações */

#define SERVER_TCP_PORT   9000   // porto para recepção das mensagens TCP
#define SERVER_UDP_PORT   9877   // porto para recepção das mensagens UDP 
#define PROGUDP2_PORT     9878   // porto do ProgUDP2
#define BUFLEN            1024

// variaveis globais para o process client
int udp_sock;
struct sockaddr_in prog_udp_2;

void process_client(int client_fd);
void erro(char *msg);

int main(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif

    // udo_sock setup
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) erro("Erro na criação do socket UDP");

    struct sockaddr_in udp_bind_addr;
    bzero(&udp_bind_addr, sizeof(udp_bind_addr));
    udp_bind_addr.sin_family = AF_INET;
    udp_bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_bind_addr.sin_port = htons(SERVER_UDP_PORT);

    if (bind(udp_sock, (struct sockaddr*)&udp_bind_addr, sizeof(udp_bind_addr)) < 0)
        erro("Erro no bind UDP");

    // destination for UDP: ProgUDP2 on localhost:SERVER_UDP_PORT
    bzero(&prog_udp_2, sizeof(prog_udp_2));
    prog_udp_2.sin_family = AF_INET;
    prog_udp_2.sin_port   = htons(PROGUDP2_PORT);
    prog_udp_2.sin_addr.s_addr = inet_addr("127.0.0.1");

    // ----- TCP listen socket -----
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0){
        erro("na função socket TCP");
    }


    struct sockaddr_in udp_server_addr;
    bzero(&udp_server_addr, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_server_addr.sin_port = htons(SERVER_TCP_PORT);

    if (bind(tcp_sock, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr)) < 0)
        erro("na função bind TCP");
    if (listen(tcp_sock, 1) < 0)
        erro("na função listen TCP");

    printf("VPN Server: waiting for TCP and UDP messages\n");

    // ----- accept loop: one client at a time -----
    while(1) {
        struct sockaddr_in client_addr; 
        socklen_t client_addr_size = sizeof(client_addr);

        int client = accept(tcp_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client < 0) { 
            perror("accept"); 
            continue; 
        }

        process_client(client);   // handles both directions with select()
        close(client);
    }

    // (unreachable in this minimal loop)
    close(tcp_sock);
    close(udp_sock);
    return 0;
}


void process_client(int client_fd) {
    int nread = 0;
    char buffer[BUFLEN];

    while(1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);   // TCP from VPN client
        FD_SET(udp_sock, &readfds);    // UDP from ProgUDP2
        int maxfd = (client_fd > udp_sock ? client_fd : udp_sock) + 1;

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // TCP -> UDP (to ProgUDP2)
        if (FD_ISSET(client_fd, &readfds)) {
            nread = read(client_fd, buffer, BUFLEN);
            printf("Received: %s", buffer);
            if (nread <= 0) break;  // closed or error
            if (sendto(udp_sock, buffer, nread, 0,(struct sockaddr*)&prog_udp_2, sizeof(prog_udp_2)) == -1) {
                perror("sendto UDP");
                break;
            }
            
        }

        // UDP -> TCP (back to VPN client)
        if (FD_ISSET(udp_sock, &readfds)) {
            struct sockaddr_in from; socklen_t flen = sizeof(from);
            int n = recvfrom(udp_sock, buffer, BUFLEN, 0,(struct sockaddr*)&from, &flen);
            printf("Received: %s", buffer);
            if (n < 0) { 
                perror("recvfrom UDP"); 
                break; 
            }
            
            if (write(client_fd, buffer, n) < 0) {
                perror("write TCP"); 
                break; 
            }
        }
    }

}

void erro(char *msg) {
    printf("Erro: %s\n", msg);
    exit(-1);
}
