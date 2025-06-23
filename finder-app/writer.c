#include<stdio.h>
#include<syslog.h>
#include<string.h>
#include <fcntl.h>
#include <unistd.h>
int main(int argc, char* argv[]){
	if(argc != 3){
		syslog(LOG_ERR,"please fill three augrument");
		return 1;
	}
	int fd = open(argv[1],O_WRONLY | O_CREAT | O_APPEND, 0644);
	if(fd<0){
		syslog(LOG_ERR,"can't open the file");
		return 1;
	}
	syslog(LOG_DEBUG,"Writing %s to %s",argv[2], argv[1]);
	ssize_t wr; 
	wr = write(fd, argv[2], strlen(argv[2]));
	if(wr <0){
	syslog(LOG_ERR,"can't write to file");
	}
	close(fd);
	return 0;

}
