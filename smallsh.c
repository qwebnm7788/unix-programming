/*
   12131619 컴퓨터공학과 최재원
   2017.11.5
   Shell Project #1
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CMD_ARG 10
#define MAX_CMD_GRP 10

const char* prompt = "mysh> ";

char* cmdGrps[MAX_CMD_GRP];
char* cmdVector[MAX_CMD_ARG];
char  cmdline[BUFSIZ];


void fatal(char* str);
void execute_cmdline(char* cmd);
int parse_cmdgrp(char* cmdGrp);
int  makelist(char* s, const char* delimiters, char** list, int MAX_LIST);

int main(int argc, char* argv[]) {
	pid_t pid;
	int token;
	int background;

	//시작과 동시에는 home directory에 존재한다.
	if(chdir(getenv("HOME")) == -1) {
		fatal("shell init fail");
	}

	while(1) {
		//프롬프트 출력 후 cmdline에 명령을 받아온다.
		fputs(prompt, stdout);
		fgets(cmdline, BUFSIZ, stdin);
		cmdline[strlen(cmdline) - 1] = '\0';

		background = 0;

		if(!strcmp(cmdline, "exit")) {
			//exit은 fork를 사용하지 않고 해당 쉘에서 곧바로 종료
			printf("Good bye~\n");
			exit(0);
		}else if(cmdline[0] == 'c' && cmdline[1] == 'd') {
			//cd 명령의 경우 우선 cd라는 이름이 파일이 shell bullitin 명령어 이기 때문에 /bin/sh, 즉 또 다른 쉘을 fork로 생성 후 -c옵션을 통해
			//주어진 입력을 수행하도록 할 수 있지만 현재 진행중인 프로세스가 아닌 생성된 프로세스의 working directory가 변경되기 때문에
			//chdir함수를 통하여 직접 이동시켜주어야 한다.
			makelist(cmdline, " \t", cmdVector, MAX_CMD_ARG);
			
			if(cmdVector[1] == NULL || !strcmp(cmdVector[1], "~")) {
				//cd에 주어진 인자가 아무것도 없거나 ~ 인 경우 home directory로 이동하게 해주어야 하는데
				// ~ 은 실제 홈디렉토리의 주소가 아니기 때문에 환경변수에 저장되어 있는 값을 이용해 주어야 한다.
				cmdVector[1] = getenv("HOME");
			}

			if(chdir(cmdVector[1]) == -1) {
				//에러가 발생 한 경우 에러 메세지 출력
				perror(strerror(errno));
			}

		}else {
			//얻어낸 cmdline을 이용하여 execute_cmdline함수 실행
			execute_cmdline(cmdline);
		}

		fflush(stdin);
		fflush(stdout);
	}

	return 0;
}

//str이라는 에러메세지를 stderr에 출력 후 종료
void fatal(char* str) {
	perror(str);
	exit(1);
}


//fork를 이용하여 execute_cmdgrp 함수를 통해 하나씩 실행해준다.
//parent는 wait을 통해 대기하기 때문에 한번의 루프에 하나의 명령을 수행하는 형태가 된다.
void execute_cmdline(char* cmdline) {
	pid_t pid;
	int status;
	int count = 0;
	int i = 0;
	int background;
	int result;

	count = makelist(cmdline, ";", cmdGrps, MAX_CMD_GRP);

		
	for(i = 0; i < count; ++i) {
		background = parse_cmdgrp(cmdGrps[i]);			//주어진 cmdGrps에 나누어진 명령을 다시 추가 argument로 나누어 cmdVector에 저장한다. (반환값은 background의 경우 1)
		switch(pid = fork()) {
			case -1: fatal("fork error");
			case  0: execvp(cmdVector[0], cmdVector); fatal("execvp error");		//자식 프로세스는 주어진 프로세스를 수행한다.
			default: 
				if(background == 1) {
					//background task의 경우 waitpid의 WNOHANG 옵션을 이용하여 대기하지 않는다.
					result = waitpid(-1, &status, WNOHANG);
				}else if(background == 0) {
					wait(&status);
				}else {
					fatal("execute_cmdline error");
				}
				fflush(stdin);
				fflush(stdout);
		}
	}
}

//cmdGrp으로 주어진 명령을 tap이나 띄어쓰기로 나누어 (인자를 구분하여)
//cmdVector 배열에 담아준다. background task의 경우 1을 아닌 경우 0을 반환한다.
int parse_cmdgrp(char* cmdGrp) {
	int count = 0;
	int ret = -1;
	count = makelist(cmdGrp, " \t", cmdVector, MAX_CMD_ARG);			//하나의 명령에 들어있는 여러 옵션들을 makelist를 이용하여 cmdVector배열에 넣어준다.

	if(!strcmp(cmdVector[count - 1], "&")) {
		//background process인 경우 내부에서 fork을 이용하여 새롭게 하나의 프로세스를 생성 후 주어진 명령을 수행하게 한 뒤
		cmdVector[count - 1] = '\0';		//null 로 치환
		ret = 1;
	}else {
		ret = 0;
	}

	return ret;
}

//주어진 s를 delimiters를 구분자로 하여 나누어 list배열에 최대 MAX_LIST개를 넣어준다.
//반환되는 값은 list 배열에 넣어준 token의 개수
int makelist(char* s, const char* delimiters, char** list, int MAX_LIST) {
	int numTokens = 0;
	char* snew = NULL;

	if((s == NULL) || (delimiters == NULL)) return -1;

	snew = s + strspn(s, delimiters);		//delimiter skip then snew pointer is pointing to start of the command

	if((list[numTokens] = strtok(snew, delimiters)) == NULL) {
		//token이 하나도 없다면 0을 반환한다.
		return numTokens;
	}

	numTokens = 1;
	while(1) {
		if((list[numTokens] = strtok(NULL, delimiters)) == NULL) break;	//모두 다 찾은 경우
		if(numTokens == (MAX_LIST - 1)) return -1;		//최대 token의 수를 초과한 경우 -1 반환
		numTokens++;
	}

	return numTokens;
}
