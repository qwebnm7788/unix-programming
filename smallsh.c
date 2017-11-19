/*
   12131619 컴퓨터공학과 최재원
   2017.11.19
   Shell Project #2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAX_CMD_ARG 10
#define MAX_CMD_GRP 10

const char* prompt = "mysh> ";

char* cmdGrps[MAX_CMD_GRP];
char* cmdVector[MAX_CMD_ARG];
char  cmdline[BUFSIZ];


void fatal(char* str);
void execute_cmdline(char* cmd);
int  parse_cmdgrp(char* cmdGrp);
int  makelist(char* s, const char* delimiters, char** list, int MAX_LIST);
void sigchld_handler(int signo);

int main(int argc, char* argv[]) {
	pid_t pid;
	int token;
	struct sigaction act;

	//install sigchld handler
	sigemptyset(&act.sa_mask);
	act.sa_handler = sigchld_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &act, NULL);

	//SIGINT, SIGQUIT, SIGTSTP signal을 무시하도록 설정
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);				//tcsetpgrp이 background process에서 호출되는 경우 SIGTTOU signal이 날아감 (왜 fork한게 background process group이 되었냐면 setpgid로 새로운 프로세스 그룹을 만들었기 때문)
	//시작과 동시에는 home directory에 존재한다.
	if(chdir(getenv("HOME")) == -1) {
		fatal("shell init fail");
	}

	while(1) {
		//프롬프트 출력 후 cmdline에 명령을 받아온다.
		fputs(prompt, stdout);
		fgets(cmdline, BUFSIZ, stdin);
		cmdline[strlen(cmdline) - 1] = '\0';

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

	memset(cmdGrps, 0, sizeof(cmdGrps));
	count = makelist(cmdline, ";", cmdGrps, MAX_CMD_GRP);

		
	for(i = 0; i < count; ++i) {
		background = parse_cmdgrp(cmdGrps[i]);			//주어진 cmdGrps에 나누어진 명령을 다시 추가 argument로 나누어 cmdVector에 저장한다. (반환값은 background의 경우 1)
		switch(pid = fork()) {
			case -1: 
				fatal("fork error");
			case  0:
				setpgid(0, 0);			//pgid를 자신의 pid와 동일하게 설정 -> 새로운 프로세스 그룹이 생성된다.

				//SIGINT, SIGQUIT, SIGTSTP 을 다시 default handler로 돌려놓는다. -> 이 것에 죽지않는 것은 shell만 되도록
				signal(SIGINT, SIG_DFL);
				signal(SIGQUIT, SIG_DFL);
				signal(SIGTSTP, SIG_DFL);

				//foreground인 경우 잠시동안 터미널 제어권을 가져가도록 한다.
				if(background == 0)
					tcsetpgrp(STDIN_FILENO, getpgid(0));			//자신의 부모(위의 setpgid로 자신으로 변경되어 있음)가 터미널 제어권을 갖는다.

				execvp(cmdVector[0], cmdVector); //자식 프로세스는 주어진 프로세스를 수행한다.
				fatal("execvp error");
			default: 
				if(background == 1) {
					//background task의 경우 waitpid의 WNOHANG 옵션을 이용하여 대기하지 않는다.
					//또한 "방금" fork된 child를 대기해야 하므로 pid를 첫번째 인자로 넘겨준다.
					result = waitpid(pid, &status, WNOHANG);
				}else if(background == 0) {
					//단순히 wait으로 대기하게 되면 임의의 자식이 reap될 수 있기 때문에 waitpid에 pid를 넘겨주고 세번째 인자는
					//0을 넘겨주어 기존 특정 프로세스를 기다리는 wait의 효과를 내어주었다.
					result = waitpid(pid, &status, 0);

					//foreground task가 종료된 후 다시 터미널 제어권을 가져온다.	(standard input -> 0)
					tcsetpgrp(STDIN_FILENO, getpgid(0));
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


//SIGCHLD signal이 오면 그 때 존재하는 모든 zombie process를 reap해준다.
void sigchld_handler(int signo) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}
