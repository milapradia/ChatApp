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

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

//#define P(s) printf("P(%s)\n", s)
//#define V(s) printf("V(%s)\n", s)

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
void printgroup(group g)
{
	printf("Group details\n");
	printf("Group ID: %d\n", g.gid);
	printf("Admin: %d\n", g.admin);
	printf("Members\n");
	int i;
	for(i=0; i<g.memcount; i++)
	{
		printf("%d\n", g.members[i]);
	}
}
void error(char *msg)
{
	perror(msg);
	exit(1);
}
int addclient(client *clients, int fd)
{
	int i;
	printf("child\n");
	for(i=0; i<MAXCLIENT; i++)
	{
		if(clients[i].cno < 0)
		{
			clients[i].cno = i;
			clients[i].fd = fd;
			clients[i].id = 10000 + rand() % 90000;
			return i;
		}
	}
	return -1;
}
int addmessage(message *msgq, char *msg, int cid)
{
	int i;
	for(i=0; i<MAXCLIENT; i++)
	{
		if(msgq[i].cid < 0)
		{
			strcpy(msgq[i].msg, msg);
			msgq[i].cid = cid;
			return 1;
		}
	}
	return 0;
}
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
		smn.array[i] = 0;
		//semctl(sema, i, SETVAL, 0);
	}
	semctl(sema, 0, SETALL, smn);
	vop.sem_num = 0;
	pop.sem_num = 0;
	//*/
	/*
	char *semc = "semc";
	char *semg = "semg";
	char *sema = "sema";
	*/
	int writer = -1;
	for(i=0; i<MAXCLIENT; i++)
	{
		clients[i].cno = -1;
		clients[i].id = -1;
		msgq[i].msg[0] = '\0';
	}
	for(i=0; i<MAXGROUPS; i++)
	{
		groups[i].gid = -1;
		groups[i].admin = -1;
	}
	srand(time(0));
	while(1)
	{
		//Waiting for connection from client
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) 
			error("ERROR on accept");
		else
		{
			
			if(cgmcount[0] >= MAXCLIENT)
			{
				sprintf(response, "Number of Max client reached\n");
				n = write(newsockfd, response, strlen(response));
				if (n < 0) error("ERROR writing to socket");
				close(newsockfd);
			}
			else
			{
				printf("Prefork\n");
				P(semc);
				for(i=0; i<MAXCLIENT; i++)
				{
					if(clients[i].cno < 0)
					{
						clients[i].cno = i;
						clients[i].fd = newsockfd;
						clients[i].id = 10000 + rand() % 90000;
						msgq[i].msg[0] = '\0';
						break;
					}
				}
				V(semc);
				if(i<MAXCLIENT)
				{
					cindex = i;
					child = fork();
					if(child == 0)
					{
						//child
						printf("child\n");
						break;
					}
					printf("Forked parent\n");
					cgmcount[0]++;
				}
				else
				{
					sprintf(response, "Number of Max client reached\n");
					n = write(newsockfd, response, strlen(response));
					if (n < 0) error("ERROR writing to socket");
					close(newsockfd);
				}
			}	
		}
	}
	int tempin;
	if(child == 0)
	{
		//cindex = addclient(clients, newsockfd);
		printf("Adding client child\n");
		if(cindex >= 0)
		{
			printf("Connected to client index %d with Unique ID: %d\n", cindex+1, clients[cindex].id);
			sprintf(response, "Connected to client index %d with Unique ID: %d\n", cindex+1, clients[cindex].id);
			n = send(newsockfd, response, strlen(response), 0);
			if (n < 0) error("ERROR writing to socket");
			
			writer = fork();
			if(writer == 0)
			{
				//writer
				printf("Writer %d\n", cindex);
				vop.sem_num = clients[cindex].cno;
				pop.sem_num = clients[cindex].cno;
				while(1)
				{
					
					//printf("writer waiting %d\n", clients[cindex].id);
					//printf("sem %d: %d", pop.sem_num, sem_getvalue(sema, pop.sem_num));
					//printf("Waiting for A[%d]\n", pop.sem_num);
					P(sema);
					//printf("Acquired A[%d]\n", pop.sem_num);
					//scanf("%d", &tempin);
					//printf("Writing: %s\n", msgq[cindex].msg);
					if(msgq[cindex].msg[0] != '\0')
					{
						//printf("Seriously Writing: %s\n", msgq[cindex].msg);
						//sprintf(response, "Seriously Writing");
						if(strcmp(msgq[cindex].msg, "client killed") == 0)
						{
							
							msgq[cindex].msg[0] = '\0';
							//V(sema);
							printf("Quitting writer\n");
							break;
						}
						n = send(newsockfd, msgq[cindex].msg, strlen(msgq[cindex].msg), 0);
						printf("Sent: %s to %d\n", msgq[cindex].msg, clients[cindex].id);
						//V(sema);
						//n = send(newsockfd, response, strlen(response), 0);
						if (n < 0) error("ERROR writing to socket");
						if(strcmp(msgq[cindex].msg, "Notified everyone") == 0)
						{
							
							msgq[cindex].msg[0] = '\0';
							//V(sema);
							printf("Quitting writer\n");
							break;
						}
						msgq[cindex].msg[0] = '\0';
					}
					/*
					else
					{
						//printf("Message empty %d\n", clients[cindex].id);
						usleep(10);
					}
					*/
					//else
						//usleep(10);
					//V(sema);
					//printf("Released A[%d]\n", pop.sem_num);
				}
					
			}
			else
			{
				//reader and processor
				//Getting request
				
				while(1)
				{
					
					printf("Reader %d\n", cindex);
					n = recv(newsockfd, buffer, 255, 0);
					if (n < 0) error("ERROR reading from socket");
					else if(n == 0)
					{
					//CTRL + C
					
						int mindex = -1;
						int myid = clients[cindex].id;
						printf("CRTL + C\n");
						P(semg);
						for(g=0; g<MAXGROUPS; g++)
						{
							//chekcing admin
							if(groups[g].admin == myid)
							{
								groups[g].gid = -1;
								groups[g].admin = -1;
								continue;
							}
							mindex = -1;
							for(k=0; k<groups[g].memcount; k++)
							{
								//sendclient[groups[g].addmem[k]] = 0;
								if(groups[g].addmem[k] == cindex)
								{
									sprintf(response, "%s%d\n", response, groups[g].gid);
									mindex = k;
									groups[g].addmem[k] = -1;
									groups[g].members[k] = -1;
									break;
								}
							}
							if(mindex > -1)
							{
								for(k=mindex; k<groups[g].memcount-1; k++)
								{
									groups[g].addmem[k] = groups[g].addmem[k+1];
									groups[g].members[k] = groups[g].members[k+1];
								}
								groups[g].memcount--;
							}
						}
						V(semg);
						P(semc);
						
						clients[cindex].cno = -1;
						clients[cindex].id = -1;
						
						sprintf(sermsg, "%d quitting", myid);
						msgcount = 0;
						for(j=0; j<MAXCLIENT; j++)
						{
							if(clients[j].cno > -1)
							{
								sendclient[j] = 0;
							}
							else
							{
								sendclient[j] = 1;
								msgcount++;
							}
						}
						sendclient[cindex] = 1;
						printf("Msgcount: %d", msgcount);
						
						//scanf("%d", &tempin);
						while(1)
						{
							for(j=0; j<MAXCLIENT; j++)
							{
								if(j != cindex && sendclient[j] != 1 )	
								{
									printf("Notifying %d\n", clients[j].id);
									vop.sem_num = j;
									pop.sem_num = j;
									//P(sema);
									if(msgq[j].msg[0] == '\0')
									{
										
										strcpy(msgq[j].msg, sermsg);
										//printf("Written: %s\n", sermsg);
										sendclient[j] = 1;
										msgcount++;
										V(sema);
									}
									//V(sema);
								}
								printf("Msgcount %d : %d", clients[j].id, msgcount);
							}
							//scanf("%d", &tempin);
							printf("Msgcount: %d", msgcount);
							if(msgcount == MAXCLIENT)
								break;
						}
						
						V(semc);
						printf("Notified everyone\n");
						sprintf(response, "client killed");
						vop.sem_num = cindex;
						pop.sem_num = cindex;
						//P(sema);
						while(msgq[cindex].msg[0] != '\0');
						strcpy(msgq[cindex].msg, response);
						V(sema);
						cgmcount[0]--;
						//V(semg);
						//fclose(fptr);
						printf("waiting for child writer\n");
						wait(NULL);
						return 0; 
					
					}
					else
					{
						if(buffer[strlen(buffer)-1] == '\n')
							buffer[strlen(buffer)-1] = '\0';
						printf("Client %d sent message: %s\n", clients[cindex].id, buffer);
						index = 0;
						while(buffer[index] == ' ' || buffer[index] == '\t')
						{
							index++;
						}
						if(index != 0)
						{
							printf("Trimming leading white spaces\n");
							j = 0;
							while(buffer[j + index] != '\0')
							{
								buffer[j] = buffer[j + index];
								j++;
							}
							buffer[j] = '\0';
							printf("Trimmed input: %s\n", buffer);
						}
						strcpy(bufcopy, buffer);
						token = strtok(buffer, " ");
						//printf("semg waiting\n");
						
						
						//P(semg);
						
						
						//printf("semg down\n");
						if(strcmp(bufcopy, "/active") == 0)
						{
							P(semc);
							if(cgmcount[0]>0)
							{
								printf("Active clients - %d\n", cgmcount[0]);
								sprintf(response, "Active clients - %d\n", cgmcount[0]);
								for(j=0; j<MAXCLIENT; j++)
								{
									if(clients[j].cno >= 0)
									{
										sprintf(response, "%s%d", response, clients[j].id);
										if(j == cindex)	
											sprintf(response, "%s * This is you\n", response);
										else
											sprintf(response, "%s\n", response);	
									}
								}
							}
							V(semc);
						}
						else if(strcmp(token, "/send") == 0)
						{
						
							token = strtok(NULL, " ");
							//printf("%s\n", token);
							receiver = atoi(token);
							sprintf(r_str, "%d", receiver);
							//msgadded = 0;
							if(strcmp(token, r_str) == 0)
							{
								if(receiver != clients[cindex].id)
								{
									P(semc);
									for(j=0; j<MAXCLIENT; j++)
									{
										if(receiver == clients[j].id)	
										{
											break;
										}
									}
									V(semc);
									if(j>=cgmcount[0])
									{
										sprintf(response, "No such user");
									}
									else
									{
										token = strtok(NULL, "\n");
										if(token != NULL)
										{
											//char sermsg[256];
											//bzero(sermsg,256);
											sprintf(sermsg, "%d said %s", clients[cindex].id, token);
											printf("Writing: %s\n", sermsg);
											while(1)
											{
												vop.sem_num = j;
												pop.sem_num = j;
												//P(sema);
												if(msgq[j].msg[0] == '\0')
												{
											
													strcpy(msgq[j].msg, sermsg);
													//printf("Written: %s\n", sermsg);
													sprintf(response, "Sent %s to %d", token, clients[j].id);
													V(sema);
													break;
												}
												/*
												else if(strcmp(msgq[j].msg, "Notified everyone") == 0)
												{
													sprintf(response, "Unable to send...User %d offline", receiver);
													V(sema);
													break;
												}
												*/
												//V(sema);
											}
											/*
											for(j=0; j<MAXCLIENT; j++)
											{
												if(msgq[j].msg[0] != '\0')
												{
													printf("Client %d: %s\n", j, msgq[j].msg);
												}
											}
											*/
										}
										else
										{
											sprintf(response, "Empty message");
										}
									}
								}
								else
								{
									sprintf(response, "Cannot send to same client");
								}
							}
							else
							{
								sprintf(response, "Invalid client ID");
							}
						}
						else if(strcmp(token, "/broadcast") == 0)
						{
							token = strtok(NULL, "");
							//bzero(sermsg,256);
							sprintf(sermsg, "%d Broadcasted %s", clients[cindex].id, token);
							msgcount = 1;
							P(semc);
							for(j=0; j<MAXCLIENT; j++)
							{
								if(clients[j].cno > -1)
								{
									sendclient[j] = 0;
									
								}
								else
								{
									sendclient[j] = 1;
									msgcount++;
								}
							}
							P(semc);
							sendclient[cindex] = 1;
							/*
							for(j=0; j<MAXCLIENT; j++)
							{
								printf("%d ", sendclient[j]);
							}
							*/
							//printf("MSGcount =  %d\n", msgcount);
							sprintf(response, "Broadcasting: %s\n", token);
							while(1)
							{
								P(semc);
								for(j=0; j<MAXCLIENT; j++)
								{
									//clients[j].
									if(clients[j].id > -1 && j != cindex && sendclient[j] != 1 )	
									{
										vop.sem_num = j;
										pop.sem_num = j;
										//P(sema);
										if(msgq[j].msg[0] == '\0')
										{
											
											strcpy(msgq[j].msg, sermsg);
											//printf("Written: %s\n", sermsg);
											sendclient[j] = 1;
											sprintf(response, "%sMessage %s sent to User %d\n", response, token, clients[j].id);
											msgcount++;
											V(sema);
										}
										/*
										else if(strcmp(msgq[j].msg, "Notified everyone") == 0)
										{
											sprintf(response, "%sUnable to send...User offline\n", response);
											sendclient[j] = 1;
											//V(sema);
										}
										*/
										//V(sema);
									}
								}
								V(semc);
								if(msgcount == MAXCLIENT)
									break;
							}
							sprintf(response, "Message broadcasted: %s", token);
						}
						else if(strcmp(token, "/makegroup") == 0)
						{
							printf("Making group\n");
							
							/*
							group *newgroup = (group *)malloc(sizeof(group *));
							node *newnode = (node *)malloc(sizeof(node *));
							newnode->next = NULL;
							newnode->id = 10000 + rand() % 90000;
							newgroup->member = newnode;
							*/
							//gcount++;
							printf("Waiting for G: %d\n", clients[cindex].id);
							//P(semg);
							printf("Acquired G: %d\n", clients[cindex].id);
							for(g=0; g<MAXGROUPS; g++)
							{
								if(groups[g].gid < 0)
								{
									groups[g].admin = clients[cindex].id;
									groups[g].gid = 10000 + rand() % 90000;
									groups[g].memcount = 1;
									groups[g].members[0] = clients[cindex].id;
									groups[g].addmem[0] = cindex ;
									break;
								}
							}
							
							if(g<MAXGROUPS)
							{
								printf("GID: %d\n", groups[g].gid);
								int groupvalid = 1, clientfound = 0;
								token = strtok(NULL, " ");
								while(token != NULL)
								{
									printf("Token: %s\n", token);
									if(strlen(token) > 5)
									{
										sprintf(response, "Invalid client ID" );
										groupvalid = 0;
										break;
									}
									cid = atoi(token);
									sprintf(cidstr, "%d", cid);
									if(strcmp(token, cidstr) == 0)
									{
										for(j=0; j<MAXCLIENT; j++)
										{
											if(clients[j].id == cid)	
											{
												break;
											}
										}
										if(j < MAXCLIENT)
										{
											//client found
											for(k=0; k<groups[g].memcount; k++)
											{
												if(groups[g].members[k] == cid)	
												{
													break;
												}
											}
											if(k < groups[g].memcount)
											{
												//duplicate entry
												sprintf(response, "duplicate client ID" );
												groupvalid = 0;
												break;
											}
											else
											{
												groups[g].members[groups[g].memcount] = cid;
												groups[g].addmem[groups[g].memcount] = j;
												groups[g].memcount++;
												if(groups[g].memcount > MAXGROUPSIZE)
												{
													sprintf(response, "Max group size exceeded" );
													groupvalid = 0;
													break;
												}
											}
										}
										else
										{
											//not found
											sprintf(response, "Client not found" );
											groupvalid = 0;
											break;
										}
									}
									else
									{
										sprintf(response, "Invalid client ID" );
										groupvalid = 0;
										break;
									}
									token = strtok(NULL, " ");
								}
								if(groupvalid == 0)
								{
									printf("Group not created\n");
									groups[g].gid = -1;
									groups[g].admin = -1;
									for(k=0; k<groups[g].memcount; k++)
									{
										groups[g].members[k] = -1;
										groups[g].addmem[k] = -1;
									}
									groups[g].memcount = 0;
								}
								else
								{
									printf("Group created\n");
									sprintf(sermsg, "You have been added to group %d", groups[g].gid);
									sprintf(response, "group created: %d\n", groups[g].gid);
									cgmcount[1]++;
									for(j=0; j<MAXCLIENT; j++)
									{
										sendclient[j] = 1;
									}
									for(k=0; k<groups[g].memcount; k++)
									{
										sendclient[groups[g].addmem[k]] = 0;
									}
									sendclient[cindex] = 1;
									msgcount = MAXCLIENT - groups[g].memcount + 1;
									while(1)
									{
										for(j=0; j<MAXCLIENT; j++)
										{
											if(j != cindex && sendclient[j] != 1 )
											{
												vop.sem_num = j;
												pop.sem_num = j;
												printf("Waiting for A[%d]\n", j);
												//P(sema);
												if(msgq[j].msg[0] == '\0')
												{
												
													strcpy(msgq[j].msg, sermsg);
													//printf("Written: %s\n", sermsg);
													sprintf(response, "%s%d\n", response, clients[j].id);
													sendclient[j] = 1;
													msgcount++;
													V(sema);
												}
												//V(sema);
												printf("Released A[%d]\n", j);
											}
										}
										if(msgcount == MAXCLIENT)
											break;
									}
								}
							}
							else
							{
								sprintf(response, "Maximum groups limit reached\n");
							}
							//V(semg);
							printf("Released G: %d\n", clients[cindex].id);
						}
						else if(strcmp(token, "/sendgroup") == 0)
						{
							token = strtok(NULL, " ");
							gid = atoi(token);
							sprintf(gidstr, "%d", gid);
							if(strcmp(token, gidstr) == 0)
							{
								token = strtok(NULL, " ");
								if(token != NULL)
								{
									P(semg);
									for(g=0; g<MAXGROUPS; g++)
									{
										if(groups[g].gid == gid)
										{
											//token = strtok(NULL, " ");
												for(k=0; k<groups[g].memcount; k++)
												{
													if(groups[g].addmem[k] == cindex)
													{
														break;
													}
												}
												if(k<groups[g].memcount)
												{
													printf("Sending msg %s to group id: %d\n", token, gid);
													sprintf(sermsg, "%d send in group %d: %s", clients[cindex].id, gid, token);
													for(j=0; j<MAXCLIENT; j++)
													{
														sendclient[j] = 1;
													}
													for(k=0; k<groups[g].memcount; k++)
													{
														sendclient[groups[g].addmem[k]] = 0;
													}
													sendclient[cindex] = 1;
													msgcount = MAXCLIENT - groups[g].memcount + 1;
													while(1)
													{
														for(j=0; j<MAXCLIENT; j++)
														{
															if(j != cindex && sendclient[j] != 1 )
															{
																vop.sem_num = j;
																pop.sem_num = j;
																//P(sema);
																if(msgq[j].msg[0] == '\0')
																{
															
																	strcpy(msgq[j].msg, sermsg);
																	//printf("Written: %s\n", sermsg);
																	sendclient[j] = 1;
																	msgcount++;
																	V(sema);
																}
																//V(sema);
															}
														}
														if(msgcount == MAXCLIENT)
															break;
													}
													sprintf(response, "Sent in group %d: %s", gid, token);
													break;
												}
												else
												{
													sprintf(response, "You are not a member of %d", gid);
													break;
												}
												
										}
									}
									V(semg);
									if(g >= MAXGROUPS)
									{
										sprintf(response, "Group ID %d not found", gid);
									}
								}
								else
								{
									sprintf(response, "Empty message");
								}
								
							}
							else
							{
								sprintf(response, "Invalid group ID");
							}
						}
						else if(strcmp(token, "/activegroups") == 0)
						{
							sprintf(response, "Active groups of %d\n", clients[cindex].id);
							int gc = 0; 
							printf("Semg waiting\n");
							P(semg);
							printf("Semg down\n");
							for(g=0; g<MAXGROUPS; g++)
							{
								if(groups[g].gid > -1)
								{
									for(k=0; k<groups[g].memcount; k++)
									{
										//sendclient[groups[g].addmem[k]] = 0;
										if(groups[g].addmem[k] == cindex)
										{
											sprintf(response, "%s%d\n", response, groups[g].gid);
											gc++;
											break;
										}
									}	
								}
							}
							V(semg);
							//printf("Semg up\n");
							sprintf(response, "%sTotal active groups of %d: %d\n", response, clients[cindex].id, gc);
						}
						else if(strcmp(token, "/quit") == 0)
						{
							//P(semg);
							//Cleaning up groups
							int mindex = -1;
							int myid = clients[cindex].id;
							printf("/quit");
							P(semg);
							for(g=0; g<MAXGROUPS; g++)
							{
								//chekcing admin
								if(groups[g].admin == myid)
								{
									groups[g].gid = -1;
									groups[g].admin = -1;
									continue;
								}
								mindex = -1;
								for(k=0; k<groups[g].memcount; k++)
								{
									//sendclient[groups[g].addmem[k]] = 0;
									if(groups[g].addmem[k] == cindex)
									{
										sprintf(response, "%s%d\n", response, groups[g].gid);
										mindex = k;
										groups[g].addmem[k] = -1;
										groups[g].members[k] = -1;
										break;
									}
								}
								if(mindex > -1)
								{
									for(k=mindex; k<groups[g].memcount-1; k++)
									{
										groups[g].addmem[k] = groups[g].addmem[k+1];
										groups[g].members[k] = groups[g].members[k+1];
									}
									groups[g].memcount--;
								}
							}
							V(semg);
							P(semc);
							
							clients[cindex].cno = -1;
							clients[cindex].id = -1;
							
							sprintf(sermsg, "%d quitting", myid);
							msgcount = 0;
							for(j=0; j<MAXCLIENT; j++)
							{
								if(clients[j].cno > -1)
								{
									sendclient[j] = 0;
								}
								else
								{
									sendclient[j] = 1;
									msgcount++;
								}
							}
							sendclient[cindex] = 1;
							printf("Msgcount: %d", msgcount);
							int tempin;
							//scanf("%d", &tempin);
							while(1)
							{
								for(j=0; j<MAXCLIENT; j++)
								{
									if(j != cindex && sendclient[j] != 1 )	
									{
										printf("Notifying %d\n", clients[j].id);
										vop.sem_num = j;
										pop.sem_num = j;
										//P(sema);
										if(msgq[j].msg[0] == '\0')
										{
											
											strcpy(msgq[j].msg, sermsg);
											//printf("Written: %s\n", sermsg);
											sendclient[j] = 1;
											msgcount++;
											V(sema);
										}
										//V(sema);
									}
									printf("Msgcount %d : %d", clients[j].id, msgcount);
								}
								//scanf("%d", &tempin);
								printf("Msgcount: %d", msgcount);
								if(msgcount == MAXCLIENT)
									break;
							}
							
							V(semc);
							printf("Notified everyone\n");
							sprintf(response, "Notified everyone");
							vop.sem_num = cindex;
							pop.sem_num = cindex;
							//P(sema);
							while(msgq[cindex].msg[0] != '\0');
							strcpy(msgq[cindex].msg, response);
							V(sema);
							cgmcount[0]--;
							//V(semg);
							//fclose(fptr);
							printf("waiting for child writer\n");
							wait(NULL);
							return 0; 
						}
						else if(strcmp(token, "/activeallgroups") == 0)
						{
							sprintf(response, "Active groups\n");
							int gc = 0;
							P(semg);
							for(g=0; g<MAXGROUPS; g++)
							{
								if(groups[g].gid > -1)
								{
									sprintf(response, "%s%d\n", response, groups[g].gid);
									gc++;
								}
							}
							V(semg);
							sprintf(response, "%sTotal groups: %d\n", response, gc);
						}
						else
						{
							sprintf(response, "Invalid command");
						}
						//printf("semg down\n");
						//V(semg);
						//printf("%d semg up\n", clients[cindex].id);
						n = send(newsockfd, response, strlen(response), 0);
						if (n < 0) error("ERROR writing to socket");
					}
				}
			}
		}
	}
	fclose(fptr);
	///*
	semctl(semc, 0, IPC_RMID, 0);
	semctl(semg, 0, IPC_RMID, 0);
	semctl(sema, 0, IPC_RMID, 0);
	//*/
	return 0; 
}
