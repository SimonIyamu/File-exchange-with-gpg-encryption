#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mirrorInterface.h"

extern char *filePath;
extern bool clear;
extern char *commonDir, *inputDir, *mirrorDir, *logFileName, *id, *buffSize;

int main(int argc, char *argv[]){

    /* Handling command line arguments */
    if(argc==13){
        int i;
        for(i=1 ; i<argc ; i++){
            if(!strcmp(argv[i],"-n"))
                id = argv[i+1];
            if(!strcmp(argv[i],"-c")){
                commonDir = argv[i+1];
            }
            if(!strcmp(argv[i],"-i"))
                inputDir = argv[i+1];
            if(!strcmp(argv[i],"-m"))
                mirrorDir = argv[i+1];
            if(!strcmp(argv[i],"-b"))
                buffSize = argv[i+1];
            if(!strcmp(argv[i],"-l"))
                logFileName = argv[i+1];
        }
    }
    else{
        printf("Wrong ammount of arguments\n");
        return(1);
    }

    /* Check if inputDir exists */
    if(!isDir(inputDir)){
        printf("Error: Input Directory %s does not exist.\n",inputDir);
        exit(22);
    }

    /* Check if inputDir exists (it shouldn't) */
    if(isDir(mirrorDir)){
        printf("Error: Mirror Directory %s exists already.\n",mirrorDir);
        exit(22);
    }

    /* Clear the previous log file if it exists */
    unlink(logFileName);
    
    printf("New client with id %d\n",getpid());

    /* Create commonDir (if it does not exist) */
    printf("%s was created.\n",commonDir);
    mkdir(commonDir,0777);

    /* Create mirrorDir */
    printf("%s was created.\n",mirrorDir);
    mkdir(mirrorDir,0777);

    filePath = writeProcess(commonDir,id);
   
    /* Signal for termination */ 
    static struct sigaction act1;
    act1.sa_handler = sighandler;
    sigemptyset(&(act1.sa_mask));
    sigaction(SIGINT, &act1, NULL);
    sigaction(SIGQUIT, &act1, NULL);

    static struct sigaction act2;
    act2.sa_handler = transferFail;
    sigemptyset(&(act2.sa_mask));
    sigaction(SIGUSR2, &act2, NULL);
    sigaction(SIGALRM, &act2, NULL);

    //signal(SIGUSR2, transferFail);
    //signal(SIGALRM, sighandler);

    synch(commonDir,inputDir,mirrorDir,id,buffSize); 
    watch(commonDir,inputDir,mirrorDir,id,buffSize); 

    return 0;
}
