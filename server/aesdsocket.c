#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BACKLOG (10)
#define PORT "9000"
#define MY_MAX_SIZE 500

struct addrinfo *p;
int socketfd;
int new_fd;
int fd;


void handler()
{
	printf("\ncaught signal, exiting");
	close(fd);
	close(socketfd);
	close(new_fd);	
	freeaddrinfo(p);
	remove("/var/tmp/aesdsocketdata.txt");

}

int main()
{
	struct addrinfo hints;
	struct addrinfo *res;
	//clear the structure instance
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;	//any protocol: IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;	//TCP
	hints.ai_flags = AI_PASSIVE;    //assign address

	//starting the connection with the client using the series of functions
	if(getaddrinfo(NULL, PORT, &hints, &res) != 0)
	{
		perror("\ngetaddrinfo");
		return -1;
	}	

	//converting the IP address into string
	char ipstr[30];
	for(p = res; p != NULL; p = res->ai_next)
	{
		void *addr;
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
    	addr = &(ipv4->sin_addr);
		inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
	}

	//calling the socket function
	//int socketfd;

	if((socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
	{
		perror("\nsocket");
		return -1;
	}

	//bind to a connection
	if(bind(socketfd, res->ai_addr, res->ai_addrlen) != 0)
	{
		perror("\nbind");
		return -1;
	}

	//listen to a connection request from a client
	if(listen(socketfd, BACKLOG) == -1)
	{
		perror("\nlisten");
		return -1;	
	}

	freeaddrinfo(res);

	//accept the connection with the client
	struct sockaddr_storage client_addr;	
	socklen_t addr_size = sizeof(client_addr);
	//int new_fd;

	//allocating memory to the buffers to be used
	//char *buf = (char *) malloc(MY_MAX_SIZE);
	char *rem_buf = (char *) malloc(MY_MAX_SIZE);
	
	int i;
	//int fd;

	/*********************************************************************
	The loop receives, writes to the file, reads from the file, and sends
	the data back to the client. This goes on untill SIGINT signal is not
	given by the user.
	**********************************************************************/
	signal(SIGINT, handler);
	signal(SIGTERM, handler);
	while(1)
	{
		char *buf = (char *) malloc(MY_MAX_SIZE);
		char *send_buf = (char *) malloc(MY_MAX_SIZE);
		//accept the connection from the client
		if((new_fd = accept(socketfd, (struct sockaddr *)&client_addr, &addr_size)) == -1 )
		{
			perror("\naccept");
			return -1;	
		}
		else
		{
			printf("Connected with the IP: ");
			puts(ipstr);
		}

		//remove("/var/tmp/aesdsocketdata.txt");
		//open the file
		fd = open("/var/tmp/aesdsocketdata.txt", O_RDWR | O_CREAT | O_APPEND, 0777);
		if(fd == -1)
		{
			perror("\nfile open");
			return(-1);
		} 
		
		//receive from the client
		int rc, count=0;
		if((rc=recv(new_fd, buf, MY_MAX_SIZE, 0)) == -1)
			{
				perror("\nreceive");	
				close(new_fd);
				close(socketfd);
				return -1;
			}
		else
		{
			printf("\nrc: %d\n, received buffers: ", rc);
			puts(buf);
		}

		//search for the \n character (if present, write, else reallocate the buffer extra size)
		int n=1;
		for(i=0; i<MY_MAX_SIZE; i++)
		{
			if(*(buf + (MY_MAX_SIZE*(n-1)) + i) == '\n')
			{
				//write to the file
				if((write(fd, buf, (MY_MAX_SIZE*(n-1))+i+1)) == -1)
				{
					perror("\nwrite");
					close(new_fd);
					close(socketfd);
					return -1;
				}
				else
				{
					printf("\ncontent written to file:\n");
					puts(buf);
					//write(fd, "\n", 1); 
					break;
				}
			}
			else
			{
				if(i == (MY_MAX_SIZE-1))
				{
					printf("\n\nbuffer full, reloading the buffer\n\n");
					n++;
					i=0;
					buf = realloc(buf, (MY_MAX_SIZE*n));
					if((rc=recv(new_fd, buf+(MY_MAX_SIZE*(n-1)), MY_MAX_SIZE, 0)) == -1)
					{
						perror("\nreceive");	
						close(new_fd);
						close(socketfd);
						return -1;
					}
					else
					{
						printf("\nrc: %d\n, received buffers: \n", rc);
						puts(buf);
					}
				}
			}
		}
			//strcpy(rem_buf, buf+i+1);

		//read from the file to send back the content
		off_t file_size = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		int rd,sd;

		/************************************************
		if file size < MAX buf size
			read the whole file size
		else if the file size > MAx buf size
			read MAX buf size at a time until EOF is reached
		**************************************************/
		while(1)
		{
			if(file_size <= MY_MAX_SIZE)
			{
				//read the data
				rd = read(fd, send_buf, file_size);
				//send the data
				sd = send(new_fd, send_buf, file_size, 0);
				break;
			}
			else if(file_size > MY_MAX_SIZE)
			{	
				//printf("\nwhen file size is greater than max size\n");
				//read the data
				rd = read(fd, send_buf, MY_MAX_SIZE);
				if(rd == 0)
					break;
				//send the data
				if(rd < MY_MAX_SIZE)
					sd = send(new_fd, send_buf, rd, 0);
				else
					sd = send(new_fd, send_buf, MY_MAX_SIZE, 0);
				printf("\ncontents send:\n");
				puts(send_buf);
			}
		}		
		
		free(buf);
		free(send_buf);
	}
	//remove("/var/tmp/aesdsocketdata.txt");


	return 0;
}
