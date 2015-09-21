#include<unistd.h>
#include<termios.h>
#include<signal.h>
#include<errno.h>
#include<sys/types.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/wait.h>
#include<assert.h>
#define DELIM " \t\r\n"  //delimiters
char start[100],cp[100],command[1000],cdpath[100];
char* args[100];
int argc,back=1,flag=0;
int shell_terminal=STDERR_FILENO,pnum,cnum,inter;
pid_t shell_pgid;
typedef struct cnode{
	char* arg[100];
	int argc;
	char *in,*out;
	struct cnode* next;
}cnode;
typedef struct pnode{
	char name[100];
	pid_t pid,pgid;
	struct pnode* next;
}pnode;
cnode* commands=NULL;
pnode* pq = NULL;
void getprmpt();

pnode* insp(pnode* head,char n[],pid_t id,pid_t gid){
	//	printf("INSP called\n");
	pnode* tmp=(pnode*)malloc(sizeof(pnode));
	strcpy(tmp->name,n);
	tmp->pid=id;tmp->pgid=gid;
	tmp->next=NULL;
	if(head==NULL)return tmp;
	pnode* r=head;
	while(r->next!=NULL)r=r->next;
	r->next=tmp;
	return head;
}

pnode* deletep(pnode* head,pid_t id){
	if(!head)return head;
	pnode* tmp=head;
	pnode* tmp2;
	while(tmp!=NULL){
		if(tmp->pid!=id){tmp2=tmp;tmp=tmp->next;}
		else break;
	}
	if(tmp==NULL)return head;
	if(tmp==head){
		//		printf("tmp=head\n");
		head=head->next;
		free(tmp);
		return head;
	}
	else{
		tmp2->next=tmp->next;
		free(tmp);
		return head;
	}
}

pnode* search(pnode* head,pid_t id){
	pnode* tmp=head;
	while(tmp){
		if(tmp->pid==id)return tmp;
		else tmp=tmp->next;
	}
}
pid_t search_id(pnode* head,int n){
	int i=1;
	pnode* tmp=head;
	while(tmp){
		if(i==n)return tmp->pid;
		else{
			i++;tmp=tmp->next;
		}
	}
	if(!tmp)return -1;
}

void jobs(pnode* head){
	pnode* tmp=head;
	if(!tmp){
		fprintf(stdout,"No background processes\n");return;
	}
	int i=1;
	assert(tmp!=NULL);
	while(tmp!=NULL){
		assert(tmp!=NULL);
		fprintf(stdout,"[%d] %s [%d]\n",i,tmp->name,tmp->pid);
		i++;tmp=tmp->next;
	}
	return;
}
void kjob(int n,int sig){
	pid_t id=search_id(pq,n);
	if(id==-1){fprintf(stderr,"job not found\n");return;}
	pid_t gid=getpgid(id);
	if(killpg(gid,sig)<0)perror("Error sending signal\n");
	else fprintf(stderr,"signal sent\n");
}
void overkill(pnode* r){
	int i=1;
	pnode* tmp=r;
	while(tmp){
		kjob(i,9);
		tmp=tmp->next;i++;
	}
	return;
}
void fg(int n){
	pid_t id;
	id=search_id(pq,n);
	if(id==-1){
		fprintf(stderr,"No such job\n");return;
	}
	pnode* p=search(pq,id);
	fprintf(stderr,"%s\n",p->name);
	pid_t gid=getpgid(id);
	int st;
	tcsetpgrp(shell_terminal,gid);
	if(killpg(gid,SIGCONT<0))perror("Unable to continue\n");
	waitpid(id,&st,WUNTRACED);
	if(!WIFSTOPPED(st))pq=deletep(pq,id);
	else fprintf(stderr,"[%d]+ Stopped %s\n",id,(search(pq,id))->name);
	tcsetpgrp(shell_terminal,shell_pgid);
}
void pinfo(pid_t pid){
	char path[300],state[50],id[50],vm[50],expath[300],exname[100];
	char* tok;
	FILE *f;
	sprintf(path,"/proc/%d/status",pid);
	sprintf(expath,"/proc/%d/exe",pid);
	if(f=fopen(path,"r")){
		fscanf(f,"%*[^\n] %[^\n] %*[^\n] %*[^\n] %[^\n] %*[^\n] %*[^\n] %*[^\n] %*[^\n] %*[^\n] %*[^\n] %*[^\n] %[^\n]",state,id,vm);
		tok=strtok(id,DELIM);
		tok=strtok(NULL,DELIM);
		printf("pid -- %s\n",tok);
		tok=strtok(state,DELIM);
		tok=strtok(NULL,DELIM);
		printf("Process Status -- {%s} memory\n",tok);
		tok=strtok(vm,DELIM);tok=strtok(NULL,DELIM);
		printf("-- %s {Virtual Memory}\n",tok);
		readlink(expath,exname,100);
		printf("Executable Path -- %s\n",exname);
	}
	else
		perror("No such process\n");
}
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
			}
			tmp->arg[i]=NULL;
			tmp->argc--;
			if(i!=argc-1){
				tmp->in=(char*)malloc(100*sizeof(char));
				strcpy(tmp->in,args[i+1]);
			}
			//tmp->in=args[i+1]
			i+=2;
		}
		else if(strcmp(args[i],">")==0){
			tmp->argc--;
			if(i==argc-1){
				perror("No output file\n");
			}
			tmp->arg[i]=NULL;
			tmp->argc--;
			if(i!=argc-1){
				tmp->out=(char*)malloc(100*sizeof(char));
				strcpy(tmp->out,args[i+1]);
			}
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
	shell_terminal=STDERR_FILENO;
	back=0;
	getcwd(start,100);
	signal (SIGINT, SIG_IGN);
	signal (SIGTSTP, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);
	signal (SIGTTIN, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);
	shell_pgid=getpid();
	// printf("shellid %d\n",shell_pgid);
	if(setpgid(shell_pgid,shell_pgid)<0)
	{
		perror("Can't make shell a member of it's own process group");
		_exit(1);
	}
	tcsetpgrp(shell_terminal,shell_pgid);
	//printf("fgidshell %d\n",tcgetpgrp(shell_terminal));
}
void handler(int sig){
	if(sig==SIGINT)
	{
		fprintf(stderr,"\n");
		getprmpt();
	}
	else if(sig==SIGCHLD){
		int status;
		pid_t pid;
		while((pid=waitpid(-1,&status,WNOHANG))>0){
			if(pid!=-1&&pid!=0){
				if(WIFEXITED(status)){
					pnode* pr=search(pq,pid);
					if(pr){
						fprintf(stdout,"%s with pid  %d exited normally  ",pr->name,pr->pid);
						flag=1;
						pq=deletep(pq,pid);
					}
				}
				else if(WIFSIGNALED(status)){
					pnode* pr=search(pq,pid);
					if(pr){
						fprintf(stdout,"%s with pid %d signalled to kill  ",pr->name,pr->pid);flag=1;
						pq=deletep(pq,pid);
					}
				}
			}
		}
	}
	//fflush(stdout);
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
	if(f){
		if(strstr(cp,start)==NULL){
			fprintf(stderr,"%s",cp);
		}
		else{
			int l =strlen(start);
			fprintf(stderr,"~%s",cp+l);
		}
		fprintf(stderr,">");
	}
	else fprintf(stderr,"%s\n",cp);
}
void getprmpt(){
	char name[100],host[100];
	getlogin_r(name,100);
	gethostname(host,100*sizeof(char));
	getcwd(cp,100);
	fprintf(stderr,"%s@%s:",name,host);
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
	else if(strcmp(r->arg[0],"pinfo")==0){
		if(argc==1){
			pid_t id=getpid();
			pinfo(id);
		}
		else pinfo(atoi(r->arg[1]));
		fflush(stdout);return;
	}
	else if(strcmp(r->arg[0],"kjob")==0){
		if(r->argc!=3){
			fprintf(stderr,"Wrong no. of arguements\n");return;
		}
		kjob(atoi(r->arg[1]),atoi(r->arg[2]));fflush(stdout);return;
	}
	else if(strcmp(r->arg[0],"overkill")==0){overkill(pq);return;}
	else if(strcmp(r->arg[0],"fg")==0){
		if(r->argc==1){
			fprintf(stderr,"Process No. required\n");return;
		}
		else if(r->argc>2){
			fprintf(stderr,"Takes only one arguement\n");return;
		}
		fg(atoi(r->arg[1]));
		fflush(stdout);return;
	}
	else if(strcmp(r->arg[0],"pwd")==0){getpath(0);fflush(stdout);return;}
	else if(strcmp(r->arg[0],"jobs")==0){
		jobs(pq);
		fflush(stdout);return;
	}
	pid_t pid=fork();
	if(pid<0){
		perror("Child not created\n");
		_exit(-1);
	}
	if(pid==0){
		setpgid(getpid(),getpid());
		pid_t id=getpid();
		//if(back)pq=insp(pq,command,id,id);
		if(back==0)tcsetpgrp(shell_terminal,getpid());
		signal (SIGINT, SIG_DFL);
		signal (SIGQUIT, SIG_DFL);
		signal (SIGTSTP, SIG_DFL);
		signal (SIGTTIN, SIG_DFL);
		signal (SIGTTOU, SIG_DFL);
		signal (SIGCHLD, SIG_DFL);
		run(r);
		_exit(0);
	}
	if(back==0){
		int status;
		tcsetpgrp(shell_terminal,pid);
		waitpid(pid,&status,WUNTRACED);
		if(WIFSTOPPED(status)){
			pq=insp(pq,command,pid,pid);
			fprintf(stderr,"\n[%d]+ stopped",pid);
		}
		else{
			pq=deletep(pq,pid);
		}
		tcsetpgrp(shell_terminal,shell_pgid);
	}
	if(back){
		pq=insp(pq,command,pid,pid);
	}
	fflush(stdout);
}
void pipe_execute(cnode* head){
	if(cnum==1){execute(commands);return;}
	pnum=cnum-1;
	pid_t pid[cnum+1],gid;
	int pipes[pnum+1][2],i=0,j;
	for(j=0;j<pnum+1;j++){
		if(pipe(pipes[j])<0){
			perror("pipe error\n");
			return;
		}
	}
	while(head!=NULL&&i<cnum){
		pid[i]=fork();
		if(pid[i]<0){
			perror("Child process error\n");
			return;
		}
		if(pid[i]==0){
			gid=getpid();      //make first member as group leader
			if(i==0)setpgid(gid,gid);
			else setpgid(getpid(),gid);
			if(i>0){
				dup2(pipes[i-1][0],0);
				if(i!=cnum-1)dup2(pipes[i][1],1);

			}
			else{
				dup2(pipes[0][1],1);
			}
			for(j=0;j<pnum+1;j++){close(pipes[j][0]);close(pipes[j][1]);}
			run(head);
		}
		i++;
		head=head->next;
	}
	for(j=0;j<pnum+1;j++){close(pipes[j][0]);close(pipes[j][1]);}
	if(!back){
		tcsetpgrp(shell_terminal,gid);
		for(j=0;j<cnum;j++){
			waitpid(pid[j],NULL,0);
		}
		tcsetpgrp(shell_terminal,shell_pgid);
	}
}
int main(){
	fflush(stdout);
	init();
	int i=1,c;
	while(i){
		if(signal(SIGINT,handler)==SIG_ERR){
			perror("Signal error\n");
		}
		if(signal(SIGCHLD,handler)==SIG_ERR){
			perror("Signal error\n");
		}
		if(flag){printf("\n");flag=0;}
		getprmpt();
		scanf("%[^\n]",command);
		if(strlen(command)>0){
			if(command[strlen(command)-1]=='&'){
				command[strlen(command)-1]='\0';
				back=1;
			}
		}
		cnum=pipeparse(command);
		if(cnum>0)if(strcmp(commands->arg[0],"exit")==0||strcmp(commands->arg[0],"quit")==0)break;
		pipe_execute(commands);
		freec();
		scanf("%*c");
		back=0;
		commands=NULL;
		//i--;
	}
	return 0;
}

