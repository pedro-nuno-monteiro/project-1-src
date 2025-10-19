#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define SERVER_PORT       9000 // porto para recepção das mensagens TCP
#define SERVER_UDP_PORT   9877 // porto para recepção das mensagens UDP 
#define PROGUDP2_PORT     9878 // porto do ProgUDP2
#define BUFLEN	          1024

struct sockaddr_in prog_udp_2; 
int client_fd_global = -1;
int udp_sock;

void process_client(int fd);
void erro(char *msg);

int main() {

  #ifdef _WIN32
		system("cls");
	#else
		system("clear");
	#endif

  // inicializações
  int tcp_sock_id, client;

  // socket
  struct sockaddr_in vpn_client_tcp, client_addr;
  socklen_t client_addr_size = sizeof(client_addr);
  char buf[BUFLEN];

  bzero((void *) &vpn_client_tcp, sizeof(vpn_client_tcp));
  vpn_client_tcp.sin_family = AF_INET;
  vpn_client_tcp.sin_addr.s_addr = htonl(INADDR_ANY);
  vpn_client_tcp.sin_port = htons(SERVER_PORT);

  // criar socket para conexão TCP com VPN Client
  if ((tcp_sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    erro("na funcao socket");
  if(bind(tcp_sock_id, (struct sockaddr*)&vpn_client_tcp, sizeof(vpn_client_tcp)) == -1)
    erro("na funcao bind");
  if(listen(tcp_sock_id, 5) == -1)
    erro("na funcao listen");

  printf("VPN Server: waiting for TCP msg\n");

  // udp socket
  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		erro("Erro na criação do socket UDP");

  struct sockaddr_in udp_server_addr;
  bzero(&udp_server_addr, sizeof(udp_server_addr));
  udp_server_addr.sin_family = AF_INET;
  udp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  udp_server_addr.sin_port = htons(SERVER_UDP_PORT);

  if(bind(udp_sock, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr)) == -1)
    erro("Erro no bind UDP");


  printf("VPN Server: waiting for UDP msg\n");

  bzero(&prog_udp_2, sizeof(prog_udp_2));
  prog_udp_2.sin_family = AF_INET;
  prog_udp_2.sin_port = htons(PROGUDP2_PORT);
  prog_udp_2.sin_addr.s_addr = inet_addr("127.0.0.1"); // ProgUDP2 address


  fd_set readfds;
  int maxfd;
  maxfd = (udp_sock > tcp_sock_id ? udp_sock : tcp_sock_id) + 1;

  // Loop que espera por conexões TCP ou UDP
  while (1) {
  
    //clean finished child processes, avoiding zombies
    //must use WNOHANG or would block whenever a child process was working
    while(waitpid(-1, NULL, WNOHANG) > 0);

    FD_ZERO(&readfds);
    FD_SET(udp_sock, &readfds);
    FD_SET(tcp_sock_id, &readfds);

    // verifica que tipo de mensagem é que recebeu
    if (select(maxfd, &readfds, NULL, NULL, NULL) < 0)
      erro("Erro no select");

    // caso mensagem TCP
    if (FD_ISSET(tcp_sock_id, &readfds)) {

      // accept()
      client = accept(tcp_sock_id, (struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);

      if (client > 0) {
        if (fork() == 0) {
          client_fd_global = client;
          close(tcp_sock_id);
          process_client(client);
          exit(0);
        }
        close(client);
      }
    }

    // caso mensagem UDP
    if (FD_ISSET(udp_sock, &readfds)) {
      printf("aqui dentro\n");
      struct sockaddr_in udp_sender;
      socklen_t addr_size = sizeof(udp_sender);

      int recv_len = recvfrom(udp_sock, buf, BUFLEN - 1, 0, (struct sockaddr *) &udp_sender, (socklen_t *)&addr_size);

      if (recv_len > 0) {
        buf[recv_len] = '\0';
        printf("Received UDP message from %s:%d → %s\n", inet_ntoa(udp_sender.sin_addr), ntohs(udp_sender.sin_port), buf);

      
        if (client_fd_global != -1) {
          write(client_fd_global, buf, recv_len);
          printf("Forwarded UDP -> TCP to VPN Client.\n");
        } else {
          printf("No VPN Client connected. Discarding UDP message.\n");
        }
      }
    }
  }
  close(tcp_sock_id);
  close(udp_sock);
  return 0;
}

void process_client(int client_fd) {


	int nread = 0;
	char buffer[BUFLEN];

  printf("New client connected\n");

  //client_fd_global = client_fd;

  while (1) {

    while ((nread = read(client_fd, buffer, BUFLEN - 1)) > 0) {
      buffer[nread] = '\0';
      printf("TCP from VPN Client: %s\n", buffer);

      // Forward to ProgUDP2 via UDP
      if (sendto(udp_sock, buffer, nread, 0,
        (struct sockaddr *)&prog_udp_2, sizeof(prog_udp_2)) == -1) {
        perror("sendto UDP to ProgUDP2");
      } else {
        printf("Forwarded TCP -> UDP to ProgUDP2.\n");
      }
    }
  }
  printf("VPN Client disconnected.\n");
	close(client_fd);
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}
