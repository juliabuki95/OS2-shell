#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <wait.h>
#include <errno.h>
#include <fcntl.h>


void sig_handler_parent(){
    struct sigaction action;
    memset(&action,0,sizeof(action));
    action.sa_handler=SIG_IGN;
    action.sa_flags=SA_RESTART;
    //sigint ignor
    if(sigaction(SIGINT,&action,NULL)==-1){
        perror("SIGINT handling failed");
        exit(1);
    }
}
void sig_handler_child(){
    struct sigaction action;
    memset(&action,0,sizeof(action));
    action.sa_handler=SIG_DFL;
    action.sa_flags=SA_RESTART;
    //sigint default
    if(sigaction(SIGINT,&action,NULL)==-1){
        perror("SIGINT handling failed");
        exit(1);
    }
}
void wait_for_z(){
    while(waitpid(-1,NULL,WNOHANG)>0){}
}
void sig_handler_zombies(){
    struct sigaction action;
    memset(&action,0,sizeof(action));
    action.sa_handler=wait_for_z;
    action.sa_flags=SA_NOCLDSTOP|SA_RESTART;
    // kill all child zombies
    if(sigaction(SIGCHLD,&action,NULL)==-1){
        perror("SIGCHLD handling failed");
        exit(1);
    }
}
// checks for error in wait
int child_handle(int status){
    if(status==-1  && errno != ECHILD){
        perror("Wait error");
        return 0;
    }
    return 1;
}

int prepare(void){
    
    sig_handler_parent();
    sig_handler_zombies();
    return 0;
}

int finalize(void){
    return 0;
}
//regular process
int process_reg(char** arglist){
    int pid = fork();
    int status;
    if(pid==-1){
        perror("Failed forking");
        exit(1);
    }
    else if(pid==0){
        sig_handler_child();
        execvp(arglist[0],arglist);
        perror(arglist[0]);
        exit(1);
    }
    else{
        //wait for child
        return(child_handle(wait(&status)));
    }


}
//pocess with &
int process_back(char** arglist,int count){
    arglist[count-1]=NULL;
    int pid = fork();
    if(pid==-1){
        perror("Failed forking");
        exit(1);
    }
    else if(pid==0){
        execvp(arglist[0],arglist);
        perror(arglist[0]);
        exit(1);
    }
    return 1;
}
//pipe process
int process_pip(char** arglist,int i){
    
    arglist[i]=NULL;
    int ret;
    int pipefd[2];
    if( -1 == pipe( pipefd ) )
    {
        perror( "Error in piping" );
        exit( 1 );
    }
    pid_t pid1=fork();

    if(pid1==-1){
        perror("Failed forking child1 in pip");
        close(pipefd[0]);
        close(pipefd[1]);
        exit(1);
    }
    //before pipe
    else if(pid1==0){
        sig_handler_child();
        close(pipefd[0]);
        //stdout descriptor is pipefd[1]
        if(dup2(pipefd[1],STDOUT_FILENO)<0){
            perror("Failed in dup2 pid1");
            exit(1);
        }
        close(pipefd[1]);
        execvp(arglist[0],arglist);
        perror(arglist[0]);
        exit(1);
    }
    else {
      
        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("Failed forking child2 in pip");
            exit(1);
        } else if (pid2 == 0) {
            //after pipe
            sig_handler_child();
            close(pipefd[1]);
            //stdin descriptor is pipefd[0]
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("Faild in dup2 pid2");
                exit(1);
            }
            close(pipefd[0]);
            execvp(arglist[i+1], (arglist+i+1));
            perror(arglist[i+1]);
            exit(1);
        } else {
            //parent
            close(pipefd[0]);
            close(pipefd[1]);
            ret = child_handle(waitpid(pid1, NULL, WUNTRACED));
            if (ret == 0) {
                return 0;
            }
            ret = child_handle(waitpid(pid2, NULL, WUNTRACED));
            if (ret == 0) {
                return 0;
            }


        }
    }
        return 1;

}

int process_red(char** arglist,int count) {
    remove(arglist[count-1]);
    int ret;
    int fptr = open(arglist[count-1],O_CREAT|O_RDWR,0777);
    if(fptr<0){
            perror("Failed OPENING FILE");
            exit(1);
    }
    arglist[count-2]=NULL;
    int pid = fork();
    if(pid==-1){
        perror("Failed forking");
        exit(1);
    }
    else if(pid==0){
        sig_handler_child();
        //stdout descriptor is file
        if(dup2(fptr,STDOUT_FILENO)<0){
            perror("Failed in dup2");
            exit(1);
        }
        close(fptr);
        execvp(arglist[0],arglist);
        perror(arglist[0]);
        exit(1);
    }
    else{
        //wait for child
        ret=child_handle(waitpid(pid, NULL , WUNTRACED));
        close(fptr);

    }
    return ret;
}

int process_arglist(int count, char** arglist){
    int symbol=0;
    int i;
    int ret;
    //check command type
    if(!(strcmp(arglist[count-1],"&"))){
        symbol=1;
    }
    else{
        for(i=0;i<count;i++){
            if(!(strcmp(arglist[i],"|"))){
                symbol=2;
                break;
            }
            if(!(strcmp(arglist[i],">"))){
                symbol=3;
                break;
            }
        }
    }
    //each command has its one function
    switch (symbol) {
        case 1:
            ret=process_back(arglist,count);
            break;
        case 2:
            ret=process_pip(arglist,i);
            break;
        case 3:
            ret=process_red(arglist,count);
            break;
        default:
            ret=process_reg(arglist);
    }
    return ret;

}