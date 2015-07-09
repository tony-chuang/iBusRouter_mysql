#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include "mysql.h"
#include "cJSON.h"
#include "list.h"

#include <netinet/tcp.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/sockios.h>
#endif

#define PORT_MOBILE 9000

#define MyHOST "localhost"
#define MyNAME ""
#define MyPASSWD ""
#define MyDB   "test"

#define SELECT_COMMAND "select * from ibusDB where gatewaymac = '%s' and devicemac = '%s' \
	and timestamp between '%s' and '%s'"

char selectvalue[4][20];
char buf[1024] = { '\0' };
char *selectqueryitem[4] = {
	"gatewaymac", "devicemac", "starting_time", "ending_time"
};
char *aszflds[17] = {
	"mac", "model", "status", "voltage", "current", "frequency", "powerfactor", "activepower", 
	"apparentpower", "mainenergy", "negativeenergy", "rssi", "lqi", "signalstrength", "alias", 
	"routermac", "timestamp"
};

struct json_node{
	cJSON* jsonptr;
	struct llhead link;
};

void LL_ADD_TAIL(struct llhead*, cJSON*);
int LL_GET_LENGTH(struct llhead*);
void LL_FREE_ALL(struct llhead*);
struct llhead fun3(struct llhead* , MYSQL_RES*, int);

int sendstringline(int, char*);

void signals_handler(int num)
{
	int status = 0;
	wait(&status);

	return;
}

void signals_register(void)
{
	struct sigaction sig;
	memset(&sig, 0, sizeof sig);
	sig.sa_handler = &signals_handler;

	sigaction(SIGCHLD, &sig, NULL);
	return;
}

void delet_char(char *source ,char *dest)
{
	while(*source)
		(*source == '"')? (*source++) : (*dest++ = *source++);
	*dest = '\0';
}

void makeSocketLinger(int fd)
{
	struct linger ling;
	ling.l_onoff=1;
	ling.l_linger=30;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}

void depleteSendBuffer(int fd) 
{
#ifdef __linux__
	int lastOutstanding=-1;
	for(;;) {
		int outstanding;
		ioctl(fd, SIOCOUTQ, &outstanding);
		//if(outstanding != lastOutstanding) 
			//printf("Outstanding: %d\n", outstanding);
		lastOutstanding = outstanding;
		if(!outstanding)
			break;
		usleep(1000);
	}
#endif
}

int main(int argc, char *argv[]){
	struct sockaddr_in addr_svr;
	int sockfd;
	int sockopt = 1;

	MYSQL my_connection;
	MYSQL *mysqlconnectres;
	MYSQL_RES *res_ptr;
	MYSQL_ROW sqlrow;
	MYSQL_FIELD *fd;
	int mysql_close_flag = 0;
	char myquery[256] = {'\0'};
	int myres = 0;
	int myrownum = 0;
	int i = 0;
	int j = 0;
	char *out;
	char jstr[92160];
	char getstring[20];
	char over[]="over";
	char conti[]="conti";
	cJSON *jsonparse;
	cJSON *jsongatewaymac;
	cJSON *jsondevicemac;
	cJSON *jsonstartingtime;
	cJSON *jsonendingtime;
	cJSON *jsonrespond, *fld;

	struct llhead *lp;
	struct json_node *jsonnodep;

	signals_register();

	memset(&addr_svr, 0, sizeof(addr_svr));
	addr_svr.sin_family = AF_INET;
	addr_svr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_svr.sin_port = htons(PORT_MOBILE);

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &sockopt, sizeof(sockopt));

	if (bind(sockfd, (struct sockaddr *) &addr_svr, sizeof(addr_svr)) == -1) {
		perror("bind failed");
		exit(1);
	}

	if (listen(sockfd, 10) == -1) {
		perror("listen failed");
		exit(1);
	}

	int static runtimes = 1;
	int call_accept_flag = 0;
	/* server loop ; not concurrent mode */
	for (;;) {
		int connfd;
		char str[128] = { '0' };
		struct sockaddr_in addr_cln;
		socklen_t sLen = sizeof(addr_cln);

		if (call_accept_flag == 0)
			printf("\n********** Call accept ! **********\n");

		connfd = accept(sockfd, (struct sockaddr *) &addr_cln, &sLen);

		if (connfd == -1) {
			if (errno == EINTR) {
				call_accept_flag = 0;
				perror("accept");
				continue;
			} else {
				perror("accept");
				exit(1);
			}	
		} else {
			pid_t pid;
			pid=fork();
			if (pid < 0)
				printf("error in fork!");
			else if (pid == 0) { //child
				int res = 0;

				printf("runtimes : %d\n", runtimes);
				inet_ntop(AF_INET, &addr_cln.sin_addr, str, sizeof str);
				printf("Client addr: %s, port: %d\n", str, ntohs(addr_cln.sin_port));

				memset(buf, 0, sizeof(buf));
				res = recv(connfd, buf, sizeof(buf), 0);
				if (res > 0) {
					//?‹å?è§???æŸ¥è©¢ã€å?è£ã€å‚³??
					/********************ä¸€?è§£??+ ?¢ç?mysqlè«‹æ?èªžå¥**********************/
					jsonparse = cJSON_Parse(buf);
					if (!jsonparse){
						printf("Error before: [%s]\n",cJSON_GetErrorPtr());
						goto jsonerror;
					}

					jsongatewaymac = cJSON_GetObjectItem(jsonparse, selectqueryitem[0]);
					if (!jsongatewaymac){
						printf("Error before: [%s]\n",cJSON_GetErrorPtr());
						goto jsonerror;
					}else{
						out = cJSON_Print(jsongatewaymac);
						delet_char(out, getstring);
						strcpy(selectvalue[0], getstring);
					}
					free(out);

					jsondevicemac = cJSON_GetObjectItem(jsonparse, selectqueryitem[1]);
					if (!jsondevicemac){
						printf("Error before: [%s]\n",cJSON_GetErrorPtr());
						goto jsonerror;
					}else{
						out = cJSON_Print(jsondevicemac);
						delet_char(out, getstring);
						strcpy(selectvalue[1], getstring);
					}
					free(out);

					jsonstartingtime = cJSON_GetObjectItem(jsonparse, selectqueryitem[2]);
					if (!jsonstartingtime){
						printf("Error before: [%s]\n",cJSON_GetErrorPtr());
						goto jsonerror;
					}else{
						out = cJSON_Print(jsonstartingtime);
						delet_char(out, getstring);
						strcpy(selectvalue[2], getstring);
					}
					free(out);

					jsonendingtime = cJSON_GetObjectItem(jsonparse, selectqueryitem[3]);
					if (!jsonendingtime){
						printf("Error before: [%s]\n",cJSON_GetErrorPtr());
						goto jsonerror;
					}else{
						out = cJSON_Print(jsonendingtime);
						delet_char(out, getstring);
						strcpy(selectvalue[3], getstring);
					}
					free(out);

					sprintf(myquery, SELECT_COMMAND, selectvalue[0], selectvalue[1], 
							selectvalue[2], selectvalue[3]);


					/***************************äºŒã€æŸ¥è©¢ã€æ???**************************/
					//mysql???+?¥è©¢?½å?
					mysql_init(&my_connection);
					mysqlconnectres = mysql_real_connect (&my_connection,
							MyHOST, MyNAME, MyPASSWD, MyDB, 0, NULL, CLIENT_FOUND_ROWS);
					if (mysqlconnectres != &my_connection) {
						fprintf(stderr, "Select error %d: %s\n",mysql_errno(&my_connection),
								mysql_error(&my_connection));
						goto jsonerror;
					}else{
						printf ("db connect success \n");
						myres = mysql_query(&my_connection, myquery);
						if (!myres) {
							res_ptr=mysql_store_result(&my_connection);

							if(res_ptr){
								printf("Retrieved %lu Rows\n",(unsigned long)mysql_num_rows(res_ptr));
								myrownum=mysql_num_rows(res_ptr);

								int jsonlistsize=0;
								int outlen=0;
								char outlenchar[10]={'\0'};
								LL_HEAD(jsonlist2);

								jsonlist2=fun3(&jsonlist2, res_ptr, 60);
								jsonlistsize = LL_GET_LENGTH(&jsonlist2);
								printf("size of jsonlist = %d\n", jsonlistsize);
								LL_FOREACH(&jsonlist2, lp){
									jsonnodep = LL_ENTRY(lp, struct json_node, link);
									out = cJSON_Print(jsonnodep->jsonptr);
									outlen=strlen(out);
									printf("out len = %d\n", outlen);
									res = sendstringline(connfd, out);
									if(res<0){
										printf("sendstringline error\n");
										free(out);
										continue;
									}
									free(out);
								}
								LL_FREE_ALL(&jsonlist2);
							}
							mysql_free_result(res_ptr);
						}else{
							fprintf(stderr, "select error %d:%s\n",
									mysql_errno(&my_connection), mysql_error(&my_connection));
							mysql_close_flag = 1;
							goto jsonerror;
						}
						mysql_close(&my_connection);
					}

jsonerror://10.?‹æ”¾cJSON
					if (mysql_close_flag == 1){
						mysql_close(&my_connection);
						mysql_close_flag = 0;
					}
					cJSON_Delete(jsonparse);
				}else{
					perror("recv");
				}
				memset(buf, 0, sizeof(buf));
				depleteSendBuffer(connfd);//test
				close(connfd);
				exit(0);
			} else {  //parent	
				runtimes++;
				if (call_accept_flag == 0)
					call_accept_flag = 1;
				close(connfd);
			}
		}
	}
	return 0;
}

void LL_ADD_TAIL(struct llhead* jsonlist2, cJSON* ptr){
	struct json_node* node;
	int i=0;
	struct llhead *lp;
	struct json_node *p;

	node = (struct json_node*)malloc(sizeof(struct json_node));
	(*node).jsonptr = ptr;
	(*node).link.prev = NULL;
	(*node).link.next = NULL;
	LL_TAIL(jsonlist2, &(*node).link);
	return;
}

int LL_GET_LENGTH(struct llhead* jsonlist){
	int leng=0;
	struct llhead *lp;
	LL_FOREACH(jsonlist, lp){
		leng++;
	}
	return leng;
}

void LL_FREE_ALL(struct llhead* jsonlist){
	struct llhead *lp;
	struct llhead *lp2;
	struct json_node *p;
	LL_FOREACH_SAFE(jsonlist, lp, lp2){
		p=LL_ENTRY(lp, struct json_node, link);
		cJSON_Delete(p->jsonptr);
		LL_DEL(lp);
		free(p);
	}
	return;
}

/* fun3 */

struct llhead fun3(struct llhead* jsonlist, MYSQL_RES *resptr, int jsonobjnum){
	int cnt=0;
	int i=0;
	int j=0;
	int fieldnum = mysql_num_fields(resptr);
	int rownum = mysql_num_rows(resptr);
	ldiv_t objnum;
	MYSQL_ROW row;
	MYSQL_FIELD *fields;
	cJSON *json, *fld;

	fields = mysql_fetch_fields(resptr);

	if(jsonobjnum != 0){
		objnum = ldiv(rownum, jsonobjnum);
	}

	if(jsonobjnum == 0){
		json = cJSON_CreateArray();
		for (cnt=0; cnt<rownum; cnt++){
			row = mysql_fetch_row(resptr);
			cJSON_AddItemToArray(json,fld=cJSON_CreateObject());
			for(i=0; i<fieldnum; i++){
				cJSON_AddStringToObject(fld, fields[i].name, row[i]);
			}
		}
		LL_ADD_TAIL(jsonlist, json);
	}else{
		for(i=0; i<objnum.quot; i++){
			json = cJSON_CreateArray();
			for (cnt=0; cnt<jsonobjnum; cnt++){
				row=mysql_fetch_row(resptr);
				cJSON_AddItemToArray(json,fld=cJSON_CreateObject());
				for(j=0; j<fieldnum; j++){
					cJSON_AddStringToObject(fld, fields[j].name, row[j]);
				}
			}
			LL_ADD_TAIL(jsonlist, json);
		}
		if(objnum.rem){
			json = cJSON_CreateArray();
			for (cnt=0; cnt<objnum.rem; cnt++){
				row=mysql_fetch_row(resptr);
				cJSON_AddItemToArray(json,fld=cJSON_CreateObject());
				for(i=0; i<fieldnum; i++){
					cJSON_AddStringToObject(fld, fields[i].name, row[i]);
				}
			}
			LL_ADD_TAIL(jsonlist, json);
		}
	}
	return *jsonlist;
}

int sendstringline(int s, char *string){
	int len=0;
	int len_cal=0;
	int len_size=0;
	int res=0;
	char len_char[10]={'\0'};
	char *msg;

	len=strlen(string);
	len_cal=len;
	for(; len_cal>0; len_cal/=10){
		len_size++;
	}
	if(len_size>10){
		printf("msg is too large, len_size > 10.");
		exit(1);
	}
	memset(len_char, 0, sizeof(len_char));
	sprintf(len_char, "%d", len);
	res = send(s, len_char, sizeof(len_char), 0);
	if(res<0){
		perror("send");
		exit(1);
	}
#ifdef __linux__
			for(;;){
				int outstanding;
				ioctl(s, SIOCOUTQ, &outstanding);
				if(!outstanding)
					break;
				usleep(1000);
			}
#endif

	res = send(s, string, strlen(string), 0);
	if(res<0){
		perror("send");
		exit(1);
	}
#ifdef __linux__
			for(;;){
				int outstanding;
				ioctl(s, SIOCOUTQ, &outstanding);
				if(!outstanding)
					break;
				usleep(1000);
			}
#endif
	return res;
}
