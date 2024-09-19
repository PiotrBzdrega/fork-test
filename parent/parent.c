    #include <unistd.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <sys/types.h>
	#include <errno.h>
	#include <fcntl.h>
	#include <limits.h> //PATH_MAX
	#define LEN 11
int main(int argc, char** argv)
{
	if(argc<3)
	{
        printf("not enough arguments!!\n");
		printf("test [max childrens] [pseudoterminal number]!!\n");
	    exit(1);
	}
 	
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

	int pfd[2];
	if(pipe(pfd)==-1)
	{
		perror("pipe");
		exit(1);
	}
	printf("read pipe end=%d, write pipe end=%d\n",pfd[0],pfd[1]);
	
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
			close(pfd[0]);

			snprintf(numb,LEN,"%d",child_nr+1);
			args[1]=numb;
		
			//int null_fd=open("/dev/null",O_WRONLY);
			//dup2(null_fd,1);
			//close(null_fd);

			//int pts_fd=open("/dev/pts/2",O_WRONLY);
			//if(pts_fd==-1)
			//{
			//	perror("open pts_fd");
			//	exit(1);
			//}
			//dup2(pts_fd,1);
			//dup2(pts_fd,2);
			//close(pts_fd);

			dup2(pfd[1],STDOUT_FILENO);
			dup2(pfd[1],STDERR_FILENO);
			close(pfd[1]);

			execv(args[0],args);

		default:
			break;
		}	
	}

	
	//close write pipe end
	close(pfd[1]);


	int pts_fd=open(pty_path,O_WRONLY);
	if(pts_fd==-1)
	{
		perror("open pts_fd");
		exit(1);
	}
	dup2(pts_fd,STDOUT_FILENO);
	dup2(pts_fd,STDERR_FILENO);
	close(pts_fd);


	printf("waits for read data in pipe\n");

	char buffer[1024];
	memset(buffer,0x00,1024);
	
	int len;
	while(1)
	{
		len=read(pfd[0],buffer,1023);
		printf("received %d bytes:\n",len);
		//buffer[n]='\n';
		printf("%.*s",len,buffer);
	}

//    int counter=0;

//    while(1)
//    {
//    	printf("counter=%d\n",counter);
//		counter++;
//		sleep(5);
//    }
    return 0;
}


