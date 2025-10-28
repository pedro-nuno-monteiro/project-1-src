all:
	gcc manager.c -o manager -lm
	gcc VPN_Client.c -o VPN_Client -lm
	gcc VPN_Server.c -o VPN_Server -lm
	gcc ProgUDP1.c -o ProgUDP1
	gcc ProgUDP2.c -o ProgUDP2
