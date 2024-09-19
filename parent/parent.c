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
	#define LEN 11
	#define _GNU_SOURCE

void *readFunc(void *fd);

int main(int argc, char** argv)
{
	if(argc!=4)
	{
        printf("not valid amount of arguments!!\n");
		printf("parent [max children] [pseudoterminal number] [pipe/pty]\n");
	    exit(1);
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

	char pty_path[20];
	snprintf(pty_path,20,"/dev/pts/%d",target_pty);

	printf("target pty=%s\n",pty_path);

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
            exit(1);
        }
 	    printf("pty name=%s\n",ptyName);
		printf("master pty=%d, slave pty=%d\n",IPCfd[0],IPCfd[1]);
	}
	else
	{
		if(pipe(IPCfd)==-1)
		{
			perror("pipe");
			exit(1);
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

	int pts_fd=open(pty_path,O_WRONLY);
	if(pts_fd==-1)
	{
		perror("open pts_fd");
		exit(1);
	}
	dup2(pts_fd,STDOUT_FILENO);
	dup2(pts_fd,STDERR_FILENO);
	close(pts_fd);


//    int counter=0;
//    while(1)
//    {
//    	printf("counter=%d\n",counter);
//		counter++;
//		sleep(5);
//    }

	pthread_t thread1;


	printf("waits for read data in %s\n",ptyMode ? "pty" : "pipe");
	pthread_create(&thread1,NULL,readFunc,(void*) IPCfd[0]);

	pthread_join(thread1,NULL);	

    return 0;
}

void *readFunc(void *fd)
{
	char buffer[1024];
	memset(buffer,0x00,1024);
	
	int len;
	while(1)
	{
		len=read((int)fd,buffer,1023);
		printf("received %d bytes:\n",len);
		//buffer[n]='\n';
		printf("%.*s",len,buffer);
	}

}

