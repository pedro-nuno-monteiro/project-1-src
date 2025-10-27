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
#include <math.h>

#define BUFLEN                1024
#define UDP1_PORT             9875   // porto fixo do Prog_UDP1 (bind)
#define VPN_CLIENT_UDP_PORT   9876   // porto deste cliente VPN (bind)

struct DH_parameters {
    // valores públicos
    int p, g;

    // valores secretos
    int a;
    
    int A, B;

    int key;
};

struct Packet {
    char message[BUFLEN];
    unsigned int hash_value;
};

void erro(char *msg);
void clear_screen();
void dh_calcula_key(struct DH_parameters *dh);
void criptar(char * message, int key, int encriptar, int metodo);
unsigned int hash(const char * message);

int main(int argc, char *argv[]) {

    clear_screen();

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

    // --- protocolo Diffie-Hellman ---
    struct DH_parameters dh;
    dh.p = 23;
    dh.g = 5;
    dh.a = 6;

    // cálculo de A
    dh.A = (int)pow(dh.g, dh.a) % dh.p;

    // envio de A para o VPN Server
    printf("Default process\n");
    printf("Sending A = %d to VPN Server\n", dh.A);
    if (write(tcp_sock, &dh.A, sizeof(dh.A)) == -1) {
        erro("Erro no envio de A");
    }

    // recebe B do VPN Server
    if (read(tcp_sock, &dh.B, sizeof(dh.B)) == -1) {
        erro("Erro ao receber B");
    }
    printf("Received B = %d from VPN Server\n", dh.B);

    // calcula chave secreta S
    dh_calcula_key(&dh);

    clear_screen();

    printf("VPN Client: waiting for TCP and UDP messages\n\n");

    // assumir método = 2
    int metodo = 1;

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
            if (recv_len < 0) {
                erro("recvfrom UDP");
            }
            
            buf[recv_len] = '\0';
            
            printf("\nReceived [ProgUDP1]: %s", buf);
            
            struct Packet p;
            // encriptar mensagem + calcular hash
            criptar(buf, dh.key, 1, metodo);
            unsigned int hash_value = hash(buf);

            strcpy(p.message, buf);
            p.hash_value = hash_value;

            // enviar para VPN Server via TCP
            if (write(tcp_sock, &p, sizeof(p)) == -1) {
                erro("Write to VPN Server");
            }
        }

        // caso mensagem TCP
        if (FD_ISSET(tcp_sock, &readfds)) {

            struct Packet pp;
            int recv_len = read(tcp_sock, &pp, sizeof(pp));
            if (recv_len <= 0) {
                erro("VPN Server closed");
            } 

            unsigned int received_hash = pp.hash_value;
            strncpy(buf, pp.message, BUFLEN - 1);
            buf[BUFLEN - 1] = '\0';

            printf("\nReceived [VPN Server]: %s", buf);

            // desencriptar mensagem + calcular hash
            unsigned int hash_value = hash(buf);
            criptar(buf, dh.key, 0, metodo);
            
            // verificar integridade
            if (hash_value != received_hash) {
                erro("Hash diferentes! Mensagem corrompida\n");
            } else {
                printf("Hash iguais! Mensagem íntegra.\n");
            }

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

void clear_screen() {
	// limpar ecrã
	#ifdef _WIN32
		system("cls");
	#else
		system("clear");
	#endif
}

void dh_calcula_key(struct DH_parameters *dh) {
    // key = B^a mod p
    dh->key = (int)pow(dh->B, dh->a) % dh->p;

    printf("Chave secreta, K = %d\n\n", dh->key);
    printf("Press Enter para continuar!");
    getchar();
    return;
}

void criptar(char *message, int key, int encriptar, int metodo) {

    switch (metodo) {
    case 1:
        // Generalised Caesar Cipher
        if (encriptar) {
            //encriptar
            for(int i = 0; message[i] != '\0'; i++) {
                message[i] = (message[i] + key) % 256;
            }

        } else {
            //desencriptar
            for(int i = 0; message[i] != '\0'; i++) {
                message[i] = (message[i] - key + 256) % 256;
            }
        }
        break;
    
    case 2:

        // Vigenère cypher
        // gerar key_str a partir de key
        char key_str[32];
        unsigned int seed = key;
        for(int i = 0; i < sizeof(key_str); i++) {
            seed = (214013 * seed + 2531011);
            key_str[i] = (seed >> 16) & 0xFF;
        }

        for(int i = 0; message[i] != '\0'; i++) {
            if (encriptar) {
                //encriptar
                message[i] = (message[i] + key_str[i % sizeof(key_str)]) % 256;
            } else {
                //desencriptar
                message[i] = (message[i] - key_str[i % sizeof(key_str)] + 256) % 256;
            }
        }
    }

    printf("After %scryption: %s\n", encriptar ? "en" : "de", message);
}

unsigned int hash(const char * message) {
    unsigned int hash_value = 0;
    const unsigned char * msg = (const unsigned char *)message;
    while (* msg) {
        hash_value = (hash_value * 31) + * msg;
        msg++;
    }
    printf("Hash value: %u\n", hash_value);
    return hash_value;
}
