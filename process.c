#include "p2p.h"

int main(int argc, char *argv[]) {
    fprintf(stderr, "inside process.c main\n");

  	int temp_sockfd;
  	int s2s_sockfd;
  	struct sockaddr_in addr_p;
  	struct sockaddr_in addr_s;
  	uint16_t pid;
  
  	/*----- Checking arguments -----*/
	if (argc != 4) {
		fprintf(stderr, "usage: ./process <pid> <n> <port>\n");
		exit(-1);
	}
  	
  	pid = (uint16_t)atoi(argv[1]);
  
    /*----- Opening the proxy-to-server socket TCP -----*/
    if ((temp_sockfd = p2p_socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("[p2p_socket]: proxy-to-server");
		exit(-1);
	}
        
    /*----- Setting addr_p info -----*/
	memset(&addr_p, 0, sizeof(struct sockaddr_in));
	addr_p.sin_family      = AF_INET;
	addr_p.sin_addr.s_addr = htonl(INADDR_ANY);		
	addr_p.sin_port        = htons(atoi(argv[3]));        
        
    /*----- Bind proxy-to-server port -----*/
	if (p2p_bind(temp_sockfd, (struct sockaddr *)&addr_p, sizeof(struct sockaddr_in)) == -1) {
		perror("[p2p_bind]: proxy-to-server");
		exit(-1);
	}

    /*----- Listen for proxy connection -----*/
    p2p_listen(temp_sockfd, 1);

  	/*----- Accept proxy connection -----*/
  	if (p2p_accept(temp_sockfd) == -1) {
		perror("[p2p_accept]");
		exit(-1);
	}
  
  	fprintf(stderr, "accepted\n");
       
	/*----- Opening the server-to-server socket UDP -----*/
	if ((s2s_sockfd = p2p_socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("[p2p_socket]: server-to-server");
		exit(-1);
	}
  
    /*----- Setting addr_s info -----*/
	memset(&addr_s, 0, sizeof(struct sockaddr_in));
	addr_s.sin_family      = AF_INET;
	addr_s.sin_addr.s_addr = htonl(INADDR_ANY);		
	addr_s.sin_port        = htons(ROOTID + pid); 
    

    /*----- Bind server-to-server port -----*/
	if (p2p_bind(s2s_sockfd, (struct sockaddr *)&addr_s, sizeof(struct sockaddr_in)) == -1) {
		perror("[p2p_bind]: server-to-server");
		exit(-1);
	}
        
	/*----- Listening to new connections -----*/
	p2p_run(pid);

	return(0);
}