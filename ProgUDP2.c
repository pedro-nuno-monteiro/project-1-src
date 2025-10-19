#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define PORT 9878
#define VPN_SERVER_UDP_PORT 9877
#define BUFLEN 1024

int main() {	

	#ifdef _WIN32
		system("cls");
	#else
		system("clear");	
	#endif

	int clientSocket;
	char buffer[BUFLEN];
	struct sockaddr_in serverAddr, senderAddr, vpn_server_addr;
  socklen_t addr_size = sizeof(senderAddr);
	
	/*Create UDP socket*/
	clientSocket = socket(PF_INET, SOCK_DGRAM, 0);
	
	/*Configure settings in address struct*/
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	/*Configure VPN Server address struct*/
	bzero(&vpn_server_addr, sizeof(vpn_server_addr));
	vpn_server_addr.sin_family = AF_INET;
	vpn_server_addr.sin_port = htons(VPN_SERVER_UDP_PORT); // porta do VPN Server UDP
	vpn_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	// Associa o socket à informação de endereço
	if(bind(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
		perror(	"Erro no bind UDP");

	printf("ProgUDP2 ready to send and receive messages\n");

	fd_set readfds;
	int maxfd = clientSocket > STDIN_FILENO ? clientSocket : STDIN_FILENO;

	while(1) {

		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds); // user input
		FD_SET(clientSocket, &readfds);	// UDP incoming

		if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
			perror("select");
			exit(1);
		}

		// enviar mensagem
		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			if (fgets(buffer, BUFLEN, stdin) != NULL) {
				int nBytes = strlen(buffer) + 1;
				sendto(clientSocket, buffer, nBytes, 0, (struct sockaddr *)&vpn_server_addr, sizeof(vpn_server_addr));
				printf("Sent: %s", buffer);
			}
		}
		
		// receber mensagem UDP
		if (FD_ISSET(clientSocket, &readfds)) {
			int nBytes = recvfrom(clientSocket, buffer, BUFLEN - 1, 0, (struct sockaddr *)&senderAddr, &addr_size);
			if (nBytes > 0) {
				buffer[nBytes] = '\0';
				printf("Received from %s:%d → %s", inet_ntoa(senderAddr.sin_addr), ntohs(senderAddr.sin_port), buffer);
			}
		}
	}
	return 0;
}
