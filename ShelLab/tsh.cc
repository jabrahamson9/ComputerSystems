// 
// tsh - A tiny shell program with job control
// 
// <Put your name and login ID here>
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;

//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char **argv) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//

void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
    
  char *argv[MAXARGS]; //array of pointers to characters to fill command line argument with the maximum number of arguments

  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
 /* int bg = parseline(cmdline, argv); 
  if (argv[0] == NULL)  
    return;   /* ignore empty lines */
    
    //int to record for bg   
//*******************************************************************
    int bg; //********************* check if in background         
    pid_t pid;      
    sigset_t mask;
    
    bg = parseline(cmdline, argv); //built in fucntion has two arguments. basically it is saying that it will fill the command line with the argv arguments. Return true if the user has requested a BG job, false if the user has requested a FG job. 
    
    //check if valid builtin_cmd
    if(!builtin_cmd(argv)) { //forking and execing a specified program only for builtins
        
        // blocking first
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        // forking
        if (fork() == 0){ //this means we are in the child process****
            if(execv(argv[0], argv) < 0){//give it the path to the command which is first thing we type in or argv[0] and then the argument vector argv is the child process
                //we are execing the process and it SHOULDNT RETURN if we killed off the process successfully so if it doesnt then an ERROR
                
                printf("Command Error\n");
                    exit(0); //error then terminate
            }
            
        }
        else if(pid == 0) { //store the PID
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            setpgid(0, 0);
            //check if command is there
            if(execvp(argv[0], argv) < 0) {
                printf("%s: Command not found\n", argv[0]);
                exit(1);
            }
        } 
        ///ADDING JOBS ******************************
        else {
            if(!bg){ //if no background then add to foreground (only 1)
                addjob(jobs, pid, FG, cmdline); //returns 1 if successful 0 if not
            }
            else { //else add it to the background cuz can have as many as you want
                addjob(jobs, pid, BG, cmdline);
            }
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            
            //if bg/fg
            if (!bg){
                //wait for fg process
                waitfg(pid); 
            } 
            else { //it is in the background!!!!!!!
                //print for bg print a status message
                printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
            }
        }
    }

  return;
}
//*********************************************************************************

/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//
int builtin_cmd(char **argv) 
{
  /*string cmd(argv[0]);
  return 0;     /* not a builtin command */
    
    if (strcmp(argv[0], "quit") == 0) { //This is so you can type quit and quit the shell with string compare
        exit(0);
    }
    else if (!strcmp("&", argv[0])){
        return 1;
    }
    else if (!strcmp("jobs", argv[0])) {  
        listjobs(jobs);  
        return 1;  
    }  
    else if (!strcmp("bg", argv[0]) || !(strcmp("fg", argv[0]))) {  //putting things into the foreground or the background 
        //call bgfg
        do_bgfg(argv);  
        return 1;  
    }  
    return 0; //not a built in command so we would then need to fork and child
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv) 
{
    /*
  struct job_t *jobp=NULL;
    
  /* Ignore command if no argument 
  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }
    
  /* Parse the required PID or %JID arg 
  if (isdigit(argv[1][0])) {
    pid_t pid = atoi(argv[1]);
    if (!(jobp = getjobpid(jobs, pid))) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  else if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    if (!(jobp = getjobjid(jobs, jid))) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  }	    
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  //
  string cmd(argv[0]);

  return;*/
    struct job_t *job;
    char *tmp;
    int jid;
    pid_t pid;

    tmp = argv[1];
    
    // if id does not exist
    if(tmp == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }
    
    // if it is a jid
    if(tmp[0] == '%') {  
        jid = atoi(&tmp[1]); 
        //get job
        job = getjobjid(jobs, jid);
        if(job == NULL){  
            printf("%s: No such job\n", tmp);  
            return;  
        }else{
            //get the pid if a valid job for later to kill
            pid = job->pid;
        }
    } 
    // if it is a pid
    else if(isdigit(tmp[0])) { 
        //get pid
        pid = atoi(tmp); 
        //get job 
        job = getjobpid(jobs, pid); 
        if(job == NULL){  
            printf("(%d): No such process\n", pid);  
            return;  
        }  
    }  
    else {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    //kill for each time
    kill(-pid, SIGCONT);
    
    if(!strcmp("fg", argv[0])) {
        //wait for fg
        job->state = FG;
        waitfg(job->pid);
    } 
    else{
        //print for bg
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        job->state = BG;
    } 
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid) // we give it a pid and it will wait until that pid is no longer associated with a forground structure
{
    struct job_t* job;
    job = getjobpid(jobs,pid); //grabbing the sructure to check and see if the jobs pid is equal to the original pid. or if the state is equal to FG
    //check if pid is valid
    if(pid == 0){
        return;
    }
    if(job != NULL){
        //sleep
    //***************************************
        while(pid==fgpid(jobs)){ //keep running until the pids are no longer equal or the pid of the child is not longer in foreground
        }
    //************************************
    }
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  
//
void sigchld_handler(int sig) 
{
    int status;  // fg or bg or undefined
    pid_t pid;  //childs process id
    
    while ((pid = waitpid(fgpid(jobs), &status, WNOHANG|WUNTRACED)) > 0) {  //reap the child that terminated (deleting), WNHOHANG it will leave if the process is already stopped
        if (WIFSTOPPED(status)){ 
            //change state if stopped
            getjobpid(jobs, pid)->state = ST; //now we have gottem the job
            int jid = pid2jid(pid);
            printf("Job [%d] (%d) Stopped by signal %d\n", jid, pid, WSTOPSIG(status));
        }  
        else if (WIFSIGNALED(status)){
            //delete is signaled
            int jid = pid2jid(pid);  
            printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, WTERMSIG(status));
            deletejob(jobs, pid); //once the job terminates we delete the job
        }  
        else if (WIFEXITED(status)){  
            //exited
            deletejob(jobs, pid);  
        }  
    }  
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
//
void sigint_handler(int sig) 
{
      pid_t pid = fgpid(jobs);  
    
    //check for valid pid
    if (pid != 0) {     
        kill(-pid, sig);
    }   
    return; 
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//
void sigtstp_handler(int sig) 
{
    pid_t pid = fgpid(jobs);  
    //check for valid pid
    if (pid != 0) { 
        kill(-pid, sig);  
    }  
    return; 
}

/*********************
 * End signal handlers
 *********************/




