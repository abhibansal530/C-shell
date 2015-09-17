#include<unistd.h>
#include<termios.h>
#include<signal.h>
#include<errno.h>
#include<sys/types.h>
#include<fcntl.h>
#include<stdio.h>
#include <sys/prctl.h>
#include<stdlib.h>
#include<string.h>
#include<sys/wait.h>
#include<assert.h>
#define DELIM " \t\r\n"  //delimiters
char start[100],cp[100],command[1000],cdpath[100];
char* args[100];
int argc;
int shell_terminal=STDERR_FILENO,pnum,cnum;
pid_t shell_pgid;
typedef struct cnode{
	char* arg[100];
	int argc;
	char *in,*out;
	struct cnode* next;
}cnode;
cnode* commands=NULL;
cnode* ins(cnode* root){
	cnode* tmp=(cnode*)malloc(sizeof(cnode));
	tmp->argc=argc;
	tmp->next=NULL;
	tmp->in=tmp->out=NULL;
	int i=0,flag=0;
	if(argc==0){tmp->arg[0]=NULL;return tmp;}
	while(i<argc){
		tmp->arg[i]=(char*)malloc(100*sizeof(char));
		//tmp->arg[i]=args[i];
		strcpy(tmp->arg[i],args[i]);
		if(strcmp(args[i],"<")==0){
			tmp->argc--;
			if(i==argc-1){
				perror("No input file\n");
				return root;
			}
			tmp->arg[i]=NULL;
			tmp->argc--;
			tmp->in=(char*)malloc(100*sizeof(char));
			strcpy(tmp->in,args[i+1]);
			//tmp->in=args[i+1]
			i+=2;
		}
		else if(strcmp(args[i],">")==0){
			tmp->argc--;
			if(i==argc-1){
				perror("No output file\n");
				return root;
			}
			tmp->arg[i]=NULL;
			tmp->argc--;
			tmp->out=(char*)malloc(100*sizeof(char));
			//tmp->out=args[i+1];
			strcpy(tmp->out,args[i+1]);
			i+=2;
		}
		else i++;
	}
	if(root==NULL)return tmp;
	cnode* r=root;
	while(r->next!=NULL)r=r->next;
	r->next=tmp;
	return root;
}
void run(cnode* r){
	int input,output;
	if(r->in!=NULL){
		input=open(r->in,O_RDONLY);
		dup2(input,0);
		close(input);
	}
	if(r->out!=NULL){
		output=open(r->out,O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
		dup2(output,1);
		close(output);
	}
	if(execvp(r->arg[0],r->arg)<0){
		perror("Wrong Command\n");
		return;
	}
}
void init(){
	getcwd(start,100);
	/*shell_pgid=getpid();
	  printf("shellid %d\n",shell_pgid);
	  if(setpgid(shell_pgid,shell_pgid)<0)
	  {
	  perror("Can't make shell a member of it's own process group");
	  _exit(1);
	  }
	  tcsetpgrp(shell_terminal,shell_pgid);
	  printf("fgid %d\n",tcgetpgrp(shell_terminal));*/
}
void freeargs(){
	int i;
	for(i=0;i<100;i++)args[i]=NULL;
}
void freec(){
	int i;
	for(i=0;i<1000;i++)command[i]='\0';
}
void print(cnode* r){
	int i=0;
	while(i<r->argc)printf("%s\n",r->arg[i++]);
	if(r->in!=NULL)printf("in: %s\n",r->in);
	if(r->out!=NULL)printf("out: %s\n",r->out);
}
void parse(char* command){
	freeargs();
	if(strlen(command)==0){return;}
	char* cmd;
	cmd=strtok(command,DELIM);
	int k=0;
	while(cmd){
		//printf("%s ",cmd);
		args[k++]=cmd;
		cmd=strtok(NULL,DELIM);
	}
	//printf("\n");
	argc=k;
}
int pipeparse(char* maincomm){
	char* pcmd;
	char* save1;
	int ret=0;
	pcmd=strtok_r(maincomm,"|",&save1);
	//printf("%s\n",pcmd);
	while(pcmd){
		//printf("%s\n",pcmd);
		ret++;
		parse(pcmd);
		commands=ins(commands);
		pcmd=strtok_r(NULL,"|",&save1);
	}
	return ret;
}
void getpath(int f){
	getcwd(cp,100);
	if(strstr(cp,start)==NULL){
		printf("%s",cp);
	}
	else{
		int l =strlen(start);
		printf("~%s",cp+l);
	}
	if(f)printf(">");
	else printf("\n");
}
void getprmpt(){
	char name[100],host[100];
	getlogin_r(name,100);
	gethostname(host,100*sizeof(char));
	getcwd(cp,100);
	printf("%s@%s:",name,host);
	getpath(1);
}
int cd(){
	if(argc==1||(args[1][0]=='~'&&strlen(args[1])==1)){              //only cd,thus go to home
		if(chdir(start)<0){
			perror("Error\n");
			return -1;
		}
		return 0;
	}
	if(args[1][0]=='~'){
		strcpy(cdpath,start);
		strcat(cdpath,args[1]+1);
		if(chdir(cdpath)<0){
			perror("Error\n");
			return -1;
		}
		return 0;
	}
	else{

		if(chdir(args[1])<0){
			perror("Error\n");
			return -1;
		}
		return 0;
	}
}
void execute(cnode* r){
	if(r->arg[0]==NULL){return;}
	if(strcmp(r->arg[0],"cd")==0){
		if(cd()==-1){
			perror("error\n");
		}
		fflush(stdout);
		return;
	}
	else if(strcmp(r->arg[0],"pwd")==0){getpath(0);fflush(stdout);return;}
	//else if(strcmp(r->arg[0],"exit")==0||strcmp(r->arg[0],"quit")==0){fflush(stdout);ret
	int status;
	pid_t pid=fork();
	if(pid<0){
		perror("Child not created\n");
		_exit(-1);
	}
	if(pid==0){
		//setpgid(0,0);
		//setpgid(getpid(),getpid());
		//printf("chpgpid %d\n",getpgid(getpid()));
		//printf("chppid %d\n",getppid());
		//printf("fgid %d\n",tcgetpgrp(shell_terminal));
		//tcsetpgrp(shell_terminal,getpid());
		/*if(execvp(r->arg[0],r->arg)!=0){
			perror("Invalid command\n");
		}*/
		run(r);
		_exit(0);
	}
	//else tcsetpgrp(0,getpgrp());
	wait(&status);
}
void pipe_execute(cnode* head){
	if(cnum==1){execute(commands);return;}
	pnum=cnum-1;
	pid_t pid[cnum+1];
	int pipes[pnum+1][2],i=0,j;
	for(j=0;j<pnum+1;j++){
		if(pipe(pipes[j])<0){
				perror("pipe error\n");
				return;
		}
	}
	//print(head);
	//print(head->next);
	while(head!=NULL&&i<cnum){
		//print(head);
		pid[i]=fork();
		if(pid[i]<0){
			perror("Child process error\n");
			return;
		}
		if(pid[i]==0){
			if(i>0){
				dup2(pipes[i-1][0],0);
				if(i!=cnum-1)dup2(pipes[i][1],1);
				//for(j=0;j<pnum+1;j++){close(pipes[j][0]);close(pipes[j][1]);}
				//run(head);

			}
			else{
				dup2(pipes[0][1],1);
				//for(j=0;j<pnum+1;j++){close(pipes[j][0]);close(pipes[j][1]);}
				//run(head);
			}
		        for(j=0;j<pnum+1;j++){close(pipes[j][0]);close(pipes[j][1]);}
			run(head);
		}
		i++;
		head=head->next;
	}
	for(j=0;j<pnum+1;j++){close(pipes[j][0]);close(pipes[j][1]);}
	for(j=0;j<cnum;j++)waitpid(pid[j],NULL,0);
}
int main(){
	init();
	int i=1;
	while(i){
		getprmpt();
		scanf("%[^\n]",command);
		cnum=pipeparse(command);
		//print(commands);
		if(cnum>0)if(strcmp(commands->arg[0],"exit")==0||strcmp(commands->arg[0],"quit")==0)break;
		pipe_execute(commands);
		fflush(stdout);
		freec();
		scanf("%*c");
		commands=NULL;
		//i--;
	}
	return 0;
}

