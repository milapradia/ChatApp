/*
Submitted by
Milap Radia
19CS60R57
*/
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <semaphore.h>

#define PORTNO 8057
#define MAX_LIMIT 255
#define STDIN 0
#define MAXCLIENT 5
#define MAXGROUPSIZE 5
#define MAXGROUPS 5
#define MSGQ 15
///*
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)
//*/
/*
#define P(s) printf("P(%s)\n", s)
#define V(s) printf("V(%s)\n", s)
//*/
typedef struct client 
{
	int id;
	int fd;
	int cno;
}client;
typedef struct group 
{
	int gid;
	int admin;
	int members[10];
	int addmem[10];
	int memreq[10];
	int memcount;
	int status;
}group;
typedef struct message 
{
	char msg[256];
	int cid;
}message;
union semun 
{
       int              val;    /* Value for SETVAL */
       struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
       unsigned short  *array;  /* Array for GETALL, SETALL */
       struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                   (Linux-specific) */
};

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen;
	char buffer[256], bufcopy[256], response[256], sermsg[256];;
	int client_number = 0;
	
	struct sockaddr_in serv_addr, cli_addr;
	struct sockaddr_storage remoteaddr;
	int n, i, index, j, cindex, g;
	FILE *fptr;
	char *token;
	
	//server_records.txt File open
	fptr = fopen("server_records.txt", "a+");
	int k, cid, x, gid;
	char msg[256], r_str[256], cidstr[10], gidstr[10];	
	socklen_t addrlen;
	char *line = NULL;
	size_t len = 0;
	ssize_t readline;
	int brflag = 0;
	int sender, receiver, child;
	int sendclient[MAXCLIENT];
	int msgcount = 0;
	//Opening socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	error("ERROR opening socket");
	int option = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = PORTNO;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	
	//Binding socket to PORTNO
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
		  sizeof(serv_addr)) < 0) 
		  error("ERROR on binding");
	
	//Server ready to listen on PORTNO
	listen(sockfd, 1);
	clilen = sizeof(cli_addr);
	
	//Defining sembuf for P and V operation
	struct sembuf pop, vop;
	pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1; vop.sem_op = 1;
	
	group *groups;
	client *clients;
	message *msgq;
	int *cgmcount;
	
	key_t key;
	int shmidc, shmidq, shmidg, shmidm;
	//Allocating Shared memory
	key = ftok(".", 'c');
	shmidc = shmget(key, sizeof(client[MAXCLIENT]),IPC_CREAT | 0666);
	clients = (client *)shmat(shmidc,0,0);
	
	key = ftok(".", 'g');
	shmidg = shmget(key, sizeof(group[MAXGROUPSIZE]),IPC_CREAT | 0666);
	groups = (group *)shmat(shmidg,0,0);
	
	key = ftok(".", 'q');
	shmidq = shmget(key, sizeof(message[MSGQ]),IPC_CREAT | 0666);
	msgq = (message *)shmat(shmidq,0,0);
	
	key = ftok(".", 'm');
	shmidm = shmget(key, sizeof(int[3]),IPC_CREAT | 0666);
	cgmcount = (int *)shmat(shmidm,0,0);
	cgmcount[0] = 0;
	
	
	///*
	int semc;
	key = ftok(".", 'C');
	semc = semget(key, 1, 0666 | IPC_CREAT);
	semctl(semc, 0, SETVAL, 1);
	
	int semg;
	key = ftok(".", 'G');
	semg = semget(key, 1, 0666 | IPC_CREAT);
	semctl(semg, 0, SETVAL, 1);
	
	int sema;
	key = ftok(".", 'A');
	sema = semget(key, MAXCLIENT, 0666 | IPC_CREAT);
	semctl(sema, 0, SETVAL, 0);
	
	union semun smn;
	smn.array = (short *)malloc(sizeof(short[MAXCLIENT]));
	
	//Initializing sema for msg queuq
	for(i=0; i<MAXCLIENT; i++)
	{
		//vop.sem_num = i;
		//V(sema);
		//smn.array[i] = 0;
		semctl(sema, i, SETVAL, 0);
	}
	//semctl(sema, 0, SETALL, smn);
	vop.sem_num = 0;
	pop.sem_num = 0;
	//*/
	
	fclose(fptr);
	///*
	semctl(semc, 0, IPC_RMID, 0);
	semctl(semg, 0, IPC_RMID, 0);
	semctl(sema, 0, IPC_RMID, 0);
	shmctl(shmidc, IPC_RMID, 0);
	shmctl(shmidq, IPC_RMID, 0);
	shmctl(shmidg, IPC_RMID, 0);
	shmctl(shmidm, IPC_RMID, 0);
	//*/
	return 0; 
}
