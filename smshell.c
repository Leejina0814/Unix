#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include "smallsh.h"


static char inbuf[MAXBUF], tokbuf[2*MAXBUF], *ptr = inbuf, *tok = tokbuf;
char *prompt = "$>";
int intr_p = 0;
int fg_pid = 0;
static char special [] = {' ', '\t','&', ';', '\n', '\0' };//특수기호 정리
void handler(int sig);//ctrl+C 와 ctrl+\를 handling하는 함수
int join(char *com1[], char *com2[]);//pipe연결하는 함수
int chk =0;
int userin(char *p){
	int c, count;
	ptr = inbuf; tok = tokbuf;
	printf("%s", prompt);
	count = 0;

	while(1){
		if((c= getchar() )== EOF)//코멘드 창에서 입력받음
			return EOF;
		if(count <MAXBUF)
			inbuf[count++] = c;
		if(c == '\n' && count < MAXBUF){
			inbuf[count] = '\0';
			return count;
		}
		if(c == '\n'){//버퍼 크기보다 큰경우
			printf("smallsh : inputline too long\n");
			count = 0;
			printf("%s", prompt);
		}
	}
}

void  procline(){
	char * arg[MAXARG+1];
	int toktype = 0;
	int narg = 0;
	int type;

	intr_p = 0;

	for(;;){
		switch(toktype = gettok(&arg[narg])){//gettok함수를 통해 token의 type을 반환받음
			case ARG: //일반 argument
				if(narg< MAXARG) narg++;
				break;
			case EOL:
			case SEMICOLON:
			case AMPERSAND:
				type = (toktype == AMPERSAND) ?  BACKGROUND : FOREGROUND;
				if(narg != 0){
					arg[narg] = NULL;
					runcommand(arg, type);//command실행

					if(toktype == EOL)//종료
						return;

					narg = 0;
					break;
				}
		}
	}
}
//입력받은 명령을 정리해줌
int gettok(char **outptr){
	int type;
	*outptr = tok;
	while(*ptr == ' ' || *ptr == '\t')//비어있는 경우 정리
		ptr++;

	*tok++ = *ptr;

	switch(*ptr++){
		case '\n': type = EOL;break;
		case '&': type = AMPERSAND;break;
		case ';': type = SEMICOLON;break;
		default : type = ARG;

			  while(inarg(*ptr))
				  *tok++ = *ptr++;
	}
	*tok++ = '\0';
	return type;
}

//특수문자인지 아닌지 알려줌
int inarg(char c){
	char *wrk;

	for(wrk = special; *wrk != '\0'; wrk++){
		if(c == *wrk)
			return (0);
	}
	return (1);
}
//명령을 실행하는 함수
int runcommand(char **cline, int where){
	pid_t pid;
	int exitstat, ret;
	int status;

	struct sigaction sa_ign,sa_conf;;
	sa_ign.sa_handler = SIG_IGN;
	sigemptyset(&sa_ign.sa_mask);
	sa_ign.sa_flags = SA_RESTART;

	sa_conf.sa_handler = handler;//signal을 handling하는 함수 지정
	sigemptyset(&sa_conf.sa_mask);
	sa_conf.sa_flags = SA_RESTART;

	//cd명령인 경우
	if(strcmp(cline[0], "cd")== 0){
		if(chdir(cline[1]))//해당 directory로 변경
			printf("No directory\n");
		return 0;
	}
	//chk가 1인경우는 pipe관련 명령을 수행한다
	if(chk == 1){
		int i;
		char *cmdvec1[4];//앞쪽 명령어 저장
		char *cmdvec2[5];//뒷쪽 명령어 저장
		for(i=0; ; i++){
			if(!strcmp(cline[i], "|")){//"|"이 나오기 전까지 저장
				cmdvec1[i] = NULL;
				break;
			}
			cmdvec1[i] = cline[i];
		}
		int k = 0;
		for(i = i+1; cline[i] != NULL; i++){//"|"이 나온 후부터 저장
			cmdvec2[k] = cline[i];
			k++;
		}
		cmdvec2[k] = NULL;	
		join(cmdvec1, cmdvec2);	//join함수로 넘겨줌
		return 0;
	}
	//그외의 명령어 처리
	if((pid = fork())<0){
		perror("smallsh");
		return(-1);
	}

	if(pid == 0){//child인 경우
		sigaction(SIGINT, &sa_ign, NULL);
		sigaction(SIGQUIT, &sa_ign, NULL);
		execvp(*cline, cline);
		perror(*cline);
		exit(127);
	}
	else
		fg_pid = pid;
	if(where == BACKGROUND){//background인 경우
		fg_pid = 0;
		printf("[Process id `:%d]\n", pid);
		return(0);
	}

	sigaction(SIGINT, &sa_conf, NULL);
	sigaction(SIGQUIT, &sa_conf, NULL);
	//ctrl+C 와 ctrl + \의 경우 handling하게 처리
	while((ret = wait(&exitstat))!= pid && ret != -1);
	fg_pid = 0;
	return(ret == -1 ? -1 : exitstat);

}
//handling해주는 함수
void handler(int sig){
	switch(sig){
		case SIGINT:
			printf("Ctrl + c SIGINT\n");
			break;
		case SIGQUIT:
			printf("Ctrl + \ SIGQUIT\n");
			break;
	}

}
//pipe연결 해주는 함수
//com1의 출력을 com2의 입력으로!
int join(char *com1[], char *com2[]){
	int p[2], status;
	pid_t pid1;
	pid_t pid2;
	//pipe생성
	if(pipe(p) == -1){
		printf("fork call error");
		exit(1);
	}
	//앞쪽 명렁수행
	pid1  = fork();
	switch(pid1){
	case -1:
		perror("fork error");break;
	case 0:
		//출력부분을 pipe로 지정
		dup2(p[1],1);
		close(p[1]);
		close(p[0]);
		execvp(com1[0], com1);
		exit(0);
	}
	//뒷쪽 명령 수행
	pid2 = fork();
	switch(pid2){
	case -1: perror("fork");break;
	case 0:
		//입력부분을 pipe로 지정
		dup2(p[0],0);
		close(p[1]);
		close(p[0]);
		execvp(com2[0], com2);
		exit(0);
	}
}

int main(){

	struct sigaction sa;
	sa.sa_handler = handler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	while(userin(prompt)!= EOF){
		//pipe관련 연산이 들어오는 경우 chk를 1로!
		if(strchr(inbuf, '|') != NULL){
			chk  = 1;

		}
		else{	chk = 0;
		}
		procline();
	}
}
