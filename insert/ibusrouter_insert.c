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

#define MyHOST "localhost"
#define MyNAME ""
#define MyPASSWD ""
#define MyDB   "test"

#define INSERT_COMMAND  "insert into \
	ibusDB(mac, model, status, voltage,current, \
			frequency, powerfactor, activepower, \
			apparentpower, mainenergy, negativeenergy, \
			rssi, lqi, signalstrength, alias, routermac, \
			timestamp) values('%s', '%s', '%s', '%s', '%s', \
				'%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s')"

char value[17][50];
char buf[4096] = { '\0' };
char *columnname[17] =
{ "mac", "model", "status", "voltage", "current", "frequency",
	"powerfactor", "activepower", "apparentpower", "mainenergy",
	"negativeenergy", "rssi", "lqi",
	"signalstrength", "alias", "mac", "time"
};

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

void cjson_mysql_insert(void);

int main(int argc, char *argv[])
{
	struct sockaddr_in addr_svr;
	int sockfd;
	int sockopt = 1;

	signals_register();

	memset(&addr_svr, 0, sizeof(addr_svr));
	if (argc == 2)
		addr_svr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		addr_svr.sin_addr.s_addr = inet_addr("127.0.0.1");

	addr_svr.sin_family = AF_INET;
	addr_svr.sin_port = htons(6066);

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &sockopt,
			sizeof(sockopt));

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
			else if (pid == 0) {

				printf("runtimes : %d\n", runtimes);

				int res = 0;
				inet_ntop(AF_INET, &addr_cln.sin_addr, str, sizeof str);
				printf("Client addr: %s, port: %d\n", str, ntohs(addr_cln.sin_port));

				while (res = read(connfd, buf, sizeof(buf) - 1)) {

					static cnt = 0;
					if (res == -1) {
						perror("read");

					}

					if (res) {
						cnt++;
						if(cnt == 2){
							cjson_mysql_insert();
						}
					}
					if (cnt >= 2) {
						cnt = 0;
						break;
					}
				}
				memset(buf, 0, sizeof(buf));
				close(connfd);
				exit(0);
			} else {	
				runtimes++;
				if (call_accept_flag == 0)
					call_accept_flag = 1;
				close(connfd);
			}
		}
	}
	return 0;
}

void cjson_mysql_insert(void){
	int mysql_close_flag = 0;
	//1.3+4 輸入:一個char字串 輸出:cJSON-object0 , 3.char->json+4.取得object0
	cJSON *jsonparse;
	jsonparse = cJSON_Parse(buf);
	if (!jsonparse){
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
		goto jsonerror;
	}
	cJSON *jsonnowdata;
	jsonnowdata = cJSON_GetArrayItem(jsonparse,0);
	if (!jsonnowdata){
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
		goto jsonerror;
	}

	//2.5 輸入:cJSON-object0 輸出:value[15]、value[16]
	cJSON *jsonroutermac;
	char *Routermac;
	char getstring[50];
	jsonroutermac = cJSON_GetObjectItem(jsonnowdata, columnname[15]);
	if (!jsonroutermac){
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
		goto jsonerror;
	}else{
		Routermac = cJSON_Print(jsonroutermac);
		delet_char(Routermac, getstring);
		strcpy(value[15], getstring);
	}
	free(Routermac);

	cJSON *jsontimestamp;
	char *Timestamp;
	jsontimestamp = cJSON_GetObjectItem(jsonnowdata, columnname[16]);
	if (!jsontimestamp){
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
		goto jsonerror;
	}else{
		Timestamp = cJSON_Print(jsontimestamp);
		delet_char(Timestamp, getstring);
		strcpy(value[16], getstring);
	}
	free(Timestamp);

	//3.6+7 輸入:cJSON-object0 輸出:devices array、devices array的size
	//6.取得devices
	//7.計算devices array的size
	cJSON *jsondevicesdata;
	int devicesamount;
	jsondevicesdata = cJSON_GetObjectItem(jsonnowdata,"devices");
	if (!jsondevicesdata){
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
		goto jsonerror;
	}

	devicesamount = cJSON_GetArraySize(jsondevicesdata);

	//8.連線mysql
	MYSQL my_connection;
	MYSQL *mysqlconnectres;
	mysql_init(&my_connection);
	mysqlconnectres = mysql_real_connect (&my_connection,
			MyHOST, MyNAME, MyPASSWD, MyDB, 0, NULL, CLIENT_FOUND_ROWS);
	cJSON *jsonOneDeviceAllData;
	cJSON *jsonOneDeviceData;
	if (mysqlconnectres == &my_connection) {
		printf ("db connect success \n");
		/*4.9 輸入:devices array、devices array的size 輸出:無 
		  動作:每個devices array 的 object 依序寫入value，
		  在寫入query，寫入mysql*/
		int i = 0;
		int j = 0;
		char *data;
		char Myquery[512] = {'\0'};
		int Myres = 0;
		for (i = 0; i < devicesamount; i++){
			jsonOneDeviceAllData = cJSON_GetArrayItem(jsondevicesdata, i);
			if (!jsonOneDeviceAllData){
				printf("Error before: [%s]\n",cJSON_GetErrorPtr());
				mysql_close_flag = 1;
				goto jsonerror;
			}
			for (j=0; j<15; j++){
				jsonOneDeviceData = cJSON_GetObjectItem(jsonOneDeviceAllData, columnname[j]);
				if (!jsonOneDeviceData){
					printf("Error before: [%s]\n",cJSON_GetErrorPtr());
					mysql_close_flag = 1;
					goto jsonerror;
				}
				data = cJSON_Print(jsonOneDeviceData);
				delet_char(data, getstring);
				strcpy(value[j], getstring);
				free(data);
			}
			sprintf(Myquery, INSERT_COMMAND, value[0], value[1], value[2], value[3],
					value[4], value[5], value[6], value[7], value[8], value[9], value[10], 
					value[11], value[12], value[13], value[14], value[15], value[16]);

			Myres = mysql_query(&my_connection, Myquery);
			if (!Myres) {
				printf("Inserted %lu rows\n", 
						(unsigned long) mysql_affected_rows (&my_connection));
			}else{
				fprintf(stderr, "Insert error %d:%s\n",
						mysql_errno(&my_connection),
						mysql_error(&my_connection));
				mysql_close_flag = 1;
				goto jsonerror;
			}
		}
	} else {
		fprintf(stderr, "Insert error %d: %s\n",mysql_errno(&my_connection),
				mysql_error(&my_connection));
	}

jsonerror://10.釋放cJSON
	if (mysql_close_flag == 1){
		mysql_close(&my_connection);
		mysql_close_flag = 0;
	}
	cJSON_Delete(jsonparse);

	return;
}
