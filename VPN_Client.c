// vpn_client.c — versão final (localhost), UDP1 com bind 9875
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define BUFLEN                1024
#define UDP1_PORT             9875   // porto fixo do Prog_UDP1 (bind)
#define VPN_CLIENT_UDP_PORT   9876   // porto deste cliente VPN (bind)

void erro(char *msg);

int main(int argc, char *argv[]) {

    // limpar ecrã
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    if (argc != 3) {
        printf("cliente <host> <port>\n");
        exit(-1);
    }

    // inicializações
    int udp_sock, tcp_sock;
    char buf[BUFLEN];

    // UDP: bind deste cliente em 9876 (para receber do ProgUDP1)
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        erro("Erro na criação do socket UDP");

    struct sockaddr_in vpn_client_udp;
    memset(&vpn_client_udp, 0, sizeof(vpn_client_udp));
    vpn_client_udp.sin_family      = AF_INET;
    vpn_client_udp.sin_port        = htons(VPN_CLIENT_UDP_PORT);
    vpn_client_udp.sin_addr.s_addr = htonl(INADDR_ANY);

    // Associa o socket à informação de endereço
    if (bind(udp_sock, (struct sockaddr*)&vpn_client_udp, sizeof(vpn_client_udp)) == -1)
        erro("Erro no bind UDP");

    // TCP: conectar ao VPN Server
    char VPN_server[100];
    strcpy(VPN_server, argv[1]);

    struct hostent *hostPtr;
    if ((hostPtr = gethostbyname(VPN_server)) == 0)
        erro("Não consegui obter endereço");

    struct sockaddr_in vpn_server_tcp;
    memset(&vpn_server_tcp, 0, sizeof(vpn_server_tcp));
    vpn_server_tcp.sin_family = AF_INET;
    vpn_server_tcp.sin_addr.s_addr =((struct in_addr *)(hostPtr->h_addr))->s_addr;
    vpn_server_tcp.sin_port = htons((short)atoi(argv[2]));

    if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        erro("socket TCP");

    if (connect(tcp_sock, (struct sockaddr *)&vpn_server_tcp, sizeof(vpn_server_tcp)) < 0)
        erro("Connect TCP");

    // endereço FIXO do UDP1 (agora com bind em 9875) ---
    struct sockaddr_in udp1_fixed;
    memset(&udp1_fixed, 0, sizeof(udp1_fixed));
    udp1_fixed.sin_family = AF_INET;
    udp1_fixed.sin_addr.s_addr = inet_addr("127.0.0.1");
    udp1_fixed.sin_port = htons(UDP1_PORT);

    // configuração do select
    fd_set readfds;
    int maxfd = (udp_sock > tcp_sock ? udp_sock : tcp_sock) + 1;

    printf("VPN Client: waiting for TCP and UDP messages\n\n");

    while(1) {

        FD_ZERO(&readfds);
        FD_SET(udp_sock, &readfds); // de ProgUDP1 para cá (UDP)
        FD_SET(tcp_sock, &readfds); // do VPN Server para cá (TCP)

        // verifica que tipo de mensagem é que recebeu
        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0)
            erro("Erro no select");

        // --- caso mensagem UDP (ProgUDP1 -> vpn_client) ---
        if (FD_ISSET(udp_sock, &readfds)) {

            struct sockaddr_in prog_udp_1; // apenas para log
            socklen_t slen = sizeof(prog_udp_1);

            int recv_len = recvfrom(udp_sock, buf, BUFLEN - 1, 0, (struct sockaddr *) &prog_udp_1, &slen);
            if (recv_len < 0){
                erro("recvfrom UDP");
            }

            buf[recv_len] = '\0';

            printf("Received [ProgUDP1]: %s", buf);
            
            // enviar para VPN Server via TCP
            if (write(tcp_sock, buf, recv_len) == -1) {
                erro("Write to VPN Server");
            }
        }

        // caso mensagem TCP
        if (FD_ISSET(tcp_sock, &readfds)) {

            int recv_len = read(tcp_sock, buf, BUFLEN - 1);
            if (recv_len <= 0){
                erro("VPN Server closed");
            }

            buf[recv_len] = '\0';
            printf("Received [VPN Server]: %s", buf);

            // reenviar para ProgUDP1 via UDP
            if (sendto(udp_sock, buf, recv_len, 0, (struct sockaddr *)&udp1_fixed, sizeof(udp1_fixed)) == -1) {
                erro("sendto UDP1");
            }
        }
    }

    close(tcp_sock);
    close(udp_sock);
    return 0;
}

void erro(char *msg) {
    printf("Erro: %s\n", msg);
    exit(-1);
}
