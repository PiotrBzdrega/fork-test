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
	#include <dirent.h>
	#include <sys/inotify.h>
	#include <bits/pthreadtypes.h>
	#include <linux/limits.h>
	#include <sys/timerfd.h>
	// #include <linux/time.h>
	// #include <linux/time.h>
	#define LEN 11

struct ptyStruct
{
	int pty;
	int fd;
};

struct argStruct
{
	int ptyFD;
	int eventFD;
	struct ptyStruct ptsFD[5];
	int timerFD;
};



void *pollFD(void *str);
void *readFunc(void *fd);
void *listenFunc(void *pack);

bool redirectStdout(int pts);

int availablePts(struct ptyStruct ptss[5], int len);
void appendPts(struct ptyStruct ptss[5], int len, int num, bool init);
bool removePts(struct ptyStruct ptss[5], int len, int pty);
void findNotAssignedPts(struct ptyStruct ptss[5], int len);

int main(int argc, char** argv)
{
	if(argc!=4)
	{
        printf("not valid amount of arguments!!\n");
		for (size_t i = 0; i < argc; i++)
		{
			printf("%s\n",argv[argc]);
		}
		
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

	//max amount of pts's
	int ptssLen = 5;
	struct ptyStruct ptss[5];
	for (size_t i = 0; i < ptssLen; i++)
	{
		ptss[i].pty = -1;
		ptss[i].fd = -1;
	}
	
	if(availablePts(ptss,ptssLen) != 0)
	{
		perror("availablePts");
		exit(EXIT_FAILURE);
	}

	//create pipe or pseudoterminal
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

		if (target_pty == -1)
		{
			char r[10];
			// extract number from /dev/pts/8
    		int ret=sscanf(ptyName, "%9[/devpts] %d", r, &target_pty);
    		printf("prefix=%s \t pts=%d\n",r,target_pty);
		}
		
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

	for (size_t i = 0; i < ptssLen; i++)
	{
		printf("%d\n",ptss[i].pty);
		if (ptss[i].pty != -1)
		{
			char pty_path[20];
			snprintf(pty_path,20,"/dev/pts/%d",ptss[i].pty);

			ptss[i].fd=open(pty_path,O_WRONLY | O_NONBLOCK);
			if(ptss[i].fd==-1)
			{		
				perror(pty_path);
				return false;
			}
		}
	}	

	//close slave side of pty
	//close write pipe end
	close(IPCfd[1]);


	// target_pty=-1;
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
	//array of all opened ptss
	for (size_t i = 0; i < ptssLen; i++)
	{	
		fdArgs.ptsFD[i] = ptss[i];
	}
	
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
	nfds_t nFDs=4; //pseudoterminal ,event, inotify, timer

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
	
	/*********INOTIFY**********/
	/*************************/
	int inotifyFD = inotify_init();
	if (inotifyFD < 0) 
	{
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

	// Watch the /dev/pts directory
    int watchFD = inotify_add_watch(inotifyFD, "/dev/pts", IN_CREATE | IN_DELETE);
	if (watchFD < 0) 
	{
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }
	pFDs[2].fd=inotifyFD;
	pFDs[2].events = POLLIN;

	/**********TIMERFD*********/
	/*************************/
	//deactivated by default
	pFDs[3].fd = -1;
	pFDs[3].events = 0;

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
			// printf("  fd=%d; events: %s%s%s\n", pFDs[i].fd,
   			// (pFDs[i].revents & POLLIN)  ? "POLLIN "  : "",
            // (pFDs[i].revents & POLLHUP) ? "POLLHUP " : "",
            // (pFDs[i].revents & POLLERR) ? "POLLERR " : "");

			if(pFDs[i].revents & POLLIN)
			{	
				int len;
				if (pFDs[i].fd == inotifyFD)
				{
					len = read(pFDs[i].fd,buffer,sizeof(buffer));

					int j=0;
					while (j < len) 
					{
            			struct inotify_event *event = (struct inotify_event *) &buffer[j];

            			if (event->len) 
						{
                			if (event->mask & IN_CREATE) 
							{
                	    		printf("New pts opened: %s\n", event->name);
								int num;
								if(sscanf(event->name,"%d",&num)==1)
								{
									appendPts(args->ptsFD,5,num,false);
								}
                			}
						   	else if (event->mask & IN_DELETE) 
							{
                	    		printf("pts closed: %s\n", event->name);
								int num;
								if(sscanf(event->name,"%d",&num)==1)
								{
									if(removePts(args->ptsFD,5,num))
									{
										findNotAssignedPts(args->ptsFD, 5);
									}
								}
                			}
            			}
            			j += (sizeof(struct inotify_event)) + event->len;
        			}
				}
				else
				// request from pseudoterminal
				if(pFDs[i].fd == args->ptyFD)
				{
					if (counter < 11)
					{
						len = read((int)pFDs[i].fd,buffer,sizeof(buffer));
						for (size_t i = 0; i < 5; i++)
						{
							if (args->ptsFD[i].pty == -1)
							{
								continue;
							}
							dprintf(args->ptsFD[i].fd,"%d received %d bytes:\n",args->ptsFD[i].pty, len);
							dprintf(args->ptsFD[i].fd,"%.*s",len,buffer);
						}
						
						// printf("received %d bytes:\n",len);
						// printf("%.*s",len,buffer);
					}
					else
					{
						//discard data received but not read
						if(tcflush(pFDs[i].fd,TCIFLUSH)==0)
						;
							// printf("buffer flushed\n");
					}
				}
				else
				//request from udp server
				if (pFDs[i].fd == args->eventFD)
				{
					len = read((int)pFDs[i].fd,&counter,sizeof(counter));
					printf("received %d bytes:\n",len);
					printf("%llu\n",(unsigned long long)counter);

					if(counter < 11)
					{
						//check if timer is already activated
						if (pFDs[3].fd=-1)
						{	
							//TODO: not sure if NONBLOCK timer is needed 
							pFDs[3].fd=timerfd_create( CLOCK_REALTIME, TFD_NONBLOCK);
							if(pFDs[3].fd ==-1)
							{
								perror("timerfd_create");
								exit(EXIT_FAILURE);
							}
							struct itimerspec newValue;
							newValue.it_value.tv_nsec = 0;
							newValue.it_value.tv_sec = 120;

							// w/o interval
							newValue.it_interval.tv_sec = 0;
							newValue.it_interval.tv_nsec = 0;
							if(timerfd_settime(pFDs[3].fd, 0, &newValue, NULL) == -1)
							{
								perror("timerfd_settime");
								exit(EXIT_FAILURE);								
							}
							pFDs[3].events=POLLIN;
						}
						
					}
					
					// redirectStdout(counter>10 ? -1 : (int)counter);

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
				//timer
				if (i==3)
				{
					uint64_t numExp;
					len = read(pFDs[i].fd, &numExp, sizeof(uint64_t));
					if (len != sizeof(uint64_t))
					{
						perror("read timerfd");
					}
					printf("expiration %ld\n",numExp);
					counter=11;

 					close(pFDs[i].fd);
					pFDs[i].fd = -1;
				}
				
						
				resPoll--;
				if(resPoll == 0)
				{
					break; //no more requests
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

int availablePts(struct ptyStruct ptss[5], int len)
{
	//check which pts's are opened
	DIR *dirp=opendir("/dev/pts");
	if (dirp == NULL) 
	{
 		perror("opendir");
 		return 1;
 	}

	struct dirent *dir;

	while((dir=readdir(dirp)) != NULL)
	{
		printf("%s\n", dir->d_name);
		int num;
		int formatRes=sscanf(dir->d_name,"%d",&num);
		if(formatRes==1)
		{
			printf("number\n");
			appendPts(ptss,len,num,true);
		}
	}
	closedir(dirp);
	return 0;
}

void appendPts(struct ptyStruct ptss[5], int len,int num, bool init)
{
	int biggestPts=num;
	int biggestPtsIdx=-1;
	//check if new pts is smaller than already stored
	for (int i = 0; i < len ; i++)
	{
		if (ptss[i].pty==-1)
		{
			//found empty slot for new pts
			ptss[i].pty=num;
			biggestPtsIdx=-1;

			if (!init)
			{
				char pty_path[20];
				snprintf(pty_path,20,"/dev/pts/%d",ptss[i].pty);

				ptss[i].fd=open(pty_path,O_WRONLY | O_NONBLOCK);
				if(ptss[i].fd==-1)
				{		
					perror(pty_path);
					ptss[i].pty=-1;
					return;
				}
				printf("opened pty:%d file descriptor %d\n",ptss[i].pty,ptss[i].fd);
			}
			break;
		}
		else 
		if (ptss[i].pty>biggestPts && init)
		{
			//found bigger than current to drop
			biggestPts = ptss[i].pty;
			biggestPtsIdx = i;
		}
	}

	if (biggestPtsIdx != -1)
	{
		//drop biggest pts from storage
		ptss[biggestPtsIdx].pty=num;
	}
}

bool removePts(struct ptyStruct ptss[5], int len, int pty)
{
	for (int i = 0; i < len ; i++)
	{
		if (ptss[i].pty==pty)
		{
			//found occupied slot by given pty
			ptss[i].pty=-1;
			close(ptss[i].fd);
			printf("closed pty: %d file descriptor %d\n",pty,ptss[i].fd);
			ptss[i].fd = -1;
			return true;
		}
	}
	return false;
}

void findNotAssignedPts(struct ptyStruct ptss[5], int len)
{
	char buf[NAME_MAX];
	//check current STDOUT pts to not take it into account
	int bytes=readlink("/proc/self/fd/1",buf,NAME_MAX);
	if (bytes == -1) 
	{
       perror("readlink");
       exit(EXIT_FAILURE);
    }
	buf[bytes]='\n';
	printf("STDOUT link=%s\n",buf);

	int stdoutPty;
	char prefix[9];
	int ret=sscanf(buf, "%9[/devpts] %d", prefix, &stdoutPty);
    printf("prefix=%s \t pts=%d\n",prefix,stdoutPty);

	//check which pts's are opened
	DIR *dirp=opendir("/dev/pts");
	if (dirp == NULL) 
	{
 		perror("opendir");
 		return ;
 	}

	//Pts available in /dev/pts/...
	const int maxPts=10;
	int availablePts[maxPts];
	memset(availablePts,-1,maxPts);
	struct dirent *dir;

	int foundPts=0;
	while((dir=readdir(dirp)) != NULL)
	{
		printf("%s\n", dir->d_name);
		int num;
		int formatRes=sscanf(dir->d_name,"%d",&num);
		if(formatRes==1 )
		{
			printf("number %d\n",num);

			bool alreadyExists=false;
			//check if given pts is already in ptts storage
			for (size_t i = 0; i < len; i++)
			{
				if (ptss[i].pty==num || stdoutPty==num)
				{
					alreadyExists=true;
					break;
				}
			}

			if (alreadyExists)
			{
				continue;
				printf("already exists\n");
			}

			availablePts[foundPts]=num;
			foundPts++;
			if (foundPts>=maxPts)
			{
				break;
			}
		}
	}
	closedir(dirp);

	if (foundPts==0)
	{
		printf("not found any new PTS\n");
		return;
	}

	printf("all founded pts:\n");
	for (size_t i = 0; i < foundPts; i++)
	{
		printf("%d\t",availablePts[i]);
	}
	printf("\n");

	//sort founded Pts
	for (size_t i = 0; i < foundPts-1; i++)
	{
		for (size_t j = i+1; j < foundPts; j++)
		{
			if (availablePts[i] < availablePts[j])
			{
				printf("change: %d is smaller than %d\n",availablePts[i], availablePts[j]);
				int tmp=availablePts[i];
				availablePts[i]=availablePts[j];
				availablePts[j]=tmp;
			}
		}
	}

	printf("all founded sorted pts:\n");
	for (size_t i = 0; i < foundPts; i++)
	{
		printf("%d\t",availablePts[i]);
	}
	printf("\n");

	for (size_t i = 0; i < len; i++)
	{
		if (ptss[i].pty==-1 )
		{	
			printf("foundPts: %d\n",foundPts);
			foundPts--;
			ptss[i].pty=availablePts[foundPts];

			char pty_path[20];
			printf("New pty %d will be opening\n",ptss[i].pty);
			snprintf(pty_path,20,"/dev/pts/%d",ptss[i].pty);
			ptss[i].fd=open(pty_path,O_WRONLY | O_NONBLOCK);
			if(ptss[i].fd==-1)
			{		
				perror(pty_path);
				ptss[i].pty=-1;
				i--;
				continue;
			}
			printf("opened pty:%d file descriptor %d\n",ptss[i].pty,ptss[i].fd);

			if (foundPts<=0)
			{
				break;
			}
		}
	}
	

}
