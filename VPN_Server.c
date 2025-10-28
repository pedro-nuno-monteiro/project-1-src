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
#include <math.h>

/*
Sentido 1: ProgUDP1  ----UDP---->  VPN Client  ----TCP---->  VPN Server ----UDP---->  ProgUDP2

        - ProgUDP1 envia para VPN Client:   9876
        - VPN Client envia para VPN Server 9000
        - VPN Server envia para ProgUDP2:   9878

Sentido 2: ProgUDP2  ----UDP---->  VPN Server  ----TCP---->  VPN Client ----UDP---->  ProgUDP1
        - ProgUDP2 envia para VPN Server:   9877
        - VPN Server envia para VPN Client: 9000
        - VPN Client envia para ProgUDP1:   9875
*/

/* inicializações */

#define SERVER_TCP_PORT   9001     // porto para recepção das mensagens TCP
#define SERVER_UDP_PORT   9877   // porto para recepção das mensagens UDP 
#define PROGUDP2_PORT     9878   // porto do ProgUDP2
#define BUFLEN            1024

struct DH_parameters {
    // valores públicos
    int p, g;

    // valores secretos
    int b;
    
    int A, B;

    int key;
};

struct Packet {
    char message[BUFLEN];
    unsigned int hash_value;
};

// variaveis globais para o process client
int udp_sock;
struct sockaddr_in prog_udp_2;
int metodo_global = 1; // default method

void process_client(int client_fd);
void erro(char *msg);
int manager_menu();
void clear_screen();
void dh_calcula_key(struct DH_parameters *dh);
void criptar(char *message, int key, int encriptar, int metodo);
unsigned int hash(const char * message);

int main(void) {

    //int metodo = 1; // default method
    // se nao receber nenhuma mensagem do manager, usa o default

	clear_screen();

    // depois o método virá do manager
	//int metodo = manager_menu();

    // udp_sock setup
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) erro("Erro na criação do socket UDP");

    struct sockaddr_in udp_bind_addr;
    bzero(&udp_bind_addr, sizeof(udp_bind_addr));
    udp_bind_addr.sin_family = AF_INET;
    udp_bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_bind_addr.sin_port = htons(SERVER_UDP_PORT);

    if (bind(udp_sock, (struct sockaddr*)&udp_bind_addr, sizeof(udp_bind_addr)) < 0)
        erro("Erro no bind UDP");

    bzero(&prog_udp_2, sizeof(prog_udp_2));
    prog_udp_2.sin_family = AF_INET;
    prog_udp_2.sin_port   = htons(PROGUDP2_PORT);
    prog_udp_2.sin_addr.s_addr = inet_addr("127.0.0.1");

    // tcp_sock setup
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

    while(1) {
        struct sockaddr_in client_addr; 
        socklen_t client_addr_size = sizeof(client_addr);

        int client = accept(tcp_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client < 0) { 
            erro("accept"); 
            continue; 
        }

        process_client(client);  
        close(client);
    }

    close(tcp_sock);
    close(udp_sock);
    return 0;
}

void process_client(int client_fd) {
    int nread = 0;
    char buffer[BUFLEN];

    struct DH_parameters dh;
    // --- protocolo Diffie-Hellman ---
    dh.p = 23;
    dh.g = 5;
    dh.b = 9;

    // cálculo de B
    dh.B = (int)pow(dh.g, dh.b) % dh.p;

    // envio de B para o VPN Client
    printf("Default process\n");
    // recebe A do VPN Client
    if (read(client_fd, &dh.A, sizeof(dh.A)) == -1) {
        erro("Erro ao receber A");
    }
    printf("Received A = %d from VPN Server\n", dh.A);

    printf("Sending B = %d to VPN Client\n", dh.B);
    if (write(client_fd, &dh.B, sizeof(dh.B)) == -1) {
        erro("Erro no envio de B");
    }

    // calcula chave secreta S
    dh_calcula_key(&dh);

    printf("VPN Server: waiting for TCP and UDP messages\n\n");
    
    while(1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);   // TCP from VPN client
        FD_SET(udp_sock, &readfds);    // UDP from ProgUDP2
        int maxfd = (client_fd > udp_sock ? client_fd : udp_sock) + 1;

		// espera por dados TCP ou UDP
        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            erro("select");
            break;
        }

        // TCP -> UDP (to ProgUDP2)
        if (FD_ISSET(client_fd, &readfds)) {

            struct Packet p;

            nread = read(client_fd, &p, sizeof(p));
            if (nread <= 0) 
				break;  // closed or error
            
            strncpy(buffer, p.message, BUFLEN - 1);
            buffer[BUFLEN - 1] = '\0';
            unsigned int received_hash = p.hash_value;

            printf("\nReceived [VPN Client]: %s\n", buffer);

            int metodo = metodo_global;

            printf("Using method: %d\n", metodo);

            // desencriptar mensagem + calcular hash
            unsigned int hash_value = hash(buffer);
            criptar(buffer, dh.key, 0, metodo);

            // verificar integridade
            if (hash_value != received_hash) {
                erro("Hash diferentes! Mensagem corrompida\n");
            } else {
                printf("Hash iguais! Mensagem íntegra.\n");
            }

			// enviar para ProgUDP2 via UDP
            if (sendto(udp_sock, buffer, nread, 0,(struct sockaddr*)&prog_udp_2, sizeof(prog_udp_2)) == -1) {
                erro("sendto UDP");
                break;
            }
            
        }

        // UDP -> TCP (back to VPN client)
        if (FD_ISSET(udp_sock, &readfds)) {

            struct sockaddr_in from; 
			socklen_t flen = sizeof(from);

            int n = recvfrom(udp_sock, buffer, BUFLEN, 0,(struct sockaddr*)&from, &flen);
            if (n < 0) { 
                erro("recvfrom UDP"); 
                break; 
            }

            buffer[n] = '\0';

            if (strncmp(buffer, "METODO: ", 8) == 0) {
                int metodo_enviado = atoi(buffer + 8);
                if (metodo_enviado == 1 || metodo_enviado == 2) {
                    metodo_global = metodo_enviado;
                    printf("\nMétodo atualizado via manager: METODO= %d\n", metodo_global);
                } else {
                    printf("\n[SERVER] METODO invalido: %s\n", buffer);
                }
                continue;
            }

            else{

                printf("\nReceived [ProgUDP2]: %s", buffer);

                int metodo = metodo_global;
                printf("Using method: %d\n", metodo);

                struct Packet pp;
                // encriptar mensagem + calcular hash
                criptar(buffer, dh.key, 1, metodo);
                unsigned int hash_value = hash(buffer);

                pp.hash_value = hash_value;
                strcpy(pp.message, buffer);

                // enviar de volta para o cliente VPN via TCP
                if (write(client_fd, &pp, sizeof(pp)) < 0) {
                    erro("write TCP"); 
                    break; 
                }
        }
        }
    }
}

int manager_menu() {

	int opcao = 0;

	printf("***************************************\n");
	printf("        VPN Server Manager             \n");
	printf(" 1. Utilizar encriptação por default   \n");
	printf(" 2. Configurar métodos criptográficos  \n");
	printf("***************************************\n\n");

    printf("Selecione uma opção: ");
    while (scanf("%d", &opcao) != 1 || opcao < 1 || opcao > 2) {
        printf("Opção inválida. Tente novamente\n\nSelecione uma opção: ");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        opcao = 0;
    }

    int metodo = 1;
	switch (opcao) {
        case 1:
            clear_screen();
            printf("Selecionou a encriptação por default = Generalised Caesar Cipher.\n");
            scanf("Pressione Enter para continuar."); 
            break;
        
        case 2:
            clear_screen();
            metodo = 0;
            printf("Selecione o método criptográfico:\n");
            printf("1. Generalised Caesar Cipher (default)\n");
            printf("2. Vigenère cypher\n");
            printf("Opção: ");
            while (scanf(" %d", &metodo) != 1 || metodo < 1 || metodo > 2) {
                printf("Opção inválida. Tente novamente\n\nSelecione uma opção: ");
                int d;
                while ((d = getchar()) != '\n' && d != EOF);
                metodo = 0;
            }
            break;
            
        default:
            break;
	}
    clear_screen();
    return metodo;  
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
    dh->key = (int)pow(dh->A, dh->b) % dh->p;

    printf("Chave secreta, K = %d\n\n", dh->key);
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