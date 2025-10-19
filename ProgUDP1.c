#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define UDP_PORT 9876
#define BUFLEN 1024

int main() {

	#ifdef _WIN32
		system("cls");
	#else
		system("clear");	
	#endif

	int clientSocket;
	char buffer[BUFLEN];
	struct sockaddr_in serverAddr, senderAddr;
  socklen_t addr_size = sizeof(senderAddr);
	
	/*Create UDP socket*/
	clientSocket = socket(PF_INET, SOCK_DGRAM, 0);
	
	/*Configure settings in address struct*/
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(UDP_PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	printf("ProgUDP1 ready to send and receive messages\n");

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
				sendto(clientSocket, buffer, nBytes, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
				printf("\nSent: %s", buffer);
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
