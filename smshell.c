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
static char special [] = {' ', '\t','&', ';', '\n', '\0' };//Ư����ȣ ����
void handler(int sig);//ctrl+C �� ctrl+\�� handling�ϴ� �Լ�
int join(char *com1[], char *com2[]);//pipe�����ϴ� �Լ�
int chk =0;
int userin(char *p){
	int c, count;
	ptr = inbuf; tok = tokbuf;
	printf("%s", prompt);
	count = 0;

	while(1){
		if((c= getchar() )== EOF)//�ڸ�� â���� �Է¹���
			return EOF;
		if(count <MAXBUF)
			inbuf[count++] = c;
		if(c == '\n' && count < MAXBUF){
			inbuf[count] = '\0';
			return count;
		}
		if(c == '\n'){//���� ũ�⺸�� ū���
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
		switch(toktype = gettok(&arg[narg])){//gettok�Լ��� ���� token�� type�� ��ȯ����
			case ARG: //�Ϲ� argument
				if(narg< MAXARG) narg++;
				break;
			case EOL:
			case SEMICOLON:
			case AMPERSAND:
				type = (toktype == AMPERSAND) ?  BACKGROUND : FOREGROUND;
				if(narg != 0){
					arg[narg] = NULL;
					runcommand(arg, type);//command����

					if(toktype == EOL)//����
						return;

					narg = 0;
					break;
				}
		}
	}
}
//�Է¹��� ����� ��������
int gettok(char **outptr){
	int type;
	*outptr = tok;
	while(*ptr == ' ' || *ptr == '\t')//����ִ� ��� ����
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

//Ư���������� �ƴ��� �˷���
int inarg(char c){
	char *wrk;

	for(wrk = special; *wrk != '\0'; wrk++){
		if(c == *wrk)
			return (0);
	}
	return (1);
}
//����� �����ϴ� �Լ�
int runcommand(char **cline, int where){
	pid_t pid;
	int exitstat, ret;
	int status;

	struct sigaction sa_ign,sa_conf;;
	sa_ign.sa_handler = SIG_IGN;
	sigemptyset(&sa_ign.sa_mask);
	sa_ign.sa_flags = SA_RESTART;

	sa_conf.sa_handler = handler;//signal�� handling�ϴ� �Լ� ����
	sigemptyset(&sa_conf.sa_mask);
	sa_conf.sa_flags = SA_RESTART;

	//cd����� ���
	if(strcmp(cline[0], "cd")== 0){
		if(chdir(cline[1]))//�ش� directory�� ����
			printf("No directory\n");
		return 0;
	}
	//chk�� 1�ΰ��� pipe���� ����� �����Ѵ�
	if(chk == 1){
		int i;
		char *cmdvec1[4];//���� ��ɾ� ����
		char *cmdvec2[5];//���� ��ɾ� ����
		for(i=0; ; i++){
			if(!strcmp(cline[i], "|")){//"|"�� ������ ������ ����
				cmdvec1[i] = NULL;
				break;
			}
			cmdvec1[i] = cline[i];
		}
		int k = 0;
		for(i = i+1; cline[i] != NULL; i++){//"|"�� ���� �ĺ��� ����
			cmdvec2[k] = cline[i];
			k++;
		}
		cmdvec2[k] = NULL;	
		join(cmdvec1, cmdvec2);	//join�Լ��� �Ѱ���
		return 0;
	}
	//�׿��� ��ɾ� ó��
	if((pid = fork())<0){
		perror("smallsh");
		return(-1);
	}

	if(pid == 0){//child�� ���
		sigaction(SIGINT, &sa_ign, NULL);
		sigaction(SIGQUIT, &sa_ign, NULL);
		execvp(*cline, cline);
		perror(*cline);
		exit(127);
	}
	else
		fg_pid = pid;
	if(where == BACKGROUND){//background�� ���
		fg_pid = 0;
		printf("[Process id `:%d]\n", pid);
		return(0);
	}

	sigaction(SIGINT, &sa_conf, NULL);
	sigaction(SIGQUIT, &sa_conf, NULL);
	//ctrl+C �� ctrl + \�� ��� handling�ϰ� ó��
	while((ret = wait(&exitstat))!= pid && ret != -1);
	fg_pid = 0;
	return(ret == -1 ? -1 : exitstat);

}
//handling���ִ� �Լ�
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
//pipe���� ���ִ� �Լ�
//com1�� ����� com2�� �Է�����!
int join(char *com1[], char *com2[]){
	int p[2], status;
	pid_t pid1;
	pid_t pid2;
	//pipe����
	if(pipe(p) == -1){
		printf("fork call error");
		exit(1);
	}
	//���� ������
	pid1  = fork();
	switch(pid1){
	case -1:
		perror("fork error");break;
	case 0:
		//��ºκ��� pipe�� ����
		dup2(p[1],1);
		close(p[1]);
		close(p[0]);
		execvp(com1[0], com1);
		exit(0);
	}
	//���� ��� ����
	pid2 = fork();
	switch(pid2){
	case -1: perror("fork");break;
	case 0:
		//�Էºκ��� pipe�� ����
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
		//pipe���� ������ ������ ��� chk�� 1��!
		if(strchr(inbuf, '|') != NULL){
			chk  = 1;

		}
		else{	chk = 0;
		}
		procline();
	}
}
