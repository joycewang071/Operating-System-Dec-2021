//Filename: monitor.c

//Development platform: WSL
//Remark: Everything except the running statistics for the last command with pipes since it's a parent process 
//        e.g. the running statistics of "wc -c"  in " cat m.c ! grep io ! wc -c "
// Basically, I handle this assignment with 3 cases 
// 1. without pipe: execvp
// 2. with 1 pipe: communication between parent and child 
// 3. with more than 1 pipe: communication using a shared tempory file among children and the last command as a independent process 


#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>


int argnum;                                     // number of the input argument
// char* argv[1000];                            // input argument array
char* command[1000];                            // the whole command
int i, j;                                       // variable for looping


char* sigcodeConv(int sig) {// convert signal codes to standardized signal names
    switch(sig){
        case 1 : return "SIGHUP";
        case 2 : return "SIGINT";
        case 3 : return "SIGQUIT";
        case 4 : return "SIGILL";
        case 5 : return "SIGTRAP";
        case 6 : return "SIGABRT or SIGIOT";
        case 7 : return "SIGBUS";
        case 8 : return "SIGFPE";
        case 9 : return "SIGKILL";
        case 10 : return "SIGUSR1";
        case 11 : return "SIGSEGV";
        case 12 : return "SIGUSR2";
        case 13 : return "SIGPIPE";
        case 14 : return "SIGALRM";
        case 15 : return "SIGTERM";
        case 16 : return "SIGSTKFLT";
        case 17 : return "SIGCHLD";
        case 18 : return "SIGCONT";
        case 19 : return "SIGSTOP";
        case 20 : return "SIGTSTP";
        case 21 : return "SIGTTIN";
        case 22 : return "SIGTTOU";
        case 23 : return "SIGURG";
        case 24 : return "SIGXCPU";
        case 25 : return "SIGXFSZ";
        case 26 : return "SIGVTALRM";
        case 27 : return "SIGPROF";
        case 28 : return "SIGWINCH  ";
        case 29 : return "SIGIO or SIGPOLL";
        case 30 : return "SIGPWR";
        case 31 : return "SIGSYS or SIGUNUSED";

     return "Unknown signal code";
 }
}

int commandWithPipe(char *command[argnum]) {
    

    int pipnum=0; 

    for(j = 0; j<argnum; j++) {
        if (strcmp(command[j], "!") == 0)
        { pipnum++;}
    } // find the number of pipes
   

    if (pipnum == 1 )  // use the parent-child process to implement the simpliest case with only one pipe
    { 
        
        for(j = 0; j<argnum; j++) {
        if (strcmp(command[j], "!") == 0)
            break; } // j stores the position of the logic pipe '!'
                        
        char *outputBuf[1000]; 
        for (i = 0; i < j ; i++) {
            outputBuf[i] = command[i];
        } //outputBuf is the command before the logic pipe '!'

        char *inputBuf[1000];
        for (i = 0; i < argnum-1- j; i++) {
            inputBuf[i] = command[j + 1 + i];
        } //inputBuf is the command after the logic pipe '!'
     
        int pd[2];
        pid_t pid;
        if (pipe(pd) < 0) {
            perror("pipe()");
            exit(1);
        } // create pipe

        struct timeval start, end;
        int time_used;  
        gettimeofday(&start, NULL); // real time at the start of the program

        pid = fork();

        if (pid < 0) {
            perror("fork()");
            exit(1);
        }

        //  close the read end of the child process
        if (pid == 0) {                      
            
            printf("Process with id: %d created for the command: %s \n", getpid(),outputBuf[0]);
            close(pd[0]);                   // STDOUT_FILENO  refers to pd[1]
            dup2(pd[1], STDOUT_FILENO);     
            execvp(outputBuf[0], outputBuf);
            if (pd[1] != STDOUT_FILENO) {
                close(pd[1]);
            }
            exit(1);
        }
        // parent process read output from pipe
        else {                              
    
            int status;
            struct rusage usage;
            signal(SIGINT, SIG_IGN);
            wait4(pid, &status, 0, &usage);
            
            gettimeofday(&end, NULL);  // real time at the end of the program
            
            //error handling of the child process(first command)
            if (WIFEXITED(status)) {	
                printf("The command %s terminated with returned status code = %d\n\n", outputBuf[0],WEXITSTATUS(status));
                if(WEXITSTATUS(status)){ // (WEXITSTATUS(status)==1 means the child process ended with errors 
                    if (outputBuf[0][0] != '.' && outputBuf[0][0]!='/'){
                        printf("exec: : Not a basic linux command i.e. not in /usr/bin \n\n");
                    }
                    else{
                        printf("exec: : No such file or directory\n");
                        printf("Debug hint: incorrect filename, incorrect path etc.\n\n");
                    }
                printf("monitor experienced an error in starting the command: %s \n\n", outputBuf[0]);
                }
            }  // if WIFEXITED(status)== true, WEXITSTATUS(status) returns the termination status
            if(WIFSIGNALED(status)){
                printf("The command %s is interrupted by the signal number = %d (%s)\n\n", outputBuf[0], WTERMSIG(status), strsignal(WTERMSIG(status)));
            }  //if WIFSIGNALED(status)== true,  WTERMSIG(status) returns the number of the signal that terminated the child process
            

            //print running statistics of the child process(first command)
            printf("The running statistics for %s as follows: \n", outputBuf[0]);
            printf("real: %.03f s, ",(double)(end.tv_sec + end.tv_usec / 1000000 - start.tv_sec - start.tv_usec / 1000000));
            printf("user: %ld.%03ld s, system: %ld.%03ld s \n",usage.ru_utime.tv_sec, usage.ru_utime.tv_usec/1000, usage.ru_stime.tv_sec, usage.ru_stime.tv_usec/1000);
            printf("no. of page faults: %ld \n", usage.ru_minflt+usage.ru_majflt);
            printf("no. of context switches: %ld \n\n", usage.ru_nvcsw+usage.ru_nivcsw);
            
            close(pd[1]);                    // close write end
            dup2(pd[0], STDIN_FILENO);       // pd[0]  refers to STDIN_FILENO
            execvp(inputBuf[0], inputBuf);


            printf("Error in starting the command: %s \n", inputBuf[0]);
            if (pd[0] != STDIN_FILENO) {
                close(pd[0]);
            } 
            if (inputBuf[0][0] != '.' && inputBuf[0][0]!='/'){
                printf("exec: : Not a basic linux command i.e. not in /usr/bin \n\n");
            }
            else{
                printf("exec: : No such file or directory\n");
                printf("Debug hint: incorrect filename, incorrect path etc.\n\n");
            }
            printf("monitor experienced an error in starting the command: %s \n", inputBuf[0]);
            
        }   

        return 1;
    }
    //Only for the child process:
    //Print out the termination status or the signal experienced by the child process
    //Print out the wall clock time, user time, system time, and the number of context switches 
    //NB: Given that the second command would be a parent process, so it will not print out the above information
    
    else if (pipnum > 1)// use a temporary shared file to implement the command with more than one pipes
    {
        char *f="temp4pipes.txt";
        // initialize the commands with multiple pipes
        char *mulcmd[100][100];
        int ar=0; //index of the command
        int a=0;//index of the argument of one command
        for(int i=0;i<argnum;i++) {
            if(strcmp(command[i],"!")==0) {
                mulcmd[ar][a++]=NULL;
                ar++;
                a=0;
            }
            else mulcmd[ar][a++]=command[i];
        }



        int pid=fork();

        if(pid<0) {
            perror("fork error\n");
            exit(0);
        }
        
        if(pid==0) 
        {
            int icmd;//index of the currently processing command
            for(icmd=0;icmd<pipnum;icmd++) {
         
                struct timeval start, end;
                int time_used;  
                gettimeofday(&start, NULL); // real time at the start of the program

                int pid2=fork();

                if(pid2<0) {
                    perror("fork error\n");
                    exit(0);
                }
                
                else if(pid2==0) { //child process

                    printf("Process with id: %d created for the command: %s \n", getpid(),mulcmd[icmd][0]);

                    if(icmd>0) { //if this is not the first command, close standard input and read from file
                        close(0); 
                        int fd=open(f,O_RDONLY);
                    }		
                    close(1);
                    remove(f);//delete the temporary file
                    int fd=open(f,O_WRONLY|O_CREAT|O_TRUNC,0666);
                    execvp(mulcmd[icmd][0],mulcmd[icmd]);

                    //if error
                    if (mulcmd[icmd][0][0] != '.' && mulcmd[icmd][0][0]!='/'){
                        printf("exec: : Not a basic linux command i.e. not in /usr/bin \n\n");
                    }
                    else{
                        printf("exec: : No such file or directory\n");
                        printf("Debug hint: incorrect filename, incorrect path etc.\n\n");
                    }
                    printf("monitor experienced an error in starting the command: %s \n\n", mulcmd[icmd][0]);
                    exit(1);
                    
                }
                else {
                    int status;
                    struct rusage usage;
                    signal(SIGINT, SIG_IGN);
                    wait4(pid2, &status, 0, &usage);   

                    gettimeofday(&end, NULL);  // real time at the end of the program

                    if (WIFEXITED(status))  // if WIFEXITED(status)== true, WEXITSTATUS(status) returns the termination status
                    {	
                        printf("The command %s terminated with returned status code = %d\n\n", mulcmd[icmd][0],WEXITSTATUS(status));
                    }
                    if(WIFSIGNALED(status)){
                        printf("The command %s is interrupted by the signal number = %d (%s)\n\n", mulcmd[icmd][0], WTERMSIG(status), sigcodeConv(WTERMSIG(status)));
                    }  //WTERMSIG can evaluates to the number of the signal that terminated the child process if the value of WIFSIGNALED(status) is nonzero.
                    

                    printf("real: %.03f s, ",(double)(end.tv_sec + end.tv_usec / 1000000 - start.tv_sec - start.tv_usec / 1000000));
                    printf("user: %ld.%03ld s, system: %ld.%03ld s \n",usage.ru_utime.tv_sec, usage.ru_utime.tv_usec/1000, usage.ru_stime.tv_sec, usage.ru_stime.tv_usec/1000);
                    printf("no. of page faults: %ld \n", usage.ru_minflt+usage.ru_majflt);
                    printf("no. of context switches: %ld \n\n", usage.ru_nvcsw+usage.ru_nivcsw);
             
                                                           
                    
                }
            }
            //the last command
            close(0);
            int fd=open(f,O_RDONLY);
     
            execvp(mulcmd[icmd][0],mulcmd[icmd]);
                //if error
            if (mulcmd[icmd][0][0] != '.' && mulcmd[icmd][0][0]!='/'){
                printf("exec: : Not a basic linux command i.e. not in /usr/bin \n\n");
            }
            else{
                printf("exec: : No such file or directory\n");
                printf("Debug hint: incorrect filename, incorrect path etc.\n\n");
            }
            printf("monitor experienced an error in starting the command: %s \n\n", mulcmd[icmd][0]);
            exit(1);           
        }
        
        else {

            int status;
            struct rusage usage;
            signal(SIGINT, SIG_IGN);
            wait4(pid, &status, 0, &usage);   
            if (WIFEXITED(status))  // if WIFEXITED(status)== true, WEXITSTATUS(status) returns the termination status
            {	
                printf("The last command %s terminated with returned status code = %d\n\n", mulcmd[pipnum][0],WEXITSTATUS(status));
            }
            if(WIFSIGNALED(status)){
                printf("The last command %s is interrupted by the signal number = %d (%s)\n\n", mulcmd[pipnum][0], WTERMSIG(status), sigcodeConv(WTERMSIG(status)));
            }  //WTERMSIG can evaluates to the number of the signal that terminated the child process if the value of WIFSIGNALED(status) is nonzero.
            
        }
        return 1;
    }
}


  
void do_cmd(int argnum, char* command[]) {
    pid_t pid;
   
    // recognize pipe
    for (j = 0; j < argnum; j++) {
        if (strcmp(command[j], "!") == 0) {
            commandWithPipe(command);
            return;
        }
    }

    //for command without pipe
    struct timeval start, end;
    int time_used;  
    
    gettimeofday(&start, NULL); // real time at the start of the program
    switch(pid = fork()) {
        case -1:
            printf("Child process creation failed");
            return;

        case 0: // Child process
                
                printf("Process with id: %d created for the command: %s \n", getpid(),command[0]);

                execvp(command[0], command); //after testing, all three situations can be handled well with execvp()
                if (command[0][0] != '.' && command[0][0]!='/'){
                    printf("exec: : Not a basic linux command i.e. not in /usr/bin \n\n");
                }
                else{
                    printf("exec: : No such file or directory\n");
                    printf("Debug hint: incorrect filename, incorrect path etc.\n\n");
                }
                printf("monitor experienced an error in starting the command: %s \n\n", command[0]);
                exit(1);// output error(1) and terminate the process 

        default: { //parent process
                int status;
                struct rusage usage;
                signal(SIGINT, SIG_IGN);
                wait4(pid, &status, 0, &usage);
                
                gettimeofday(&end, NULL);  // real time at the end of the program
                if (WIFEXITED(status))  // if WIFEXITED(status)== true, WEXITSTATUS(status) returns the termination status
                {	
                    printf("The command %s terminated with returned status code = %d\n\n", command[0],WEXITSTATUS(status));
                }
                if(WIFSIGNALED(status)){
                    printf("The command %s is interrupted by the signal number = %d (%s)\n\n", command[0], WTERMSIG(status), sigcodeConv(WTERMSIG(status)));
                }  //WTERMSIG can evaluates to the number of the signal that terminated the child process if the value of WIFSIGNALED(status) is nonzero.
                

                printf("real: %.03f s, ",(double)(end.tv_sec + end.tv_usec / 1000000 - start.tv_sec - start.tv_usec / 1000000));
                printf("user: %ld.%03ld s, system: %ld.%03ld s \n",usage.ru_utime.tv_sec, usage.ru_utime.tv_usec/1000, usage.ru_stime.tv_sec, usage.ru_stime.tv_usec/1000);
                printf("no. of page faults: %ld \n", usage.ru_minflt+usage.ru_majflt);
                printf("no. of context switches: %ld \n\n", usage.ru_nvcsw+usage.ru_nivcsw);
                      
        }
    
    }
	
}

int main(int argc, char *argv[]) {

    if (argc == 1) {exit(0);} 
    argnum = argc-1;
    for (i = 0; i < argnum; i++) {
        command[i]= argv[i+1] ;
        }                    // build command
    do_cmd(argnum, command); // implement command
    return 0;
} // main
