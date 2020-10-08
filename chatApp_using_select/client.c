/*
Submitted by
Milap Radia
19CS60R57
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <sys/signal.h>

#define PORTNO 8057
#define MAX_LIMIT 255

#define P(s) semop(s, &pox, 1)
#define V(s) semop(s, &vop, 1)

void error(char *msg)
{
    perror(msg);
    exit(0);
}
typedef struct node 
{
	char *msg;
	struct node* next;
}node;

int main(int argc, char *argv[])
{
    int sockfd, portno, n, i;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256], bufcopy[256];
    char *token;
    portno = PORTNO;
    //Opening socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    int k;
    char msg[256], str[256]; 
	int child = -1;
	
	key_t key;
	//Defining sembuf for P and V operation
	struct sembuf pox, vop;
	pox.sem_num = vop.sem_num = 0;
	pox.sem_flg = vop.sem_flg = IPC_NOWAIT;
	pox.sem_op = -1; vop.sem_op = 1;
	
	//Getting Semaphores
	int semout;
	key = ftok(".", 'c');
	semout = semget(key, 1, 0666 | IPC_CREAT);
	semctl(semout, 0, SETVAL, 1);
	
	child = fork();
	if(child == 0)
	{
		//Child
		while(1)
		{
			//printf("Please enter the command: ");
			//printf("child Writer\n");
			bzero(buffer,256);
			fgets(buffer, MAX_LIMIT, stdin);
			/*
			if(P(semout) > -1)
			{
				printf("Writer Acquired SEM\n");
				
				printf("Writer leaving SEM\n");
				V(semout);
			}
			else
				continue;
			*/
			strcpy(bufcopy, buffer);
			//n = write(sockfd, buffer, strlen(buffer)+1);
			
			n = send(sockfd, buffer, strlen(buffer)+1, 0);
			if (n < 0) 
				 error("ERROR writing to socket");
			//printf("Sent %s\n", buffer);
		}
	}
	else
	{
		//Parent
		
		node *front = (node *)malloc(sizeof(node *));
		node *rear = front;
		while(1)
		{
			//printf("Parent reader\n");
			char sermsg[256];
			int sender = -1;
			bzero(sermsg,256);
			//n = read(sockfd, sermsg, 256);
			//printf("Reading\n");
			n = recv(sockfd, sermsg, 256, 0);
			if (n < 0) 
				 error("ERROR reading from socket");
			else if(n == 0)
			{
				kill(child, SIGKILL);
				printf("Server exited...\n");
				break;
			}
			//printf("%d - %s\n", n, sermsg);
			//fflush(stdout);
			if(sermsg[strlen(sermsg)-1] == '\n')
				sermsg[strlen(sermsg)-1] = '\0';
			
			printf("%s\n", sermsg);
			if(strcmp(sermsg, "Number of Max client reached") == 0 || strcmp(sermsg, "Notified everyone") == 0)
			{
				printf("Quiting...\n");
				kill(child, SIGKILL);
				break;
			}
			/*
			node *temp = (node *)malloc(sizeof(node *));
			strcpy(temp->msg, sermsg);
			temp->msg = sermsg;
			temp->next = NULL;
			rear->next = temp;
			rear = rear->next;
			printf("Reader trying to acquire SEM\n");
			while(front != NULL)
				{
					printf("%s\n", front->msg);
					front = front->next;
				}
			/*
			if(P(semout) > -1)
			{
				printf("Reader Acquired SEM\n");
				while(front != NULL)
				{
					printf("%s\n", front->msg);
					front = front->next;
				}
				printf("Reader leaving SEM\n");
				V(semout);				
			}
			else
				printf("ERRNO %d\n", errno);	
			*/		
		}
	}
	/*
    while(1)
    {
		printf("Please enter the command: ");
		bzero(buffer,256);
		fgets(buffer, MAX_LIMIT, stdin);
		n = write(sockfd, buffer, strlen(buffer)+1);
		if (n < 0) 
		     error("ERROR writing to socket");

		char sermsg[256];
		bzero(sermsg,256);
		n = read(sockfd, sermsg, 256);
		if (n < 0) 
		     error("ERROR reading from socket");
		printf("Server replied: %s\n\n", sermsg);
    }
    */
    return 0;
}
