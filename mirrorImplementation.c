#include <dirent.h>
#include <errno.h>
#include <gpgme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>
#include "mirrorInterface.h"

char *commonDir, *inputDir, *mirrorDir, *logFileName, *id, *id2, *buffSize, *filePath;
bool clear=false;
static int attempts;
pid_t spid, rpid;

/* Checks if the given path is a directory */
bool isDir(const char *path) {
   struct stat statbuf;

   if (stat(path, &statbuf) != 0){
       /* File does not even exist */
       return false;
   }

   return S_ISDIR(statbuf.st_mode);
}

/* Checks if the path's file exists */
bool exists(const char *path){
    return access(path,F_OK)!=-1;
}

/* Create and write a file in commonDir which contains the process id */
char *writeProcess(const char *commonDir, const char *id){
    /* Create path */
    char *newFilePath = malloc(sizeof(char)*(strlen(commonDir) + strlen(id) + 5));
    sprintf(newFilePath,"%s/%s.id",commonDir,id);

    if (exists(newFilePath)){
        printf("Error: id belongs to some other existing client\n");
        exit(23);
    }

    /* Create and open the file */
    FILE *fp = fopen(newFilePath,"w");
    if (fp == NULL){
        printf("Unable to create file %s\n",newFilePath);
        exit(3);
    }

    /* Write the process's id into the file */
    fprintf(fp,"%d\n",getpid());

    fclose(fp);
    return newFilePath;
}

/* Synchronize this client with the rest clients that exist at this moment */
void synch(const char *commonDirPath, const char *inputDirPath, const char *mirrorDirPath, const char *id, const char *buffSize){
    struct dirent *currentFile;
    DIR *commonDir = opendir(commonDirPath);

    /* For every file */
    while((currentFile = readdir(commonDir)) != NULL){

        /* Ignore . and .. directories */
        if (!strcmp(currentFile->d_name,".") || !strcmp(currentFile->d_name,".."))
                continue;

        /* The client id is in the name of the file, before .id */
        id2 = strtok(currentFile->d_name,".");

        /* Ignore files that dont have the .id extension */
        char *extension = strtok(NULL,"\0");
        if(strcmp(extension,"id"))
            continue;

        /* Skip this client's file */
        if(!strcmp(id2,id))
            continue;

        attempts = 0;
        connectTo(commonDirPath,inputDirPath,mirrorDirPath,id,id2,buffSize);
        printf("File transfers between %s and %s are complete.\n",id,id2);
    }
    closedir(commonDir);
    printf("Synchronization was complete.\n");
}

void watch(const char *commonDirPath, const char *inputDirPath, const char *mirrorDirPath, const char *id, const char *buffSize){
    int fd,wd,length,read_ptr;
    char buffer[EVENT_BUF_LEN];

    /* Creating the inotify instance */
    fd = inotify_init();
    if (fd < 0){
        printf("Error: inotify_init failed\n");
        exit(6);
    }

    wd = inotify_add_watch(fd, commonDirPath, IN_CREATE | IN_DELETE);
    if (wd == -1){
        printf("Error: Failed to add watch on %s\n",commonDirPath);
        return;
    }
    else{
        printf("Watching %s for new clients...\n",commonDirPath);
    }

    int read_offset = 0; /* Remaining number of bytes from previous read */
    while (1) {
        /* Read next series of events */
        length = read(fd, buffer + read_offset, sizeof(buffer) - read_offset);
        if (length < 0){
            printf("Error: inotify read failed\n");
            exit(6);
        }
        length += read_offset;
        read_ptr = 0;
        
        /* Process each event making sure that at least the fixed part of the event in included in the buffer */
        while (read_ptr + EVENT_SIZE <= length) { 
            /* Points to the fixed part of next inotify_event */
            struct inotify_event *event = (struct inotify_event *) &buffer[ read_ptr ];
            
            /* If the dynamic part exceeds the buffer */
            if( read_ptr + EVENT_SIZE + event->len > length ) 
                break;

            /* Undertake the necessary actions on the mirror directory */   
            handle_event(event,commonDir,inputDir,mirrorDir,id,buffSize); 

            /* Advance read_ptr to the beginning of the next event*/
            read_ptr += EVENT_SIZE + event->len;
        }
        /* Check to see if a partial event remains at the end */
        if( read_ptr < length ) {
            /* Copy the remaining bytes from the end of the buffer to the beginning of it */
            memcpy(buffer, buffer + read_ptr, length - read_ptr);
            /* The next read will begin immediatelly after them	*/
            read_offset = length - read_ptr;
        } else
            read_offset = 0;
    }
}

void handle_event(struct inotify_event *event, const char *commonDirPath, const char *inputDirPath, const char *mirrorDirPath, const char *id, const char *buffSize){
    if ((event->mask & IN_CREATE)){
        /* The client id is in the name of the file, before .id */
        id2 = strtok(event->name,".");

        /* Ignore files that dont have the .id extension */
        char *extension = strtok(NULL,"\0");
        if(strcmp(extension,"id"))
            return;

        /* Skip this client's file */
        if(!strcmp(id2,id))
            return;

        printf("Client %s has appeared.\n",event->name);

        attempts = 0;
        connectTo(commonDirPath,inputDirPath,mirrorDirPath,id,id2,buffSize);
        printf("File transfers between %s and %s are complete.\n",id,id2);
    }
    else if ((event->mask & IN_DELETE)){

        /* The client id is in the name of the file, before .id */
        id2 = strtok(event->name,".");

        /* Ignore files that dont have the .id extension */
        char *extension = strtok(NULL,"\0");
        if(strcmp(extension,"id"))
            return;

        /* Skip this client's file */
        if(!strcmp(id2,id))
            return;

        printf("Client %s has left.\n",event->name);

        char *dirPath = pathcat(mirrorDir,id2);
        deleteDir(dirPath);
        printf("%s was removed.\n",dirPath);
        free(dirPath);
    }
}

/* Connects two clients by creating sender and receiver processes */
void connectTo(const char *commonDir, const char *inputDir, const char *mirrorDir,  const char *id1, const char *id2, const char *buffSize){

    /* Fork */
    switch(spid = fork()){
        case -1:
            printf("Error: Failed to fork\n");
            exit(3);
        case 0:
            /* Child process */
            /* Sender */

            execl("./sender","sender",commonDir,inputDir,id1,id2,buffSize,logFileName,NULL);

            printf("Error in execl\n");
            exit(1);
        default:
            /* Parent process */
            break;
    }

    switch(rpid = fork()){
        case -1:
            printf("Error: Failed to fork\n");
            exit(3);
        case 0:;
            /* Child process */
            /* Receiver */
            char sspid[PID_LENGTH+1];
            sprintf(sspid,"%d",spid);
            execl("./receiver","receiver",commonDir,mirrorDir,inputDir,id1,id2,buffSize,logFileName,sspid,NULL);

            printf("Error in execl\n");
            exit(1);
        default:
            /* Parent process */
            break;
    }

    waitpid(spid, NULL, 0);
    waitpid(rpid, NULL, 0);
}

/* Signal handler for SIGUSR2 and SIGALRM */
void transferFail(int signo){
    if (signo == SIGALRM){
        printf("Receiver has been waiting for 30 seconds. Transfer was aborted.\n");
        kill(spid, SIGINT);
        kill(rpid, SIGINT);
        exit(0);
    }

    printf("Attempt %d: Transfer failed\n",attempts+1);

    /* Kill child processes */
    printf("KILL %d, %d\n",spid,rpid);
    kill(spid, SIGINT);
    kill(rpid, SIGINT);

    attempts++;
    if(attempts >= 3){
        printf("Transfer was aborted. Client terminating.\n");
        sighandler(SIGINT);
    }

    /* Unblock signals so that they can be handled again */
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGUSR2);
    sigaddset(&signal_set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

    /* and retry */
    connectTo(commonDir,inputDir,mirrorDir,id,id2,buffSize);
}

void sighandler(int signum){
    if(!clear){
        /* Remove mirrorDir */
        deleteDir(mirrorDir);
        printf("%s was removed.\n",mirrorDir);

        /* Remove the process's file */
        remove(filePath);
        printf("%s was removed.\n",filePath);
        free(filePath);

        /* Write that the client is leaving on the log file */
        FILE *logfp = fopen(logFileName,"a");
        if (logfp==NULL)
            printf("Cannot open file %s to write.\n",logFileName);
        fprintf(logfp,"%s x\n",id); /* x indicates that the client left */
        fclose(logfp);
    }

    clear = true;
    exit(0);
}

/* Concatinate two paths */
char *pathcat(const char *a,const char *b){
    char *path = (char*) malloc(sizeof(char)*(strlen(a) + strlen(b) + 2));
    strcpy(path,a);
    strcat(path,"/");
    strcat(path,b);
    return path;
}

/* Find file's size */
int fileSizeOf(const char *path){
    FILE *dfp = fopen(path,"r");
    if (!dfp){
        printf("Cannot open file %s\n",path);
        exit(1);
    }
    fseek(dfp,0, SEEK_END);
    int size = ftell(dfp);
    rewind(dfp);
    fclose(dfp);
    return size;
}

/* Recursive function for the deletion of a given directory and its subdirectories */
void deleteDir(const char *dirPath){
    char *filePath;
    struct dirent *currentFile;
    DIR *dir = opendir(dirPath);

    /* For every file */
    while((currentFile = readdir(dir)) != NULL){

        /* Ignore . and .. directories */
        if (!strcmp(currentFile->d_name,".") || !strcmp(currentFile->d_name,".."))
                continue;

        filePath = pathcat(dirPath,currentFile->d_name);

        /* If its a subdirectory */
        if (isDir(filePath))
            /* Rec call */
            deleteDir(filePath);
        else
            /* Delete file */
            unlink(filePath);
            
        free(filePath);
    }
    closedir(dir);

    rmdir(dirPath);
}

/*-------------------------*/
/* GPGME related functions */

gpgme_genkey_result_t generateKeyPair(gpgme_ctx_t ctx){
    char *params = "<GnupgKeyParms format=\"internal\">\n"
    "Key-Type: RSA\n"
    "Key-Length: 2048\n"
    "Subkey-Type: RSA\n"
    "Subkey-Length: 2048\n"
    "Name-Real: Idk\n"
    "Name-Comment: I wish i remembered my name\n"
    "Name-Email: die4di@uoa.gr\n"
    "Expire-Date: 0\n"
    "Passphrase: abc\n"
    "</GnupgKeyParms>\n"; 

    gpgme_error_t err;

    /* Generate the keys */
    err = gpgme_op_genkey(ctx,params,NULL, NULL);
    if (err){
        printf("Error in gpgme_op_genkey\n%s: %s",gpgme_strsource(err),gpgme_strerror(err));
        exit(1);
    }

    /* Retrieve the key pair and return it */
    return gpgme_op_genkey_result(ctx);
}

gpgme_error_t passphrase_cb (void *opaque, const char *uid_hint, const char *passphrase_info, int last_was_bad, int fd) {
    int n;
    char *pass = "abc\n";
    int passlen = strlen (pass);
    int off = 0;

    do{
        n = write (fd, &pass[off], passlen - off);
        if (n > 0)
            off += n;
    }while (n > 0 && off != passlen);

    if (off == passlen)
       return 0;
    return gpgme_error_from_errno (errno);
}

void print_data_t(gpgme_data_t dh){
    char buf[512];
    gpgme_data_seek(dh, 0, SEEK_SET);
    int n;
    while((n = gpgme_data_read(dh, buf, 512)) > 0)
        printf("%s",buf);
    if (n<0){
        printf("Error gpgme_data_read\n");
        exit(1);    
    }
    printf("\n");
    gpgme_data_seek(dh, 0, SEEK_SET);
}

void write_data_t(int fd, gpgme_data_t dh){
    char buf[512];
    gpgme_data_seek(dh, 0, SEEK_SET);
    int n;
    while((n = gpgme_data_read(dh, buf, 512)) > 0)
        write(fd,buf,n);
    if (n<0){
        printf("Error gpgme_data_read\n");
        exit(1);    
    }
    gpgme_data_seek(dh, 0, SEEK_SET);
}
