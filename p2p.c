#include "p2p.h"

state_t s;

int p2p_socket(int domain, int type, int protocol) {
  	if (type == SOCK_STREAM) {
      	s.temp_sockfd = socket(domain, type, protocol);
      	return s.temp_sockfd;
    }
  	if (type == SOCK_DGRAM) {
		s.s2s_sockfd = socket(domain, type, protocol);
  		return s.s2s_sockfd;
    }
    return -1;
}

int p2p_bind(int sockfd, const struct sockaddr *addr, socklen_t socklen) {
    return bind(sockfd, addr, socklen);
}

int p2p_listen(int sockfd, int backlog) {
    return listen(sockfd, backlog);
}

int p2p_accept(int sockfd) {
  	socklen_t proxy_len = sizeof(struct sockaddr_in);
  	s.p2s_sockfd = accept(sockfd, (struct sockaddr*)s.proxy_addr, &proxy_len);
	/*fcntl(s.p2s_sockfd, F_SETFL, O_NONBLOCK);*/
    return s.p2s_sockfd;
}

void anti_entropy() {
	fprintf(stderr, "Server %d: Anti-entropy triggered\n", s.pid);
	/* Randomly pick a neighbor */
	int neighbor_port;
	srand(time(0));
	if (rand() > 0.5 * RAND_MAX) {
		neighbor_port = s.pid + ROOTID + 1;
	}
	else {
		neighbor_port = s.pid + ROOTID - 1;
	}
	fprintf(stderr, "47: random neighbor I chose is %d\n", neighbor_port);
	struct sockaddr* neighbor = NULL;
	/**/
	struct hostent *he;
	if ((he = gethostbyname("localhost")) == NULL){
		perror("gethostbyname");
		exit(-1);
	}
	struct sockaddr_in temp;

	memset(&temp, 0, sizeof(struct sockaddr_in));
	temp.sin_family = AF_INET;
	temp.sin_addr   = *(struct in_addr *)he->h_addr;
	temp.sin_port   = htons(neighbor_port);
	neighbor = (struct sockaddr*)(&temp);
	/**/

	/*build_sockaddr("localhost", neighbor_port, &neighbor);*/

	alarm(TIMEOUT);
	p2p_send_status(neighbor);
}

void p2p_run(uint16_t pid) {
  	s.pid = pid;
	int i;
	for (i = 0; i < NUM_SERVERS; i++) {
		s.vector_clock[i] = -1;
	}

	/* Set both socksets to be non-blocking */
	/* https://lasr.cs.ucla.edu/vahab/resources/signals.html */
	sigset_t newMask, oldMask;
	sigemptyset(&newMask);
	sigemptyset(&oldMask);
	
	signal(SIGALRM, anti_entropy);
	alarm(TIMEOUT);
	
  	s.pfds[0].fd = s.p2s_sockfd;
  	s.pfds[0].events = POLLIN;
  	s.pfds[1].fd = s.s2s_sockfd;
	s.pfds[1].events = POLLIN;

	while (1) {
		sigaddset(&newMask, SIGALRM);
		sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    	int poll_count = poll(s.pfds, NUM_FD, 0);
		sigprocmask(SIG_SETMASK, &oldMask, NULL);
    	if (poll_count == -1) {
        	perror("poll");
            exit(1);
        }
        int i;
      	for (i = 0; i < NUM_FD; i++) {
          	if (s.pfds[i].revents & POLLIN) {
				/*fprintf(stderr, "i = %d, fd = %d\n", i, s.pfds[i].fd);*/
           		if (s.pfds[i].fd == s.p2s_sockfd) {
                    fprintf(stderr, "message from proxy\n");
                	p2p_recv_proxy();
                }
                else if (s.pfds[i].fd == s.s2s_sockfd) {
					fprintf(stderr, "message from other server\n");
                  	p2p_recv_server();
                }
                else {
              		perror("Unrecognised connection request");
                    exit(-1);
                }  
            }
        }
	}
}

void p2p_recv_proxy() {
	char recv_buf[sizeof(msghdr)];
  	ssize_t bytes_read = recv(s.p2s_sockfd, recv_buf, sizeof(msghdr), 0);
	/*fprintf(stderr, "[p2p_recv_proxy]: %d bytesread %lu\n", s.p2s_sockfd,bytes_read);*/
	if (bytes_read == 0) {
		return;
	}
  	char* cmd;
  
  	cmd = strtok(recv_buf, " ");
  	if (strcmp(cmd, "msg") == 0) {
        fprintf(stderr, "command == msg\n");
   		uint16_t messageID = atoi(strtok(NULL, " "));
      	char* msg = strtok(NULL, "\n");
		fprintf(stderr, "parsing msg: !!!%s!!!\n", msg);
		int msg_len = bytes_read - 1 - num_of_digits(messageID) - 3 - 2; /* 1 for \0, 3 for msg, 2 spaces */
        /* Update chatlog and vector clock (corresponding to each other)*/
    	p2p_add_msg(s.pid, messageID, msg, msg_len);
		/* Randomly pick a neighbor */
		int neighbor_port;
		srand(time(0));
		if (rand() > 0.5 * RAND_MAX) {
			neighbor_port = s.pid + ROOTID + 1;
		}
		else {
			neighbor_port = s.pid + ROOTID - 1;
		}	
		fprintf(stderr, "137: random neighbor I chose is %d\n", neighbor_port);
		struct sockaddr* neighbor = NULL; 
		/**/
		struct hostent *he;
		if ((he = gethostbyname("localhost")) == NULL){
			perror("gethostbyname");
			exit(-1);
		}
		struct sockaddr_in temp;

		memset(&temp, 0, sizeof(struct sockaddr_in));
		temp.sin_family = AF_INET;
		temp.sin_addr   = *(struct in_addr *)he->h_addr;
		temp.sin_port   = htons(neighbor_port);
		neighbor = (struct sockaddr*)(&temp);
		/**/
		/*build_sockaddr("localhost", neighbor_port, &neighbor);*/
		fprintf(stderr, "received proxy msg, going to send rumor. \n");
		p2p_send_rumor(neighbor, s.pid, messageID);
    }
  	else if (strcmp(cmd, "get") == 0) {
		fprintf(stderr, "proxy want chatlog\n");
		p2p_get_chatlog();
    }
  	else if (strcmp(cmd, "crash\n") == 0) {
        fprintf(stderr, "crash server: %d\n", s.pid);
      	p2p_crash(); 
    }
  	else {
      	perror("[p2p_recv_proxy]: unrecognized command from proxy\n");
      	exit(-1);
    }
}

int verify_checksum(pkthdr *pkt) {
    uint16_t recv_checksum = pkt -> checksum;
    pkt -> checksum = 0;
    uint16_t verify_checksum = checksum((uint16_t *)pkt, sizeof(pkthdr) / sizeof(uint16_t));
    pkt -> checksum = recv_checksum;
    if (verify_checksum == recv_checksum) {
        return 1;
    }
    else {
        return 0;
    }
}

uint16_t checksum(uint16_t *buf, int nwords){
	uint32_t sum;

	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

void p2p_recv_server() {
	char recv_buf[sizeof(pkthdr)];
	struct sockaddr* from = NULL;
	socklen_t fromlen;  
  	ssize_t bytes_read = recvfrom(s.s2s_sockfd, recv_buf, sizeof(pkthdr), 0, from, &fromlen);
	/*  ssize_t recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen); */
	if (bytes_read == -1) {
		perror("recvfrom");
		exit(-1);
	}
	if (bytes_read == 0) {
		return;
	}

	/* fprintf(stderr, "s2s bytesread %lu\n", bytes_read); */
	pkthdr* recv_pkt = (pkthdr*)recv_buf;
	if (verify_checksum(recv_pkt)) {
		fprintf(stderr, "checksum ok\n");
		/* Unpack data */
		msghdr* recv_msg = (msghdr*)(recv_pkt -> data);
		fprintf(stderr, "219 s2s message type: %d\n", recv_msg -> type);
		fprintf(stderr, "220 s2s message from: %d\n", recv_msg -> from);

		/**/
		struct hostent *he;	
		if ((he = gethostbyname("localhost")) == NULL){
			perror("gethostbyname");
			exit(-1);
		}
		struct sockaddr_in temp;

		memset(&temp, 0, sizeof(struct sockaddr_in));
		temp.sin_family = AF_INET;
		temp.sin_addr   = *(struct in_addr *)he->h_addr;
		temp.sin_port   = htons(recv_msg -> from + ROOTID); 
		from = (struct sockaddr*)(&temp);
		/**/

		/*build_sockaddr("localhost", recv_msg -> from + ROOTID, &from);*/
		/* RUMOR */
		if (recv_msg -> type == 0) {
			/* Msg is expected */
			if (s.vector_clock[recv_msg -> origin] + 1 == recv_msg -> msg_id) {
				fprintf(stderr, "rumor message expected\n");
				p2p_add_msg(recv_msg -> origin, recv_msg -> msg_id, recv_msg -> msg, recv_msg -> msg_len);
			}
			/* Always send status back to the sender of the rumor msg */
			p2p_send_status(from);
		}
		/* STATUS */
		else if (recv_msg -> type == 1) {
			int i;
			for (i = 0; i < NUM_SERVERS; i++) {
				/* self has less */
				if (s.vector_clock[i] < recv_msg -> vector_clock[i]) {
					p2p_send_status(from);
					return;
				}
				/* self has more */
				else if (s.vector_clock[i] > recv_msg -> vector_clock[i]) {
					p2p_send_rumor(from, i, recv_msg -> vector_clock[i] + 1);
					return;
				}
				/* balanced at i */
				else {
					continue;
				}
			}
			/* Balanced */
			srand(time(0));
			/* Keep rumoring */ 
			if (rand() > 0.5 * RAND_MAX) {
				struct sockaddr* neighbor = NULL;
				int neighbor_port;
				if (rand() > 0.5 * RAND_MAX) {
					neighbor_port = ROOTID + s.pid + 1;
				}
				else {
					neighbor_port = ROOTID + s.pid - 1;
				}
				fprintf(stderr, "246: random neighbor I chose is %d\n", neighbor_port);
				/**/
				struct hostent *he;
				if ((he = gethostbyname("localhost")) == NULL){
					perror("gethostbyname");
					exit(-1);
				}
				struct sockaddr_in temp;

				memset(&temp, 0, sizeof(struct sockaddr_in));
				temp.sin_family = AF_INET;
				temp.sin_addr   = *(struct in_addr *)he->h_addr;
				temp.sin_port   = htons(neighbor_port);
				neighbor = (struct sockaddr*)(&temp);
				/**/
				/*build_sockaddr("localhost", neighbor_port, &neighbor);*/
				p2p_send_status(neighbor);
			}
			/* Stop rumoring */
			fprintf(stderr, "pid: %d thinks it is balanced with pid: %d, stop rumoring\n", s.pid, recv_msg -> from);
			return;
		}
		else {
			perror("Unknown msg type");
			exit(-1);
		}
	}
}

void p2p_send_rumor(struct sockaddr* to, int origin, int messageID) {
	/* Retrieve msg from chatlog, then construct msg */
	/* fprintf(stderr, "268: Inside sending rumor. \n"); */
	msg_log log = *(s.chatlog[origin][messageID]);
	msghdr data;
	data.type = RUMOR;
	data.msg_len = log.msg_len;
	data.from = s.pid;
	data.origin = origin;
	data.msg_id = messageID;
	memcpy(data.msg, log.msg, log.msg_len);

	/* Contruct packet */
	pkthdr packet;
	packet.checksum = 0;
	memcpy(packet.data, &data, sizeof(msghdr));
	packet.checksum = checksum((uint16_t *)(&packet), sizeof(pkthdr) / sizeof(uint16_t));
	/* ssize_t sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen); */
	/* fprintf(stderr, "284: before bytes_sent. \n"); */
	ssize_t bytes_sent = sendto(s.s2s_sockfd, &packet, sizeof(pkthdr), 0, to, sizeof(struct sockaddr));
	if (bytes_sent == -1) {
		perror("[p2p_send_rumor]: bytes_sent == -1");
		exit(-1);
	}
	/* fprintf(stderr, "290: bytes_sent %lu \n", bytes_sent); */
}

void build_sockaddr(char* domain, int port, struct sockaddr** receiver) {
	struct hostent *he;
	fprintf(stderr, "domain:%s\n", domain);
	if ((he = gethostbyname(domain)) == NULL){
		perror("gethostbyname");
		exit(-1);
	}
	struct sockaddr_in temp;
	/*--- Setting the neighbor's parameters -----*/
	memset(&temp, 0, sizeof(struct sockaddr_in));
	temp.sin_family = AF_INET;
	temp.sin_addr   = *(struct in_addr *)he->h_addr;
	temp.sin_port   = htons(port);
	*receiver = (struct sockaddr*)(&temp);
}

void p2p_send_status(struct sockaddr* to) {
	/* Construct msg */
	msghdr data;
	data.type = STATUS;
	data.from = s.pid;
	memcpy(data.vector_clock, s.vector_clock, NUM_SERVERS * sizeof(short));

	/* Construct packet */
	pkthdr packet;
	packet.checksum = 0;
	memcpy(packet.data, &data, sizeof(msghdr));
	packet.checksum = checksum((uint16_t *)(&packet), sizeof(pkthdr) / sizeof(uint16_t));
	/* ssize_t sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen); */
	ssize_t bytes_sent = sendto(s.s2s_sockfd, &packet, sizeof(pkthdr), 0, to, sizeof(struct sockaddr));
	if (bytes_sent == -1) {
		perror("[p2p_send_status]: bytes_sent == -1");
		exit(-1);
	}
}

/* Update chatlog and vector clock */ 
void p2p_add_msg(int origin, int messageID, char* msg, int msg_len) {
	/* Update chatlog */
	s.chatlog[origin][messageID] = malloc(sizeof(msg_log));
	s.chatlog[origin][messageID] -> msg_len = msg_len;
	memcpy(s.chatlog[origin][messageID] -> msg, msg, msg_len);
	
	/* Update vector clock */
	s.vector_clock[origin] = messageID;
}

int num_of_digits(uint16_t messageID) {
	int num = 0;
	while (messageID) {
		messageID /= 10;
		num++;
	}
	return num;
}

void p2p_get_chatlog() {
	fprintf(stderr, "vector clock informaiton: \n");
	int k;
	for (k = 0; k < NUM_SERVERS; k++) {
		fprintf(stderr, "pid: %d: %d\n", k, s.vector_clock[k]);
	}

	char load[MAX_MSG_NUM * (MAX_MSG_LEN + 1) + OFFSET] = "chatLog ";
	int i;
	int offset = OFFSET;
	for (i = 0; i < NUM_SERVERS; i++) {
		int j;
		for (j = 0; j < s.vector_clock[i] + 1; j++) {
			msg_log log = *(s.chatlog[i][j]);
			memcpy(load + offset, log.msg, log.msg_len);
			offset += log.msg_len;
			load[offset] = ','; 
			offset++;
		}
	}
	load[offset - 1] = '\n';
	/* chatLog <message>,<message>,... */
	fprintf(stderr, "%s\n", load);
	int bytes_sent = send(s.p2s_sockfd, load, offset, 0);
	if (bytes_sent == -1) {
		perror("bytes_sent == -1");
		exit(-1);
	}
	/* fprintf(stderr, "[p2p_get_chatlog]: bytessent %d\n", bytes_sent); */
	/* ssize_t send(int s, const void *buf, size_t len, int flags); */ 
}

void p2p_crash() {
	alarm(0);
	/* Free chatlog */
	int i;
	for (i = 0; i < NUM_SERVERS; i++) {
		int j;
		for (j = 0; j < s.vector_clock[i] + 1; j++) {
			free(s.chatlog[i][j]);
		}
	}
	close(s.temp_sockfd);
	close(s.p2s_sockfd);
	close(s.s2s_sockfd);
	
	exit(0);
}