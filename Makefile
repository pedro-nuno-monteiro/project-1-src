all:
	gcc VPN_Client.c -o VPN_Client
	gcc VPN_Server.c -o VPN_Server
	gcc ProgUDP1.c -o ProgUDP1
	gcc ProgUDP2.c -o ProgUDP2
	gcc udp_server.c -o udp_server
