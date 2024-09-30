    #include <unistd.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <sys/types.h>
	#include <errno.h>
	#include <fcntl.h>
	#include <limits.h> //PATH_MAX
	#include <pty.h> //openpty()
	#include <stdbool.h> //booli
	#include <fcntl.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <poll.h>
	#include <sys/eventfd.h>
	#define LEN 11

struct argStruct
{
	int ptyFD;
	int eventFD;
};

void *pollFD(void *str);
void *readFunc(void *fd);
void *listenFunc(void *pack);

bool redirectStdout(int pts);

int main(int argc, char** argv)
{
	if(argc!=4)
	{
        printf("not valid amount of arguments!!\n");
		printf("parent [max children] [pseudoterminal number] [pipe/pty]\n");
	    exit(EXIT_FAILURE);
	}
	
	bool ptyMode;
	
	if(strcmp(argv[3], "pipe") == 0)
	{
		ptyMode=false;
	}
	else
	if(strcmp(argv[3], "pty") == 0)
	{
		ptyMode=true;
	}		

	printf("pseudoTerminalMode=%d\n",ptyMode);
	
	char absolute_path_child[PATH_MAX];
	realpath("../child/child",absolute_path_child);
	printf("child process path=%s\n",absolute_path_child);
	
	int max_children;
	sscanf(argv[1],"%d",&max_children);
	printf("parent will spawn %d children\n",max_children);

	int target_pty;
	sscanf(argv[2],"%d",&target_pty);

	// char pty_path[20];
	// snprintf(pty_path,20,"/dev/pts/%d",target_pty);

	// printf("target pty=%s\n",pty_path);

	char numb[LEN];
	char *args[]={absolute_path_child,NULL,NULL};

	int null_fd=open("/dev/null",O_WRONLY);
	dup2(null_fd,STDIN_FILENO);
	close(null_fd);


    int IPCfd[2];

	if(ptyMode)
	{
		char ptyName[PATH_MAX];
        if(openpty(&IPCfd[0],&IPCfd[1],ptyName,NULL,NULL))
        {
            perror("openpty");
            exit(EXIT_FAILURE);
        }
 	    printf("pty name=%s\n",ptyName);
		printf("master pty=%d, slave pty=%d\n",IPCfd[0],IPCfd[1]);
	}
	else
	{
		if(pipe(IPCfd)==-1)
		{
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		
	//	int bufferSize=fcntl(IPCfd[0],F_GETPIPE_SZ);
	//	if(bufferSize == -1)
	//	{
	//		printf("Pipe buffer size=%d",bufferSize);
	//	}

		printf("read pipe end=%d, write pipe end=%d\n",IPCfd[0],IPCfd[1]);

	}
	
	printf("parent pid=%d\n",getpid());

	for(int child_nr=0;child_nr<max_children;child_nr++)
	{
    	pid_t pid=fork();
    
		switch(pid)
		{
		case -1:
			perror("fork");
			exit(1);
		case 0:
			printf("child %d after fork pid=%d\n",child_nr+1,getpid());
			
			//close read end of pipe
			//close master side of pty
			close(IPCfd[0]);
		
			snprintf(numb,LEN,"%d",child_nr+1);
			args[1]=numb;
		
			//duplicate pipe/pty
			dup2(IPCfd[1],STDOUT_FILENO);
			dup2(IPCfd[1],STDERR_FILENO);
			close(IPCfd[1]);

			execv(args[0],args);

		default:
			break;
		}	
	}

	
	//close slave side of pty
	//close write pipe end
	close(IPCfd[1]);

	if(!redirectStdout(target_pty))
	{
		perror("redirectStdout");
		exit(EXIT_FAILURE);
	}

	// int pts_fd=open(pty_path,O_WRONLY);
	// if(pts_fd==-1)
	// {
	// 	perror("open pts_fd");
	// 	exit(EXIT_FAILURE);
	// }
	// dup2(pts_fd,STDOUT_FILENO);
	// dup2(pts_fd,STDERR_FILENO);
	// close(pts_fd);


//    int counter=0;
//    while(1)
//    {
//    	printf("counter=%d\n",counter);
//		counter++;
//		sleep(5);
//    }

	struct argStruct fdArgs;
	fdArgs.ptyFD = IPCfd[0];
	fdArgs.eventFD = eventfd(0,0);

	pthread_t thread1,thread2;

	pthread_create(&thread1,NULL,listenFunc,(void*) &fdArgs.eventFD);

	pthread_create(&thread2,NULL,pollFD,(void*) &fdArgs);
	
	//printf("waits for read data in %s\n",ptyMode ? "pty" : "pipe");
	//pthread_create(&thread1,NULL,readFunc,(void*) IPCfd[0]);

	//pthread_join(thread1,NULL);	
	pthread_join(thread1,NULL);
	pthread_join(thread2,NULL);
    return 0;
}

void *readFunc(void *fd)
{
	char buffer[1024];
	memset(buffer,0x00,1024);
	
	int len;
	while(1)
	{
		len=read((int)fd,buffer,sizeof(buffer));
		printf("received %d bytes:\n",len);
		//buffer[n]='\n';
		printf("%.*s",len,buffer);
	}
}

void *pollFD(void *str)
{
	struct argStruct *args = str;
	struct pollfd *pFDs;
	nfds_t nFDs=2; //two fd pty and eventfd		

	char buffer[1024];
	memset(buffer,0x00,1024);

	uint64_t counter = 0;

	pFDs = calloc(nFDs,sizeof(struct pollfd));
	
	/*********PTYFD************/
	/*************************/
	pFDs[0].fd=args->ptyFD;
	pFDs[0].events = POLLIN;
	

	/*********EVENTFD**********/
	/*************************/
	pFDs[1].fd=args->eventFD;
	pFDs[1].events = POLLIN;
	
	int resPoll;
	while(1)
	{
		resPoll=poll(pFDs,nFDs,-1);
		if(resPoll == -1)
		{
			perror("poll");
			exit(EXIT_FAILURE);
		}
		
		for(nfds_t i = 0; i < nFDs; i++)
		{
			printf("  fd=%d; events: %s%s%s\n", pFDs[i].fd,
   			(pFDs[i].revents & POLLIN)  ? "POLLIN "  : "",
            (pFDs[i].revents & POLLHUP) ? "POLLHUP " : "",
            (pFDs[i].revents & POLLERR) ? "POLLERR " : "");

			if(pFDs[i].revents & POLLIN)
			{	
				int len;
				// request from pseudoterminal
				if(pFDs[i].fd == args->ptyFD)
				{
					if (counter < 11)
					{
						len = read((int)pFDs[i].fd,buffer,sizeof(buffer));
						printf("received %d bytes:\n",len);
						printf("%.*s",len,buffer);
					}
					else
					{
						//discard data received but not read
						if(tcflush(pFDs[i].fd,TCIFLUSH)==0)
							printf("buffer flushed\n");
					}
				}
				else
				if (pFDs[i].fd == args->eventFD)
				{
					len = read((int)pFDs[i].fd,&counter,sizeof(counter));
					printf("received %d bytes:\n",len);
					printf("%llu\n",(unsigned long long)counter);
					
					redirectStdout(counter>10 ? -1 : (int)counter);

					// if(counter >10)
					// {
					// 	pFDs[0].events=0;
					// }
					// else
					// {
					// 	if(pFDs[0].events ==0)
					// 	{
					// 		printf("restore to pollin\n");
					// 		pFDs[0].events = POLLIN;
					// 	}
					// }
				}
						
				resPoll-=1;
				if(resPoll == 0)
				{
					break; //not more requests
				}
			}
		}
	}
}
void *listenFunc(void *eventfd)
{	
	int port=0;
	int socketFD;
	char buffer[1024];
	struct sockaddr_in servAddr,cliAddr;

	memset(buffer,0x00,1024);
				
	socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if(socketFD < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	memset(&servAddr,0x00,sizeof(servAddr));
	memset(&cliAddr,0x00,sizeof(cliAddr));

	servAddr.sin_family = AF_INET; //IPv4
	servAddr.sin_addr.s_addr = INADDR_ANY; //all interfaces
	servAddr.sin_port = htons(port); //system assigns port

	if(bind(socketFD,(const struct sockaddr*) &servAddr, sizeof(servAddr)) <0)
	{
		perror("bind");
		close(socketFD);
		exit(EXIT_FAILURE);
	}

	socklen_t servLen=sizeof(servAddr);
	socklen_t cliLen=sizeof(cliAddr);

	if (getsockname(socketFD, (struct sockaddr*)&servAddr, &servLen) == -1) 
	{
        perror("getsockname");
        close(socketFD);
        exit(EXIT_FAILURE);
    }

	port = ntohs(servAddr.sin_port);

	printf("UDP server is listening on port %d\n", port);	

	int n;

	while(1)
	{
		n = recvfrom(socketFD, (char *)buffer, sizeof(buffer),  0, ( struct sockaddr *) &cliAddr, &cliLen);
		if(n == -1)
		{
			perror("recvfrom");
			close(socketFD);
        	exit(EXIT_FAILURE);
		}
		printf("%.*s",n,buffer);

		int fd = *(int*)eventfd;

		uint64_t value_to_write = atoi(buffer);
		int res=write(fd,&value_to_write,sizeof(value_to_write));

		printf("write to fd %d with result %d\n",fd,res);
	}
}

bool redirectStdout(int pts)
{
	printf("current tty STDOUT %s ",ttyname(STDOUT_FILENO));

	char pty_path[20];

	const char *format;
	format=(pts ==-1) ? "/dev/null" : "/dev/pts/%d";

	snprintf(pty_path,20,format,pts);
	
	printf("Try to open=%s\n",pty_path);

	int pts_fd=open(pty_path,O_WRONLY);
	if(pts_fd==-1)
	{
		perror(pty_path);
		return false;
	}
	dup2(pts_fd,STDOUT_FILENO);
	dup2(pts_fd,STDERR_FILENO);
	close(pts_fd);

    return true;
}
