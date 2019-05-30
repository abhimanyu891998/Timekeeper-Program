/*
filename: timekeeper_3235392177.c
Student's name: Abhimanyue Singh Tanwar
Student number: 3035392177
Development Platform: Ubuntu 
Compilation: 1.) gcc filename.c -o filename 2.) ./filename arguments
Remark: Possibly everything
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>

//Global vars
struct timespec start,end;  
double spc,runningInUserMode,runningInKernelMode,systemTime;
int voluntaryContext,involuntaryContext,totalContextSwitches;

//An array containing the names of signals for mapping purposes
char* signame[]={"INVALID","SIGHUP","SIGINT","SIGQUIT","SIGILL","SIGTRAP","SIGABRT","SIGBUS","SIGFPE","SIGKILL","SIGUSR1","SIGSEGV","SIGUSR2","SIGPIPE",
					"SIGALRM","SIGTERM","SIGTKFLT","SIGCHLD","SIGCONT","SIGSTOP","SIGSTP","SIGTTIN","SIGTTOU","SIGURG","SIGXCPU","SIGXFSG","SIGVTALRN",
					"SIGPROF","SIGWINCH","SIGPOL","SIGPWR","SIGSYS",NULL};


bool childrenHaveTerminated=false;

//Custom SIGINT HANDLER
void sigint_handler(int signum) {
	if(childrenHaveTerminated){
		exit(0);
	}
}


//Function to print the statistics of each child process
void printStatistics(int pid){

	char pathStat[40],pathStatus[40];
	sprintf(pathStat,"/proc/%d/stat",(int)pid); //storing the path of stat file
	sprintf(pathStatus,"/proc/%d/status",(int)pid); //storing the path of status file
	FILE* statusf; 
	FILE* statf;
	statf=fopen(pathStat,"r"); 
	statusf=fopen(pathStatus,"r");
	spc=sysconf(_SC_CLK_TCK); //To calculate clock ticks per second in the system

	if(!statusf){
		printf("Some error opening proc file");
	}

	else{
		char x[1024];
		int counter=0;


		while(fscanf(statf," %1023s",x)==1){
			counter++;
			if(counter==14){
				sscanf(x,"%lf",&runningInUserMode); //To capture the time in user mode
			}else if(counter==15){
				sscanf(x,"%lf",&runningInKernelMode); //To capture the time in kernel mode
			}

		}	
		char temp[1024];
		while(fgets(temp,1024,statusf)!=NULL){
			fscanf(statusf,"%s %d %s %d",temp,&voluntaryContext,temp,&involuntaryContext); //To capture the voluntary and involuntary context
		}



		
		runningInKernelMode=runningInKernelMode/spc;
		runningInUserMode=runningInUserMode/spc;
		
	}

    fclose(statf);
	fclose(statusf);
	totalContextSwitches=involuntaryContext+voluntaryContext; 
	systemTime=( end.tv_sec - start.tv_sec);
	printf("real: %.2f, user: %.2f, system: %.2f, context switch: %d\n",systemTime,runningInUserMode,runningInKernelMode,totalContextSwitches); //Printing the final statistics



}

//Function to execute commands passed by forking
void executeCommand(char* command[], int pipes[][2], int command_index, int pipes_count, char **envp)
{
	pid_t pid;
    siginfo_t status;

	clock_gettime(CLOCK_MONOTONIC,&start);
	pid = fork(); //To create a child process on which command will be executed

	if(pid<0)
	{
		printf("Error while forking");
		exit(-1);
	}

	else if(pid == 0)
	{   

		signal(SIGINT, SIG_DFL); //Child process will have default reaction to SIGINT
		//Handling pipes
		if(command_index==0) //Starting Command
		{
			dup2(pipes[command_index][1], 1);
			//Closing all other pipes
			for(int i=0 ; i<pipes_count ; i++)
			{   
				for(int j=0 ; j<2; j++)
				{    
					if(!(i==command_index && j==1))
					{ 
						close(pipes[i][j]);

					}
				}
			}

		}

		else if(command_index==pipes_count) //Last command
		{
			dup2(pipes[command_index-1][0],0);
			//Closing all other pipes
			for(int i=0 ; i<pipes_count ; i++)
			{   
				for(int j=0 ; j<2; j++)
				{    
					if(!(i==command_index && j==0))
					{ 
						close(pipes[i][j]);

					}
				}
			}
		}

		else
		{
			dup2(pipes[command_index-1][0],0);
			dup2(pipes[command_index][1],1);

             // Close all pipes except dup-ed one
            for (int i = 0; i < pipes_count; i++) {
                for (int j = 0; j < 2; j++) {
                    if ((i == command_index - 1 && j == 0) || (i == command_index && j == 1)) {
                        // Do nothing
                    } else {
                        close(pipes[i][j]);
                    }
                }
            }


		}

		if(execv(command[0], command)==-1) //To check for absolute path
		{   
			if(execvp(command[0], command)==-1) //To check for relative path
			{   
				if(execve(command[0],command,envp)==-1) //For envp
				{   
					exit(-1);
				}
			}
		}
	}

	else
	{   
        //need to close all pipes used by the child
		printf("Process with id: %d created for the command: %s \n",(int)pid,command[0]);
     	if (pipes_count == 0) { // No pipes for 1 command
            // Do nothing
        } else if (command_index == 0) { // First command
            close(pipes[command_index][1]);

        } else if (command_index == pipes_count) {// Last command
            close(pipes[command_index - 1][0]);

        } else { // Normal commands
            close(pipes[command_index - 1][0]);
            close(pipes[command_index][1]);
        }

		waitid(P_PID, pid, &status, WEXITED | WNOWAIT);
		clock_gettime(CLOCK_MONOTONIC, &end);
	
		
		if(status.si_status == 0) {
			printf("The command \"%s\" terminated with returned status code = %d\n", command[0], status.si_status);
		}
		else {
            //If invalid command
			if(status.si_status==255){
				printf("timekeeper experienced an error in starting the command: %s\n",command[0]);
			}
            //If interruption
            else{
				printf("The command is interrupted by the signal number = %d (%s)\n", status.si_status,signame[status.si_status]);
		
			}
		}
			

		//now printing statisitcs
		printStatistics((int)pid);


		
	}

	return;


}


//Main Function
int main(int argc, char *argv[], char **envp)
{    
	// Handling SIGINT signal
    signal(SIGINT, sigint_handler);

	//If no commands are specified
    if(argc <= 1)
    {
    	return 0;
    }

    int command_count=0;
    char command_end = 0;
    char *command_list[100][100];
     
     //Parsing the commands separated by pipes
     for (int i = 1; i < argc; i++) {
        if (*argv[i] == '!' && *(argv[i] + 1) == '\0') {

            command_list[command_count++][command_end] = NULL;
            
            command_end = 0;
        } else {
            command_list[command_count][command_end++] = argv[i];
            
        }
    }

    //For last command
    command_list[command_count++][command_end]=NULL;
    command_end =0;
    


    //Creating pipes
    int pipes[command_count-1][2];
    for(int i=0 ; i< command_count - 1 ; i++)
    {
    	pipe(pipes[i]); //All pipes created;
    }

    //Running commands one by one
    for(int i=0 ; i<command_count ; i++)
    {
    	
    	executeCommand(command_list[i], pipes, i, command_count-1, envp);

    }

    childrenHaveTerminated=true;

    





    return 0;




}



