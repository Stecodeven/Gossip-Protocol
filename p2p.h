#ifndef _p2p_h
#define _p2p_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signal.h>
#include <string.h>
#include <poll.h>

/*----- Error variables -----*/
extern int h_errno;
extern int errno;

/*----- Macros -----*/
#define PKTHDR          2
#define NUM_SERVERS		4
#define MAX_MSG_LEN		200
#define MAX_MSG_NUM     1000    /* Maximum total number of messages */
#define TIMEOUT 		10
#define ROOTID			20000
#define NUM_FD			2
#define OFFSET          8

/*----- Packet format -----*/
enum msg_type {
	RUMOR=0,
  	STATUS,
};

typedef struct {
    enum msg_type type;
    uint16_t msg_len;
    uint16_t from; 						/* sender of the message, by pid */
    uint16_t origin; 					/* where the message originated, by pid */
    uint16_t msg_id;					
    short vector_clock[NUM_SERVERS]; /* index is pid, v[i] is msg_id (continually largest) */
    char msg[MAX_MSG_LEN]; 				/* MAX_MSG_LEN is defined to be 200 */
} __attribute__((packed)) msghdr;

typedef struct {
    uint16_t checksum;     		        /* header and payload checksum                */
    uint8_t data[sizeof(msghdr)]; 		/* pointer to the payload                     */
} __attribute__((packed)) pkthdr;

typedef struct msg_log {
    uint16_t msg_len;
    char msg[MAX_MSG_LEN];
} msg_log;

typedef struct state_t {
	int temp_sockfd;			/* socketfd for listening TCP connection from proxy */
  	int s2s_sockfd;			    /* UDP shared by neighber servers */
  	int p2s_sockfd;             /* new socketfd for further communication with proxy, returned from accept() */
    struct pollfd pfds[NUM_FD];
	uint16_t pid;
  	struct sockaddr* proxy_addr;
    msg_log* chatlog[NUM_SERVERS][MAX_MSG_NUM];         /* chatlog(pid:msgs_addr), msgs(msgid:msg), msg:char[]; char msg[MAX_MSG_LEN], char* msgs[MAX_MSG_NUM], */
    short vector_clock[NUM_SERVERS];
} state_t;

extern state_t s;

int p2p_socket(int domain, int type, int protocol);
int p2p_bind(int sockfd, const struct sockaddr *addr, socklen_t socklen);
int p2p_listen(int sockfd, int backlog);
int p2p_accept(int sockfd);
void p2p_run(uint16_t pid);
void p2p_recv_proxy();
void p2p_recv_server(); 
void p2p_add_msg(int origin, int messageID, char* msg, int msg_len);
void p2p_send_rumor(struct sockaddr* to, int origin, int messageID);
void p2p_send_status(struct sockaddr* to);
void p2p_get_chatlog();
void p2p_crash();
void anti_entropy();

/* Additional Functions */
uint16_t checksum(uint16_t *buf, int nwords);
int verify_checksum(pkthdr *pkt);
int num_of_digits(uint16_t messageID);
void build_sockaddr(char* domain, int port, struct sockaddr** receiver);

#endif