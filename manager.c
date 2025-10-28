#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define VPN_CLIENT_UDP_PORT  9876   // porto onde o vpn_client recebe por UDP
#define VPN_SERVER_UDP_PORT  9877   // porto onde o vpn_server recebe por UDP 
#define BUFLEN               1024

int manager_menu();
static void clear_screen(void);
void erro(char *msg);


int main(){

    clear_screen();

    int udpsock = socket(AF_INET, SOCK_DGRAM, 0);

    if (udpsock < 0) { 
        erro("socket"); 
        return 1; 
    }

    struct sockaddr_in vpn_client, vpn_server;

    // criar ligação com vpn_client
    memset(&vpn_client, 0, sizeof(vpn_client));
    vpn_client.sin_family = AF_INET;
    vpn_client.sin_addr.s_addr = inet_addr("127.0.0.1");
    vpn_client.sin_port = htons(VPN_CLIENT_UDP_PORT);

    // criar ligação com vpn_server
    memset(&vpn_server, 0, sizeof(vpn_server));
    vpn_server.sin_family = AF_INET;
    vpn_server.sin_addr.s_addr = inet_addr("127.0.0.1");
    vpn_server.sin_port = htons(VPN_SERVER_UDP_PORT);

    int opcao;

    while(1){

        opcao = manager_menu();

        char msg[BUFLEN];

        // criar mensagem para enviar
        snprintf(msg, BUFLEN, "METODO: %d\n", opcao);

        clear_screen();

        ssize_t msg_vpn_client = sendto(udpsock, msg, (int)strlen(msg), 0, (struct sockaddr*)&vpn_client, sizeof(vpn_client));

        if (msg_vpn_client < 0){
            erro("Não foi possível enviar para o vpn_client");
        }

        else {
            printf("Enviado ao VPN_Client : %s", msg);
        }

        ssize_t msg_vpn_server = sendto(udpsock, msg, (int)strlen(msg), 0, (struct sockaddr*)&vpn_server, sizeof(vpn_server));
        if (msg_vpn_server < 0) {
            erro("Não foi possível enviar para o vpn_server");
        }
        else {
            printf("Enviado ao VPN_Server : %s", msg);
        }

    }

    return 0;
}


int manager_menu() {

	int opcao = 0;

	printf("***************************************\n");
	printf("            VPN Server Manager         \n");
	printf(" 1. Generalised Caesar Cipher          \n");
	printf(" 2. Vigenère cypher                    \n");
	printf("***************************************\n\n");

    printf("Selecione uma opção: ");
    while (scanf("%d", &opcao) != 1 || opcao < 1 || opcao > 2) {
        printf("Opção inválida. Tente novamente\n\nSelecione uma opção: ");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        opcao = 0;
    }

    return opcao;
}

static void clear_screen(void){
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void erro(char *msg) {
    printf("Erro: %s\n", msg);
    exit(-1);
}