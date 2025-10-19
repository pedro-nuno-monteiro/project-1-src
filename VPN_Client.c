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

#define BUFLEN 512	// Tamanho do buffer
#define PORT 9876	  // Porto para recepção das mensagens

void erro(char *msg);

int main(int argc, char *argv[]) {

  #ifdef _WIN32
		system("cls");
	#else
		system("clear");
	#endif

  // inicializações
  char VPN_server[100];

  int udp_sock, tcp_sock;;
  
  // socket
  struct sockaddr_in prog_udp_1, vpn_server_tcp, vpn_client_udp;

	socklen_t slen = sizeof(prog_udp_1);
	char buf[BUFLEN];

  struct hostent *hostPtr;

  if (argc != 3) {
    printf("cliente <host> <port>\n");
    exit(-1);
  }

  /* Criar socket para conexão UDP com ProgUDP 1 */
  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		erro("Erro na criação do socket UDP");
	}
  vpn_client_udp.sin_family = AF_INET;
	vpn_client_udp.sin_port = htons(PORT);
	vpn_client_udp.sin_addr.s_addr = htonl(INADDR_ANY);

  // Associa o socket à informação de endereço
	if(bind(udp_sock, (struct sockaddr*)&vpn_client_udp, sizeof(vpn_client_udp)) == -1)
		erro("Erro no bind UDP");

  printf("VPN Client: waiting for UDP msg\n");

  /* Criar socket para conexão TCP com VPN Server */

  strcpy(VPN_server, argv[1]);
  if ((hostPtr = gethostbyname(VPN_server)) == 0) {
    erro("Não consegui obter endereço");
  }

  bzero((void *) &vpn_server_tcp, sizeof(vpn_server_tcp));
  vpn_server_tcp.sin_family = AF_INET;
  vpn_server_tcp.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  vpn_server_tcp.sin_port = htons((short) atoi(argv[2]));


  if((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    erro("socket");
  }

  if(connect(tcp_sock,(struct sockaddr *)&vpn_server_tcp,sizeof (vpn_server_tcp)) < 0) {
    erro("Connect");
  }

  printf("VPN Client: connected to VPN Server(%s:%s)\n---\n", argv[1], argv[2]);

  // Esperar por UDP ou TCP

  fd_set readfds;
  int maxfd;
  maxfd = (udp_sock > tcp_sock ? udp_sock : tcp_sock) + 1;

  while (1) {

    FD_ZERO(&readfds);
    FD_SET(udp_sock, &readfds);
    FD_SET(tcp_sock, &readfds);

    // verifica que tipo de mensagem é que recebeu

    if (select(maxfd, &readfds, NULL, NULL, NULL) < 0)
      erro("Erro no select");

    // caso mensagem UDP
    if (FD_ISSET(udp_sock, &readfds)) {
      
      // Espera recepção de mensagem (a chamada é bloqueante)
      int recv_len = recvfrom(udp_sock, buf, BUFLEN, 0, (struct sockaddr *) &prog_udp_1, (socklen_t *)&slen);

      buf[recv_len] = '\0';

      printf("Mensagem de ProgUDP 1 (%s:%d)\n", inet_ntoa(prog_udp_1.sin_addr), ntohs(prog_udp_1.sin_port));
      printf("Mensagem: %s\n" , buf);

      // enviar mensagem para VPN Server
      if (write(tcp_sock, buf, 1 + strlen(buf)) == -1) {
        erro("Write to VPN Server");
      }
      else {
        printf("Mensagem reencaminhada para VPN Server via TCP\n---\n");
      }
    }

    // caso mensagem TCP
    if (FD_ISSET(tcp_sock, &readfds)) {

      int recv_len = read(tcp_sock, buf, BUFLEN - 1);
      if (recv_len <= 0) {
        erro("VPN Server closed");
      }

      buf[recv_len] = '\0';
      printf("Recebi via TCP → reenviando via UDP\n");

      // reenviar mensagem para ProgUDP 1
      if (sendto(udp_sock, buf, 1 + strlen(buf), 0, (struct sockaddr *)&prog_udp_1, slen) == -1) {
        erro("sendto");
      }
    }
  }

  close(tcp_sock);
  close(udp_sock);
  exit(0);
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}

