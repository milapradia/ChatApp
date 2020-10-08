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
#include <sys/sem.h>
#include <unistd.h>
#include <sys/signal.h>


#define PORTNO 8057
#define MAX_LIMIT 255
#define STDIN 0
#define MAXCLIENT 10
#define MAXGROUPSIZE 5


#define P(s) semop(s, &pox, 1)
#define V(s) semop(s, &vop, 1)

typedef struct node 
{
	int id;
	int fd;
	int cno;
	struct node* next;
}node;
typedef struct group 
{
	int gid;
	int admin;
	node* members;
	node* addmem;
	node* memreq;
	int memcount;
}group;

typedef struct grouplist 
{
	group g;
	struct grouplist* next;
}grouplist;

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
void printgroupdetails(group g)
{
	printf("Group details\n");
	printf("Group ID: %d\n", g.gid);
	printf("Admin: %d\n", g.admin);
	printf("Members\n");
	node *inode = g.members;
	while(inode != NULL)
	{
		printf("%d\n", inode->id);
		inode = inode->next;
	}
}
void error(char *msg)
{
	perror(msg);
	exit(1);
}
node *findnodebyID(node *nl, int id)
{
	node *inode = nl;
	while(inode != NULL)
	{
		if(inode->id == id)	
		{
			break;
		}
		inode = inode->next;
	}
	return inode;
}
node *findnodebyFD(node *nl, int fd)
{
	node *inode = nl;
	while(inode != NULL)
	{
		if(inode->fd == fd)	
		{
			break;
		}
		inode = inode->next;
	}
	return inode;
}
int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen, client_active;
	char buffer[256], bufcopy[256];
	int client_number = 0;
	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	int fdmax; // maximum file descriptor number
	struct sockaddr_in serv_addr, cli_addr;
	struct sockaddr_storage remoteaddr;
	int n, i, index, j;
	FILE *fptr;
	char *token;
	
	//server_records.txt File open
	fptr = fopen("server_records.txt", "a+");
	unsigned long elapsed;
	int k, cid, x, f;
	char msg[256], r_str[256], cidstr[10], gidstr[10];	
	socklen_t addrlen;
	char *invalidreq = "INVALID REQUEST!!", *ack = "acknoledged";
	char *line = NULL;
	size_t len = 0;
	ssize_t readline;
	struct timeval tv;
	long long t1, t2;
	long long cltime[256];
	int brflag = 0;
	int sender, receiver;
	
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	
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
	FD_SET(sockfd, &master);
	fdmax = sockfd;
	
	key_t key;
	//Defining sembuf for P and V operation
	struct sembuf pox, vop;
	pox.sem_num = vop.sem_num = 0;
	pox.sem_flg = vop.sem_flg = 0;
	pox.sem_op = -1; vop.sem_op = 1;
	
	//Getting Semaphores
	int log;
	key = ftok(".", 'l');
	log = semget(key, 1, 0666 | IPC_CREAT);
	semctl(log, 0, SETVAL, 1);
	
	grouplist *gl = NULL;
	grouplist *reqgl = NULL;
	int gcount = 0, gid;
	node *clients = NULL;
	
	while(1)
	{
		//Waiting for connection from client
		read_fds = master;
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			error("error in select");
			exit(4);
		}
		
		for(i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &read_fds)) 
			{
				if (i == sockfd)
				{
					addrlen = sizeof remoteaddr;
					newsockfd = accept(sockfd, (struct sockaddr *)&remoteaddr, &addrlen);
					if (newsockfd < 0) error("ERROR on accept");
					else 
					{
						char response[256];
						if(client_number >= MAXCLIENT)
						{
							printf("Number of Max client reached\n");
							sprintf(response, "Number of Max client reached");
							printf("writing response to FD %d\n", response, newsockfd);
							n = write(newsockfd, response, strlen(response));
							if (n < 0) error("ERROR writing to socket");
							//close(newsockfd);	
							break;
						}	
						client_number++;
						node *newnode = (node *)malloc(sizeof(node *));
						newnode->id = 10000 + rand() % 90000;
						newnode->fd = newsockfd;
						newnode->cno = client_number;
						if(clients == NULL)
						{
							//First client
							newnode->next = clients;
							clients = newnode;
						}
						else
						{
							newnode->next = clients;
							clients = newnode;
						}
						printf("Connected to client number %d with Unique ID: %d\n", client_number, newnode->id);
						sprintf(response, "Connected to server with Unique ID: %d\n", newnode->id);
						n = write(newsockfd, response, strlen(response));
						if (n < 0) error("ERROR writing to socket");
						FD_SET(newsockfd, &master);
						if (newsockfd > fdmax) { // keep track of the max
							fdmax = newsockfd;
						}
					}
				}
				else
				{
					char response[256];
					node *mynode = clients;
					node *myprevnode = NULL;
					client_active = 1;
					while(mynode != NULL)
					{
						if(mynode->fd == i)	
							break;
						myprevnode = mynode;
						mynode = mynode->next;
					}
					printf("Current client\n");
					printf("ID: %d\nFD: %d\n", mynode->id, mynode->fd);
					//Getting request
					//n = read(i, buffer, 255);
					n = recv(i, buffer, 255, 0);
					if (n < 0) error("ERROR reading from socket");
					else if (n == 0)
					{
						client_active = 0;
						grouplist *igl = gl;
						grouplist *prevgl = NULL;
						printf("Removing client from groups\n");
						while(igl != NULL)
						{
							printf("checking group ID: %d\n", igl->g.gid);
							printgroupdetails(igl->g);
							if(igl->g.admin == mynode->id)
							{
								grouplist *todelete;
								printf("%d admin of %d\n", mynode->id, igl->g.gid);
								if(prevgl != NULL)
								{
									todelete = igl;
									prevgl->next = igl->next;
									igl = prevgl;
									//free(todelete);
								}
								else
								{
									todelete = gl;
									gl = gl->next;	
									prevgl = NULL;								
								}
								igl = igl->next;
								//free(todelete);
								continue;
							}
							
							printf("checking members\n");
							node *inode = igl->g.members;
							if(inode->fd == i)
							{
								node *todelete = igl->g.members;
								igl->g.members = igl->g.members->next;
								free(todelete);
							}
							else
							{
								while(inode->next != NULL)
								{
									if(inode->next->fd == i)
									{
										node *todelete = inode->next;
										inode->next = inode->next->next;
										free(todelete);
										break;
									}
									inode = inode->next;
								}	
							}
							
							printf("checking addmem\n");
							inode = igl->g.addmem;
							if(inode != NULL)
							{
								if(inode->fd == i)
								{
									node *todelete = igl->g.addmem;
									igl->g.addmem = igl->g.addmem->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							
							printf("checking memreq\n");
							inode = igl->g.memreq;
							if(inode != NULL)
							{
								printf("memreq not NULL\n");
								if(inode->fd == i)
								{
									node *todelete = igl->g.memreq;
									igl->g.memreq = igl->g.memreq->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							printf("CHECKED group ID: %d\n", igl->g.gid);
							prevgl = igl;
							igl = igl->next;	
						}
						
						igl = reqgl;
						prevgl = NULL;
						printf("Removing client from inactive groups\n");
						while(igl != NULL)
						{
							printf("checking group ID: %d\n", igl->g.gid);
							printgroupdetails(igl->g);
							if(igl->g.admin == mynode->id)
							{
								grouplist *todelete;
								printf("%d admin of %d\n", mynode->id, igl->g.gid);
								if(prevgl != NULL)
								{
									todelete = igl;
									prevgl->next = igl->next;
									igl = prevgl;
									//free(todelete);
								}
								else
								{
									todelete = gl;
									reqgl = reqgl->next;	
									prevgl = NULL;								
								}
								igl = igl->next;
								//free(todelete);
								continue;
							}
							
							printf("checking members\n");
							node *inode = igl->g.members;
							if(inode->fd == i)
							{
								node *todelete = igl->g.members;
								igl->g.members = igl->g.members->next;
								free(todelete);
							}
							else
							{
								while(inode->next != NULL)
								{
									if(inode->next->fd == i)
									{
										node *todelete = inode->next;
										inode->next = inode->next->next;
										free(todelete);
										break;
									}
									inode = inode->next;
								}	
							}
							
							printf("checking addmem\n");
							inode = igl->g.addmem;
							if(inode != NULL)
							{
								if(inode->fd == i)
								{
									node *todelete = igl->g.addmem;
									igl->g.addmem = igl->g.addmem->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							
							printf("checking memreq\n");
							inode = igl->g.memreq;
							if(inode != NULL)
							{
								printf("memreq not NULL\n");
								if(inode->fd == i)
								{
									node *todelete = igl->g.memreq;
									igl->g.memreq = igl->g.memreq->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							printf("CHECKED group ID: %d\n", igl->g.gid);
							prevgl = igl;
							igl = igl->next;	
						}
						
						
													
						int myclientid = mynode->id;
						//deleting client
						printf("Deleting client\n");
						if(myprevnode != NULL)
						{
							node *todelete = mynode;
							myprevnode->next = mynode->next;
							free(todelete);
						}
						else
						{
							node *todelete = mynode;
							clients = clients->next;
							free(todelete);
						}
						
						printf("Deleting client\n");
						node *inode = clients;
						while(inode != NULL)
						{
							if(i != inode->fd)	
							{
								char sermsg[256];
								bzero(sermsg,256);
							
								sprintf(sermsg, "%d Quiting", myclientid);
								printf(sermsg, "notifying %d\n", inode->id);
								//printf("sending %s to %d\n", sermsg, cli_map[j]);
								n = send(inode->fd, sermsg, strlen(sermsg), 0);
								if (n < 0) error("ERROR writing to socket");
							}	
							inode = inode->next;
						}
						sprintf(response, "Notified everyone");
						
						client_number--;							
						FD_CLR(i, &master);
						printf("Client %4d left\n", myclientid);
						break;
					}
					if(buffer[strlen(buffer)-1] == '\n')
						buffer[strlen(buffer)-1] = '\0';
					printf("Client %d sent message: %s\n", mynode->id, buffer);
					
					index = 0;
					while(buffer[index] == ' ' || buffer[index] == '\t' || buffer[index] == '\n')
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
					
					//strcpy(response, evaluatePostfix(buffer));
					token = strtok(buffer, " ");
					if(strcmp(bufcopy, "/active") == 0)
					{
						if(client_number>0)
						{
							printf("Active clients - %d\n", client_number);
							sprintf(response, "Active clients\n");
							
							node *inode = clients;
							while(inode != NULL)
							{
								sprintf(response, "%s%d", response, inode->id);
								if(inode->fd == i)	
									sprintf(response, "%s * This is you", response);
								sprintf(response, "%s\n", response);
								inode = inode->next;
							}
						}
					}
					else if(strcmp(token, "/send") == 0)
					{
						token = strtok(NULL, " ");
						if(token != NULL)
						{
							//printf("%s\n", token);
							receiver = atoi(token);
							sprintf(r_str, "%d", receiver);
							if(strcmp(token, r_str) == 0)
							{
							
								node *inode = clients;
								f = 0;
								while(inode != NULL)
								{
									if(inode->id == receiver)	
									{
										f = 1;
										token = strtok(NULL, "\n");
										if(token != NULL)
										{
											char sermsg[256];
											bzero(sermsg,256);
									
											sprintf(sermsg, "%d said %s", mynode->id, token);
											//printf("sending %s to %d\n", sermsg, cli_map[j]);
											n = send(inode->fd, sermsg, strlen(sermsg)+1, 0);
											if (n < 0) error("ERROR writing to socket");
											else sprintf(response, "Message sent to %d: %s", inode->id, token);
											break;
										}
										else
										{
											sprintf(response, "No message");
											break;
										}
									}
									inode = inode->next;
								}
								if(f == 0)
								{
									sprintf(response, "No such user");
								}
							}
							else
							{
								sprintf(response, "Invalid client ID");
							}	
						}
						else
						{
							sprintf(response, "No receiver ID");
						}
					}
					else if(strcmp(token, "/broadcast") == 0)
					{
						token = strtok(NULL, "");
						node *inode = clients;
						f = 0;
						while(inode != NULL)
						{
							if(i != inode->fd)	
							{
								char sermsg[256];
								bzero(sermsg,256);
							
								sprintf(sermsg, "%d Broadcasted %s", mynode->id, token);
								//printf("sending %s to %d\n", sermsg, cli_map[j]);
								n = send(inode->fd, sermsg, strlen(sermsg)+1, 0);
								if (n < 0) error("ERROR writing to socket");
							}	
							inode = inode->next;
						}
						sprintf(response, "Message broadcasted: %s", token);
					}
					else if(strcmp(token, "/makegroup") == 0)
					{
						printf("Making group\n");
						token = strtok(NULL, " ");
						group *newgroup = (group *)malloc(sizeof(group *));
						newgroup->gid = 10000 + rand() % 90000;
						newgroup->admin = mynode->id;
						newgroup->memcount = 1;
						node *newnode = (node *)malloc(sizeof(node *));
						newnode->id = mynode->id;
						newnode->fd = mynode->fd;
						newnode->cno = mynode->cno;
						newnode->next = NULL;
						newgroup->members = newnode;
						newgroup->addmem = NULL;
						newgroup->memreq = NULL;
						int groupcreated = 1;
						sprintf(response, "");
						newgroup->memcount = 1;
						while(token != NULL)
						{
							printf("Token: %s\n", token);
							if(strlen(token) > 5)
							{
								sprintf(response, "%s: Invalid client ID\n", response, token);
								groupcreated = 0;
								break;
								//break;
							}
							else
							{
								cid = atoi(token);
								sprintf(cidstr, "%d", cid);
								if(strcmp(token, cidstr) == 0)
								{
									printf("CID: %d\n", cid);
									node *currnode = clients;
									while(currnode != NULL)
									{
										printf("currnode->id: %d\n", currnode->id);
										if(currnode->id == cid)	
											break;
										currnode = currnode->next;
									}
									if(currnode != NULL)
									{
										printf("CID found: %d\n", cid);
										node *currgroup = newnode;
										while(currgroup != NULL)
										{
											if(currgroup->id == cid)	
												break;
											currgroup = currgroup->next;
										}
										if(currgroup == NULL)
										{
											if(newgroup->memcount < MAXGROUPSIZE)
											{
												node *tempnode = (node *)malloc(sizeof(node *));
												tempnode->id = currnode->id;
												tempnode->fd = currnode->fd;
												tempnode->cno = currnode->cno;
												tempnode->next = NULL;
												tempnode->next = newnode;
												newnode = tempnode;
												newgroup->memcount++;
												sprintf(response, "%s%d: active client\n", response, cid);
											}
											else
											{
												sprintf(response, "%s Max client in a group exceeded\n", response, token);
												groupcreated = 0;
												break;
											}
										}
										else
										{
											sprintf(response, "%s%d: Ignoring duplicate entry\n", response, cid);
										}
										
									}
									else
									{
										printf("CID not found: %d\n", cid);
										sprintf(response, "%s%s: Client ID doesn't exist currently\n", response, token);
										groupcreated = 0;
										break;
									}		
								}
								else
								{
									sprintf(response, "%s%s: Invalid client ID\n", response, token);
									groupcreated = 0;
									break;
								}
							}
							token = strtok(NULL, " ");
						}
						
						if(groupcreated == 1)
						{
							grouplist *ngl = (grouplist *)malloc(sizeof(grouplist *));
							newgroup->members = newnode;
							ngl->g = *newgroup;
							ngl->next = gl;
							gl = ngl;
							printgroupdetails(*newgroup);
							printf("GID: %d\n", newgroup->gid);
							
							int reqsent = 0;
							node *inode = newgroup->members;
							while(inode != NULL)
							{
								
								if(i != inode->fd)	
								{
									char sermsg[256];
									bzero(sermsg,256);
									printf("sending request to : %d\n", inode->id);
									sprintf(sermsg, "%d added you to group %d", mynode->id, newgroup->gid);
									//printf("sending %s to %d\n", sermsg, cli_map[j]);
									n = send(inode->fd, sermsg, strlen(sermsg), 0);
									if (n < 0) error("ERROR writing to socket");
									reqsent++;
								}	
								inode = inode->next;
							}
							
							printf("GID: %d\n", newgroup->gid);
							sprintf(response, "%sGroup created with admin %d and %d other members: %d\n", response, mynode->id, reqsent, newgroup->gid);
						}
						else
						{
							sprintf(response, "%sGroup not created\n", response);
						}
					}
					else if(strcmp(token, "/sendgroup") == 0)
					{
						token = strtok(NULL, " ");
						if(token != NULL)
						{
							gid = atoi(token);
							sprintf(gidstr, "%d", gid);
							if(strcmp(token, gidstr) == 0)
							{
								grouplist *igl = gl;
								while(igl != NULL)
								{
									if(igl->g.gid = gid)
										break;								
									igl = igl->next;	
								}
								if(igl != NULL)
								{
									node *jnode = igl->g.members;
									f = 0;
									while(jnode != NULL)
									{
										if(i == jnode->fd)	
										{
											f = 1;
											break;
										}	
										jnode = jnode->next;
									}
									if(f == 1)
									{
										token = strtok(NULL, "");
										if(token != NULL)
										{
											int sentmsg = 0;
											node *inode = igl->g.members;
											while(inode != NULL)
											{
												if(i != inode->fd)	
												{
													char sermsg[256];
													bzero(sermsg,256);
							
													sprintf(sermsg, "%d sent in group %d: %s", mynode->id, gid, token);
													//printf("sending %s to %d\n", sermsg, cli_map[j]);
													n = send(inode->fd, sermsg, strlen(sermsg)+1, 0);
													if (n < 0) error("ERROR writing to socket");
													sentmsg++;
												}	
												inode = inode->next;
											}
											if(sentmsg > 0)
												sprintf(response, "Message sent to %d members in group %d: %s", sentmsg, gid, token);
											else
												sprintf(response, "No members in group other than you. ");
										}
										else
											sprintf(response, "Empty message");
									}
									else
										sprintf(response, "%d not member of %d", mynode->id, gid);
									
								}
								else
								{
									sprintf(response, "Group ID %d doesn't exist", gid);
								}	
							}
							else
							{
								sprintf(response, "Invalid group ID");
							}
						}
						else
						{
							sprintf(response, "Empty group ID");
						}
				
					}
					else if(strcmp(token, "/activegroups") == 0)
					{
						int myactivegroups = 0;
						node *inode;
						grouplist *igl = gl;
						sprintf(response, "");
						if(igl != NULL)
						{
							while(igl != NULL)
							{
								inode = igl->g.members;
								while(inode != NULL)
								{
									if(i == inode->fd)	
									{
										myactivegroups++;
										sprintf(response, "%s%d\n", response, igl->g.gid);
										break;
									}	
									inode = inode->next;
								}							
								igl = igl->next;	
							}
						}
						if(myactivegroups>0)
						{
							sprintf(response, "%s%d Active groups of %d\n", response, myactivegroups, mynode->id);
						}
						else
							sprintf(response, "No Active groups of %d\n", mynode->id);
					}
					else if(strcmp(token, "/makegroupreq") == 0)
					{
						printf("Making group\n");
						
						group *newgroup = (group *)malloc(sizeof(group *));
						newgroup->gid = 10000 + rand() % 90000;
						newgroup->admin = mynode->id;
						newgroup->memcount = 1;
						printf("0. newgroup->memcount: %d\n", newgroup->memcount);
						
						node *newnode = NULL;
						/*
						node *newnode = (node *)malloc(sizeof(node *));
						printf("y. newgroup->memcount: %d\n", newgroup->memcount);
						newnode->id = mynode->id;
						printf("x. newgroup->memcount: %d\n", newgroup->memcount);
						newnode->fd = mynode->fd;
						newnode->cno = mynode->cno;
						newnode->next = NULL;
						*/
						node *adminnode = (node *)malloc(sizeof(node *));
						adminnode->id = mynode->id;
						adminnode->fd = mynode->fd;
						adminnode->cno = mynode->cno;
						adminnode->next = NULL;
						newgroup->members = adminnode;
						
						newgroup->addmem = NULL;
						newgroup->memreq = NULL;
						int groupcreated = 1;
						sprintf(response, "");
						token = strtok(NULL, " ");
						printf("1. newgroup->memcount: %d\n", newgroup->memcount);
						newgroup->memcount = 1;
						while(token != NULL)
						{
							printf("Token: %s\n", token);
							printf("2. newgroup->memcount: %d\n", newgroup->memcount);
							if(strlen(token) > 5)
							{
								sprintf(response, "%s: Invalid client ID\n", response, token);
								groupcreated = 0;
								break;
								//break;
							}
							else
							{
								cid = atoi(token);
								sprintf(cidstr, "%d", cid);
								if(strcmp(token, cidstr) == 0)
								{
									printf("CID: %d\n", cid);
									printf("3. newgroup->memcount: %d\n", newgroup->memcount);
									node *currnode = clients;
									while(currnode != NULL)
									{
										printf("currnode->id: %d\n", currnode->id);
										if(currnode->id == cid)	
											break;
										currnode = currnode->next;
									}
									printf("4. newgroup->memcount: %d\n", newgroup->memcount);
									if(currnode != NULL)
									{
										printf("CID found: %d\n", cid);
										node *currgroup = newnode;
										while(currgroup != NULL)
										{
											if(currgroup->id == cid)	
												break;
											currgroup = currgroup->next;
										}
										printf("5. newgroup->memcount: %d", newgroup->memcount);
										if(currgroup == NULL)
										{
											printf("newgroup->memcount: %d", newgroup->memcount);
											if(newgroup->memcount < MAXGROUPSIZE)
											{
												node *tempnode = (node *)malloc(sizeof(node *));
												tempnode->id = currnode->id;
												tempnode->fd = currnode->fd;
												tempnode->cno = currnode->cno;
												tempnode->next = NULL;
												tempnode->next = newnode;
												newnode = tempnode;
												newgroup->memcount = (newgroup->memcount) + 1;
												sprintf(response, "%s%d: active client\n", response, cid);
											}
											else
											{
												sprintf(response, "%s Max client in a group exceeded\n", response, token);
												groupcreated = 0;
												break;
											}
										}
										else
										{
											sprintf(response, "%s%d: Ignoring duplicate entry\n", response, cid);
										}
									}
									else
									{
										printf("CID not found: %d\n", cid);
										sprintf(response, "%s%s: Client ID doesn't exist currently\n", response, token);
										groupcreated = 0;
										break;
									}		
								}
								else
								{
									sprintf(response, "%s%s: Invalid client ID\n", response, token);
									groupcreated = 0;
									break;
								}
							}
							token = strtok(NULL, " ");
						}
						
						if(groupcreated == 1)
						{
							grouplist *ngl = (grouplist *)malloc(sizeof(grouplist *));
							newgroup->addmem = newnode;
							ngl->g = *newgroup;
							ngl->next = reqgl;
							reqgl = ngl;
							printgroupdetails(*newgroup);
							
							int reqsent = 0;
							node *inode = newgroup->addmem;
							while(inode != NULL)
							{
								if(i != inode->fd)	
								{
									char sermsg[256];
									bzero(sermsg,256);
									printf("sending request to : %d\n", inode->id);
									sprintf(sermsg, "%d requested you to group %d", mynode->id, newgroup->gid);
									//printf("sending %s to %d\n", sermsg, cli_map[j]);
									n = send(inode->fd, sermsg, strlen(sermsg), 0);
									if (n < 0) error("ERROR writing to socket");
									reqsent++;
								}	
								inode = inode->next;
							}
							
							printf("GID: %d\n", newgroup->gid);
							sprintf(response, "%sGroup created with admin %d and sent %d requests: %d\n", response, mynode->id, reqsent, newgroup->gid);
						}
						else
						{
							sprintf(response, "%sGroup Req not successful\n", response);
						}
					}
					else if(strcmp(token, "/joingroup") == 0)
					{
						token = strtok(NULL, " ");
						if(token != NULL)
						{
							gid = atoi(token);
							sprintf(gidstr, "%d", gid);
							if(strcmp(token, gidstr) == 0)
							{
								//valid group ID
								grouplist *igl = reqgl;
								grouplist *previgl = NULL;
								while(igl != NULL)
								{
									if(igl->g.gid == gid)
										break;
									previgl = igl;
									igl = igl->next;	
								}
								if(igl != NULL)
								{
									//group found in inactive list
									node *prechecknode = igl->g.addmem;
									//Finding request
									while(prechecknode != NULL)
									{
										printf("Remaining requests = %d\n", prechecknode->id);
										prechecknode = prechecknode->next;
									}
									int memberpre = 0;
									node *inode = igl->g.members;
									while(inode != NULL)
									{
										if(i == inode->fd)
										{
											break;
										}	
										inode = inode->next;
									}
									if(inode == NULL)
									{
										//Not a member of group
										node *jnode = igl->g.addmem;
										node *prevjnode = NULL;
										f = 0;
										//Finding request
										while(jnode != NULL)
										{
											if(i == jnode->fd)
											{
												f = 1;
												break;
											}
											prevjnode = jnode;
											jnode = jnode->next;
										}
									
										//finding admin
										node *admnode = clients;
										while(admnode != NULL)
										{
											if(admnode->id == igl->g.admin)
											{
												break;
											}
											admnode = admnode->next;
										}
									
										printf("f = %d\n", f);
										char sermsg[256];
										bzero(sermsg,256);
										int islast = 0;
										if(f == 1)
										{
											//request found
											if(jnode != NULL)
											{
												//request found
												
												//deleting request
												printf("jnode->id = %d\n", jnode->id);
												if(prevjnode != NULL)
												{
													prevjnode->next = jnode->next;
													printf("prevjnode->id = %d\n", prevjnode->id);
												}
												else
												{
													
													printf("prevjnode->id = NULL\n");
													igl->g.addmem = igl->g.addmem->next;
													
													if(igl->g.addmem == NULL)
													{
														printf("igl->g.addmem = NULL\n");
														islast = 1;
													}
													else
													{
														printf("igl->g.addmem->id = %d\n", igl->g.addmem->id);
													}
												}
												//adding to members
												jnode->next = igl->g.members;
												igl->g.members = jnode;
											
											
	
												sprintf(sermsg, "%d Joined group %d", mynode->id, gid);
												
												
												sprintf(response, "Joined group %d", gid);
												if(islast == 1)
												{
													printf("last node = %d\n", islast);
													grouplist *tomove = igl;
													if(previgl != NULL)
														previgl->next = igl->next;
													else
														reqgl = reqgl->next;
													tomove->next = gl;
													gl = tomove;
													sprintf(response, "%s\nAll requested members responded hence creating group %d", response, gid);
													sprintf(sermsg, "%s\nAll requested members responded hence creating group %d", sermsg, gid);
												}
												
												n = send(admnode->fd, sermsg, strlen(sermsg), 0);
												if (n < 0) error("ERROR writing to socket");
												node *checknode = igl->g.addmem;
												//Finding request
												while(checknode != NULL)
												{
													printf("Remaining requests = %d\n", checknode->id);
													checknode = checknode->next;
												}
											}
											else
											{
												//request not found
												
												node *newnode = (node *)malloc(sizeof(node *));
												newnode->id = mynode->id;
												newnode->fd = mynode->fd;
												newnode->cno = mynode->cno;
												newnode->next = NULL;
											
												if(igl->g.memreq == NULL)
												{
													//request queue is empty
													igl->g.memreq = newnode;
												}
												else
												{
													newnode->next = igl->g.memreq;
													igl->g.memreq = newnode;
												}
												sprintf(sermsg, "%d requested to join group %d", mynode->id, gid);
												n = send(admnode->fd, sermsg, strlen(sermsg), 0);
												if (n < 0) error("ERROR writing to socket");
												sprintf(response, "No request currently hence sending request to admin %d", admnode->id);
											}										
										}
										else
										{
											//request not found
											node *memreqwe = findnodebyID(igl->g.memreq, mynode->id);
											if(memreqwe == NULL)
											{

												sprintf(response, "You are trying to send request to an Inactive group %d", gid);
											}
											else
											{
												sprintf(response, "Request pending for approval by admin: %d", admnode->id);
											}
											
										}
											
									}
									else
									{
										sprintf(response, "You are already member of group %d", gid);
									}
								}
								else
								{
									igl = gl;
									previgl = NULL;
									while(igl != NULL)
									{
										if(igl->g.gid == gid)
											break;
										previgl = igl;
										igl = igl->next;	
									}
									if(igl != NULL)
									{
										//group found in active list
										node *prechecknode = igl->g.addmem;
										//Finding request
										while(prechecknode != NULL)
										{
											printf("Remaining requests = %d\n", prechecknode->id);
											prechecknode = prechecknode->next;
										}
										int memberpre = 0;
										node *inode = igl->g.members;
										while(inode != NULL)
										{
											if(i == inode->fd)
											{
												break;
											}	
											inode = inode->next;
										}
										if(inode == NULL)
										{
											//Not a member of group
											node *jnode = igl->g.addmem;
											node *prevjnode = NULL;
											f = 0;
											//Finding request
											while(jnode != NULL)
											{
												if(i == jnode->fd)
												{
													f = 1;
													break;
												}
												prevjnode = jnode;
												jnode = jnode->next;
											}
									
											//finding admin
											node *admnode = clients;
											while(admnode != NULL)
											{
												if(admnode->id == igl->g.admin)
												{
													break;
												}
												admnode = admnode->next;
											}
									
											printf("f = %d\n", f);
											char sermsg[256];
											bzero(sermsg,256);
											int islast = 0;
											if(f == 1)
											{
												//request found
												if(jnode != NULL)
												{
													//request found
												
													//deleting request
													printf("jnode->id = %d\n", jnode->id);
													if(prevjnode != NULL)
													{
														prevjnode->next = jnode->next;
														printf("prevjnode->id = %d\n", prevjnode->id);
													}
													else
													{
													
														printf("prevjnode->id = NULL\n");
														igl->g.addmem = igl->g.addmem->next;
													
														if(igl->g.addmem == NULL)
														{
															printf("igl->g.addmem = NULL\n");
															islast = 1;
														}
														else
														{
															printf("igl->g.addmem->id = %d\n", igl->g.addmem->id);
														}
													}
													//adding to members
													jnode->next = igl->g.members;
													igl->g.members = jnode;
											
											
	
													sprintf(sermsg, "%d Joined group %d", mynode->id, gid);
												
												
													sprintf(response, "Joined group %d", gid);
													if(islast == 1)
													{
														printf("last node = %d\n", islast);
														grouplist *tomove = igl;
														if(previgl != NULL)
															previgl->next = igl->next;
														else
															reqgl = reqgl->next;
														tomove->next = gl;
														gl = tomove;
														sprintf(response, "%s\nAll requested members responded hence creating group %d", response, gid);
														sprintf(sermsg, "%s\nAll requested members responded hence creating group %d", sermsg, gid);
													}
												
													n = send(admnode->fd, sermsg, strlen(sermsg), 0);
													if (n < 0) error("ERROR writing to socket");
													node *checknode = igl->g.addmem;
													//Finding request
													while(checknode != NULL)
													{
														printf("Remaining requests = %d\n", checknode->id);
														checknode = checknode->next;
													}
												}
												else
												{
													//request not found
												
													node *newnode = (node *)malloc(sizeof(node *));
													newnode->id = mynode->id;
													newnode->fd = mynode->fd;
													newnode->cno = mynode->cno;
													newnode->next = NULL;
											
													if(igl->g.memreq == NULL)
													{
														//request queue is empty
														igl->g.memreq = newnode;
													}
													else
													{
														newnode->next = igl->g.memreq;
														igl->g.memreq = newnode;
													}
													sprintf(sermsg, "%d requested to join group %d", mynode->id, gid);
													n = send(admnode->fd, sermsg, strlen(sermsg), 0);
													if (n < 0) error("ERROR writing to socket");
													sprintf(response, "No request currently hence sending request to admin %d", admnode->id);
												}										
											}
											else
											{
												//request not found
												node *memreqwe = findnodebyID(igl->g.memreq, mynode->id);
												if(memreqwe == NULL)
												{
													node *newnode = (node *)malloc(sizeof(node *));
													newnode->id = mynode->id;
													newnode->fd = mynode->fd;
													newnode->cno = mynode->cno;
													newnode->next = NULL;
										
													if(igl->g.memreq == NULL)
													{
														igl->g.memreq = newnode;
													}
													else
													{
														newnode->next = igl->g.memreq;
														igl->g.memreq = newnode;
													}
													sprintf(sermsg, "%d requested to join group %d", mynode->id, gid);
													n = send(admnode->fd, sermsg, strlen(sermsg), 0);
													if (n < 0) error("ERROR writing to socket");
													sprintf(response, "No request currently hence sending request to admin %d", admnode->id);
												}
												else
												{
													sprintf(response, "Request pending for approval by admin: %d", admnode->id);
												}
											
											}
											
										}
										else
										{
											sprintf(response, "You are already member of group %d", gid);
										}
									}
									else
									{
										sprintf(response, "Group ID %d doesn't exist", gid);
									}
								}
							}
							else
							{
								sprintf(response, "Invalid group ID");
							}
						}
						else
						{
							sprintf(response, "Empty group ID");
						}
					}
					else if(strcmp(token, "/declinegroup") == 0)
					{
						token = strtok(NULL, " ");
						if(token != NULL)
						{
							gid = atoi(token);
							sprintf(gidstr, "%d", gid);
							if(strcmp(token, gidstr) == 0)
							{
								//valid group ID
								grouplist *igl = reqgl;
								grouplist *previgl = NULL;
								while(igl != NULL)
								{
									if(igl->g.gid == gid)
										break;
									previgl = igl;
									igl = igl->next;	
								}
								if(igl != NULL)
								{
									//group found
									node *prechecknode = igl->g.addmem;
									//Finding request
									while(prechecknode != NULL)
									{
										printf("Remaining requests = %d\n", prechecknode->id);
										prechecknode = prechecknode->next;
									}
									int memberpre = 0;
									node *inode = igl->g.members;
									while(inode != NULL)
									{
										if(i == inode->fd)
										{
											break;
										}	
										inode = inode->next;
									}
									if(inode == NULL)
									{
										//Not a member of group
										node *jnode = igl->g.addmem;
										node *prevjnode = NULL;
										f = 0;
										//Finding request
										while(jnode != NULL)
										{
											if(i == jnode->fd)
											{
												f = 1;
												break;
											}
											prevjnode = jnode;
											jnode = jnode->next;
										}
									
										//finding admin
										node *admnode = clients;
										while(admnode != NULL)
										{
											if(admnode->id == igl->g.admin)
											{
												break;
											}
											admnode = admnode->next;
										}
									
										printf("f = %d\n", f);
										char sermsg[256];
										bzero(sermsg,256);
										int islast = 0;
										if(f == 1)
										{
											//request found
											if(jnode != NULL)
											{
												//request found
												
												//deleting request
												printf("jnode->id = %d\n", jnode->id);
												if(prevjnode != NULL)
												{
													prevjnode->next = jnode->next;
													printf("prevjnode->id = %d\n", prevjnode->id);
												}
												else
												{
													
													printf("prevjnode->id = NULL\n");
													igl->g.addmem = igl->g.addmem->next;
													
													if(igl->g.addmem == NULL)
													{
														printf("igl->g.addmem = NULL\n");
														islast = 1;
													}
													else
													{
														printf("igl->g.addmem->id = %d\n", igl->g.addmem->id);
													}
												}
												//adding to members
												/*
												jnode->next = igl->g.members;
												igl->g.members = jnode;
												*/
											
	
												
												sprintf(sermsg, "%d declined group %d", mynode->id, gid);
												sprintf(response, "Declined group %d", gid);
												if(islast == 1)
												{
													printf("last node = %d\n", islast);
													grouplist *tomove = igl;
													if(previgl != NULL)
														previgl->next = igl->next;
													else
														reqgl = reqgl->next;
													tomove->next = gl;
													gl = tomove;
													sprintf(sermsg, "%s\nAll requested members responded hence creating group %d", sermsg, gid);
													if (n < 0) error("ERROR writing to socket");
													sprintf(response, "%s\nAll requested members responded hence creating group %d", response, gid);
												}
												
												
												n = send(admnode->fd, sermsg, strlen(sermsg), 0);
												if (n < 0) error("ERROR writing to socket");
												
												node *checknode = igl->g.addmem;
												//Finding request
												while(checknode != NULL)
												{
													printf("Remaining requests = %d\n", checknode->id);
													checknode = checknode->next;
												}
											}
											else
											{
												//request not found
												
												node *newnode = (node *)malloc(sizeof(node *));
												newnode->id = mynode->id;
												newnode->fd = mynode->fd;
												newnode->cno = mynode->cno;
												newnode->next = NULL;
											
												if(igl->g.memreq == NULL)
												{
													//request queue is empty
													igl->g.memreq = newnode;
												}
												else
												{
													newnode->next = igl->g.memreq;
													igl->g.memreq = newnode;
												}
												sprintf(sermsg, "%d requested to join group %d", mynode->id, gid);
												n = send(admnode->fd, sermsg, strlen(sermsg), 0);
												if (n < 0) error("ERROR writing to socket");
												sprintf(response, "No request currently hence sending request to admin %d", admnode->id);
											}										
										}
										else
										{
											//request not found
											node *memreqwe = findnodebyID(igl->g.memreq, mynode->id);
											if(memreqwe == NULL)
											{
												node *newnode = (node *)malloc(sizeof(node *));
												newnode->id = mynode->id;
												newnode->fd = mynode->fd;
												newnode->cno = mynode->cno;
												newnode->next = NULL;
										
												if(igl->g.memreq == NULL)
												{
													igl->g.memreq = newnode;
												}
												else
												{
													newnode->next = igl->g.memreq;
													igl->g.memreq = newnode;
												}
												sprintf(sermsg, "%d requested to join group %d", mynode->id, gid);
												n = send(admnode->fd, sermsg, strlen(sermsg), 0);
												if (n < 0) error("ERROR writing to socket");
												sprintf(response, "No request currently hence sending request to admin %d", admnode->id);
											}
											else
											{
												sprintf(response, "Request pending for approval by admin: %d", admnode->id);
											}
											
										}
											
									}
									else
									{
										sprintf(response, "You are already member of group %d", gid);
									}
								}
								else
								{
									sprintf(response, "Group ID %d doesn't exist", gid);
								}	
							}
							else
							{
								sprintf(response, "Invalid group ID");
							}
						}
						else
						{
							sprintf(response, "Empty group ID");
						}
					}
					else if(strcmp(token, "/quit") == 0)
					{
						//client_active = 0;
						grouplist *igl = gl;
						grouplist *prevgl = NULL;
						printf("Removing client from groups\n");
						while(igl != NULL)
						{
							printf("checking group ID: %d\n", igl->g.gid);
							printgroupdetails(igl->g);
							if(igl->g.admin == mynode->id)
							{
								grouplist *todelete;
								printf("%d admin of %d\n", mynode->id, igl->g.gid);
								if(prevgl != NULL)
								{
									todelete = igl;
									prevgl->next = igl->next;
									igl = prevgl;
									//free(todelete);
								}
								else
								{
									todelete = gl;
									gl = gl->next;	
									prevgl = NULL;								
								}
								igl = igl->next;
								//free(todelete);
								continue;
							}
							
							printf("checking members\n");
							node *inode = igl->g.members;
							if(inode->fd == i)
							{
								node *todelete = igl->g.members;
								igl->g.members = igl->g.members->next;
								free(todelete);
							}
							else
							{
								while(inode->next != NULL)
								{
									if(inode->next->fd == i)
									{
										node *todelete = inode->next;
										inode->next = inode->next->next;
										free(todelete);
										break;
									}
									inode = inode->next;
								}	
							}
							
							printf("checking addmem\n");
							inode = igl->g.addmem;
							if(inode != NULL)
							{
								if(inode->fd == i)
								{
									node *todelete = igl->g.addmem;
									igl->g.addmem = igl->g.addmem->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							
							printf("checking memreq\n");
							inode = igl->g.memreq;
							if(inode != NULL)
							{
								printf("memreq not NULL\n");
								if(inode->fd == i)
								{
									node *todelete = igl->g.memreq;
									igl->g.memreq = igl->g.memreq->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							printf("CHECKED group ID: %d\n", igl->g.gid);
							prevgl = igl;
							igl = igl->next;	
						}
						
						
						igl = reqgl;
						prevgl = NULL;
						printf("Removing client from inactive groups\n");
						while(igl != NULL)
						{
							printf("checking group ID: %d\n", igl->g.gid);
							printgroupdetails(igl->g);
							if(igl->g.admin == mynode->id)
							{
								grouplist *todelete;
								printf("%d admin of %d\n", mynode->id, igl->g.gid);
								if(prevgl != NULL)
								{
									todelete = igl;
									prevgl->next = igl->next;
									igl = prevgl;
									//free(todelete);
								}
								else
								{
									todelete = gl;
									reqgl = reqgl->next;	
									prevgl = NULL;								
								}
								igl = igl->next;
								//free(todelete);
								continue;
							}
							
							printf("checking members\n");
							node *inode = igl->g.members;
							if(inode->fd == i)
							{
								node *todelete = igl->g.members;
								igl->g.members = igl->g.members->next;
								free(todelete);
							}
							else
							{
								while(inode->next != NULL)
								{
									if(inode->next->fd == i)
									{
										node *todelete = inode->next;
										inode->next = inode->next->next;
										free(todelete);
										break;
									}
									inode = inode->next;
								}	
							}
							
							printf("checking addmem\n");
							inode = igl->g.addmem;
							if(inode != NULL)
							{
								if(inode->fd == i)
								{
									node *todelete = igl->g.addmem;
									igl->g.addmem = igl->g.addmem->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							
							printf("checking memreq\n");
							inode = igl->g.memreq;
							if(inode != NULL)
							{
								printf("memreq not NULL\n");
								if(inode->fd == i)
								{
									node *todelete = igl->g.memreq;
									igl->g.memreq = igl->g.memreq->next;
									free(todelete);
								}
								else
								{
									while(inode->next != NULL)
									{
										if(inode->next->fd == i)
										{
											node *todelete = inode->next;
											inode->next = inode->next->next;
											free(todelete);
											break;
										}
										inode = inode->next;
									}	
								}
							}
							printf("CHECKED group ID: %d\n", igl->g.gid);
							prevgl = igl;
							igl = igl->next;	
						}
						
												
						int myclientid = mynode->id;
						//deleting client
						printf("Deleting client\n");
						if(myprevnode != NULL)
						{
							node *todelete = mynode;
							myprevnode->next = mynode->next;
							free(todelete);
						}
						else
						{
							node *todelete = mynode;
							clients = clients->next;
							free(todelete);
						}
						
						printf("Deleting client\n");
						node *inode = clients;
						while(inode != NULL)
						{
							if(i != inode->fd)	
							{
								char sermsg[256];
								bzero(sermsg,256);
							
								sprintf(sermsg, "%d Quiting", myclientid);
								printf("notifying %d\n", inode->id);
								n = send(inode->fd, sermsg, strlen(sermsg), 0);
								if (n < 0) error("ERROR writing to socket");
							}	
							inode = inode->next;
						}
						sprintf(response, "Notified everyone");
						
						client_number--;							
						FD_CLR(i, &master);
						printf("Client %4d left\n", mynode->id);
						//break;
					}
					else if(strcmp(token, "/activeallgroups") == 0)
					{
						grouplist *igl = gl;
						if(igl != NULL)
						{
							sprintf(response, "Active groups\n");
							while(igl != NULL)
							{
							
								sprintf(response, "%s%d\n", response, igl->g.gid);							
								igl = igl->next;	
							}
						}
						else
							sprintf(response, "No Active groups\n");
					}
					else if(strcmp(token, "/addmember") == 0)
					{
						token = strtok(NULL, " ");
						int userid = -1;
						char uidstr[10];
						if(token != NULL)
						{
							gid = atoi(token);
							sprintf(gidstr, "%d", gid);
							char *cltoadd = strtok(NULL, " ");
							if(cltoadd != NULL)
							{
								if(strcmp(token, gidstr) == 0)
								{
									grouplist *igl = gl;
									while(igl != NULL)
									{
										if(igl->g.gid == gid)
											break;								
										igl = igl->next;	
									}
									if(igl != NULL)
									{	//group found
										if(igl->g.admin == mynode->id)
										{//current client is admin
											//token = strtok(NULL, " ");
											strcpy(token, cltoadd);
											if(token != NULL)
											{
												//printf("%s\n", token);
												//
												receiver = atoi(token);
												sprintf(r_str, "%d", receiver);
												if(strcmp(token, r_str) == 0)
												{
						
													node *inode = clients;
													f = 0;
													while(inode != NULL)
													{
														if(inode->id == receiver)	
														{
															f = 1;
															break;
														}
														inode = inode->next;
													}
													if(f == 0)
													{
														sprintf(response, "No such user");
													}
													else
													{
														//user found inode
														int memberpre = 0;
														node *mnode = igl->g.members;
														//checking members
														while(mnode != NULL)
														{
															if(receiver == mnode->id)	
															{
																break;
															}	
															mnode = mnode->next;
														}
														if(mnode == NULL)
														{//not a member
															node *memreq = igl->g.memreq;
															node *prevmemreq = NULL;
															while(memreq != NULL)
															{//searching request
																if(receiver == memreq->id)	
																{
																	break;
																}
																prevmemreq = memreq;
																memreq = memreq->next;
															}
															node *newnode = (node *)malloc(sizeof(node *));
															newnode->id = inode->id;
															newnode->fd = inode->fd;
															newnode->cno = inode->cno;
															newnode->next = NULL;
															if(memreq == NULL)
															{//request not found hnece sending
																
																node *addmem = findnodebyID(igl->g.addmem, receiver);
																if(addmem == NULL)
																{
																	sprintf(response, "No request from %d to group %d\nhence sending request to user", receiver, gid);
																	char sermsg[256];
																	bzero(sermsg,256);
		
																	sprintf(sermsg, "%d requested you to group %d", mynode->id, gid);
																	n = send(inode->fd, sermsg, strlen(sermsg), 0);
																	if (n < 0) error("ERROR writing to socket");
														
																	if(igl->g.addmem == NULL)
																	{
																		igl->g.addmem = newnode;
																	}
																	else
																	{
																		newnode->next = igl->g.addmem;
																		igl->g.addmem = newnode;
																	}
																}
																else
																{
																	sprintf(response, "REquest is already pending with %d for group %d", receiver, gid);
																}
																
																
															}
															else
															{
																//Request found
																if(prevmemreq == NULL)
																{
																	igl->g.memreq = igl->g.memreq->next;
															
																}
																else
																{
																	prevmemreq->next = memreq->next;
															
																}
																if(igl->g.members == NULL)
																{
																	igl->g.members = newnode;
																}
																else
																{
																	newnode->next = igl->g.members;
																	igl->g.members = newnode;
																}
																sprintf(response, "%d added to group %d", receiver, gid);
																char sermsg[256];
																bzero(sermsg,256);
		
																sprintf(sermsg, "you request group %d is approved by %d", gid, mynode->id);
																n = send(inode->fd, sermsg, strlen(sermsg), 0);
																if (n < 0) error("ERROR writing to socket");
															}	
														}
														else
														{
															sprintf(response, "%d is already member of group %d", receiver, gid);
														}
												
													}
												}
												else
												{
													sprintf(response, "Invalid client ID");
												}	
											}
											else
											{
												sprintf(response, "No member ID");
											}
										}
										else
										{
											sprintf(response, "You are not admin of group %d", gid);
										}
								
									}
									else
									{
										sprintf(response, "Group ID %d doesn't exist", gid);
									}	
								}
								else
								{
									sprintf(response, "Invalid group ID");
								}
							}
							else
							{
								sprintf(response, "Empty member ID");
							}
						}
						else
						{
							sprintf(response, "Empty group ID");
						}
					}
					else
					{
						sprintf(response, "Invalid command");
					}
					
					printf("Sending result: %s\n", response);
					if(client_active == 1)
					{
						n = write(i, response, strlen(response));
						printf("Sent result\n");
						if (n < 0) error("ERROR writing to socket");
						if(strcmp(response, "Notified everyone") == 0)
						{
							close(i);
							client_number--;
						}	
					}
					
					fseek(fptr, 0, SEEK_END);
					printf("logs: %5d\t%25s\t%15s\n\n", mynode->id, bufcopy, response);
					fprintf(fptr, "%5d\t%25s\t%15s\n", mynode->id, bufcopy, response);
					fflush(fptr);
					
				}
			}
		}
		
	}
	fclose(fptr);
	return 0; 
}
