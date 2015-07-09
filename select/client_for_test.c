#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include "cJSON.h"
#include <errno.h>
#include <string.h>

#define SERVER_IP "192.168.2.88"
#define PORT_NUMBER 9000

int fun (char *buf){
	cJSON *jsondata;
	int row=0;
	jsondata=cJSON_Parse(buf);
    row = cJSON_GetArraySize(jsondata);
	cJSON_Delete(jsondata);
	return row;
}

char* recvstringline(int);
int recvstringline2(int, char **);

int main(int argc, char *argv[]){
	int sockfd;
	struct sockaddr_in addr;
	int res=0;
	int flag=0;
	int reshead=0;
	int jsonrow=0;
	int totaljsonrow=0;
	char* resfinal;
	char respsize[10]={'\0'};
	char jsonitems[] = "{\"gatewaymac\" : \"0004EDD82300\", \"devicemac\" : \"000D6F000075814B\", \
					   \"starting_time\" : \"1427600000\",\"ending_time\" : \"1428089815\"}";
	while (1) {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			perror("socket");
			exit(1);
		}

		bzero(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(PORT_NUMBER);
		addr.sin_addr.s_addr = inet_addr(SERVER_IP);

		res = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));
		if (res < 0) {
			perror("connect");
			exit(1);
		}

		res = send(sockfd, jsonitems, sizeof jsonitems, 0);
		if (res < 0) {
			perror("ERROR writing to socket");
			exit(1);
		}

		while(1){
			resfinal = recvstringline(sockfd);
			if(!resfinal){
				break;
			}
			res = strlen(resfinal);
			printf("res = %d\n", res);
			jsonrow = fun(resfinal);
			totaljsonrow = totaljsonrow+jsonrow;
			free(resfinal);
		}
		close(sockfd);
		printf("total jsonrow = %d\n",totaljsonrow);
		totaljsonrow=0;
		sleep(5);
	}
	return 0;
}

char* recvstringline(int s){
	int flag=0;
	int res=0;
	int len=0;
	char len_char[10]={'\0'};
	char *resptr=NULL;

	while(1){
		if(flag==0){
			res = recv(s, len_char, sizeof(len_char), MSG_WAITALL);
			if(res>0){
				len=atoi(len_char);
				flag=1;
			}else if(res==0){
				break;
			}else{
				perror("recv");
				break;
			}
		}else if(flag==1){
			resptr=malloc(len+1);
			memset(resptr, 0, len+1);
			res=recv(s, resptr, len, MSG_WAITALL);
			if(res>0){
				if(res==len){
					len = strlen(resptr);
					break;
				}else{
					printf("not recv all msg.");
					free(resptr);
					resptr=NULL;
					break;
				}
			}else if(res==0){
				printf("not recv any msg.");
				free(resptr);
				resptr=NULL;
				break;
			}else{
				free(resptr);
				resptr=NULL;
				perror("recv");
				break;
			}
		}
	}
	return resptr;
}

