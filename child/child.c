    #include <unistd.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <time.h>

int main(int argc, char** argv)
{
    if(argc<2)
	{
		printf("not enough arguments!!!:exit\n");
		exit(1);
	}
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);    
	int pid=getpid();
    while(1)
    {
        time(&rawtime);
    	info = localtime(&rawtime);    
		//dprintf(STDOUT_FILENO,"child=%s\t pid=%d\t current time=%s\n",argv[1] ,pid ,asctime(info));
		printf("child=%s\t pid=%d\t started=%s\n",argv[1] ,pid ,asctime(info));	
        sleep(5);
    }
    return 0;
}
