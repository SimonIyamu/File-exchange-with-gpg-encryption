#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gpgme.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mirrorInterface.h"

void sendDir(int, FILE*, const char*, const char*, const char*, int, gpgme_ctx_t ctx, gpgme_key_t *key);

int main(int argc, char *argv[]){
    printf("Im the sender of %d. My id is %d\n",getppid(),getpid());

    if(argc != 7){
        printf("Wrong ammount of arguments. Given %d. Expected 6.\n",argc);
        exit(2);
    }

    char *commonDir = argv[1], *inputDirPath = argv[2], *id1 = argv[3], *id2 = argv[4], *logFileName = argv[6];
    int  buffSize = atoi(argv[5]); 

    gpgme_ctx_t ctx;
    gpgme_error_t err;
    gpgme_engine_info_t info;
    gpgme_genkey_result_t genkey_res;
    gpgme_key_t key[2] = {NULL, NULL};

    /* Set signal handler for termination */
    static struct sigaction act;
    act.sa_handler = sighandler;
    sigemptyset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    /* Initialize gpgme */
    gpgme_check_version(NULL);
    err = gpgme_new (&ctx);
    if (err){
        printf("Error in gpgme_new\n%s: %s",gpgme_strsource(err),gpgme_strerror(err));
        exit(1);
    }
    err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
    if (err){
        printf("Error in gpgme_engine_check_version\n%s: %s",gpgme_strsource(err),gpgme_strerror(err));
        exit(1);
    }
    err = gpgme_get_engine_info(&info);
    if (err){
        printf("Error in gpgme_get_engine_info\n%s: %s",gpgme_strsource(err),gpgme_strerror(err));
        exit(1);
    }
    err = gpgme_ctx_set_engine_info(ctx,GPGME_PROTOCOL_OpenPGP,NULL,NULL);
    if (err){
        printf("Error in gpgme_ctx_set_engine_info\n");
        exit(1);
    }

    /* Set passphrase function */
    gpgme_set_pinentry_mode(ctx, GPGME_PINENTRY_MODE_LOOPBACK);
    gpgme_set_passphrase_cb (ctx, passphrase_cb, NULL);


    /* Creating the named pipe */
    char *fifoPath = malloc(sizeof(char)*(strlen(commonDir) + 2*ID_LENGTH + 11));
    sprintf(fifoPath,"%s/%s_to_%s.fifo",commonDir,id1,id2);
    if(mkfifo(fifoPath,0666) == -1 && errno != EEXIST){
        printf("Error %d in mkfifo of %s\n",errno,fifoPath);
        /* Sending SIGUSR2 to parent process, in order to try again */
        printf("Sending SIGUSR2 to parent process(%d), in order to try again\n",getppid());
        kill(getppid(),SIGUSR2);
        exit(2);
    }

    /* Send info through the pipe*/
    int fd;

    /* Open pipe */
    if ((fd = open(fifoPath, O_WRONLY)) < 0){  
        printf("Error in opening pipe %s\n",fifoPath);
        /* Sending SIGUSR2 to parent process, in order to try again */
        kill(getppid(),SIGUSR2);
        exit(2);
    }

    /* Open log file for writing */
    FILE *logfp = fopen(logFileName,"a");
    if (logfp==NULL)
        printf("Cannot open file %s to write.\n",logFileName);

    /* Get the public key of the receiver */
    char *commonFileName = malloc(sizeof(char)*(strlen(commonDir)+strlen(id2)+5));
    sprintf(commonFileName,"%s/%s.id",commonDir,id2);
    FILE *cffp = fopen(commonFileName,"r");
    if (!cffp){
        printf("Error: Could not open %s\n",commonFileName);
        exit(1);
    }
    char fpr[FPR_LENGTH];
    fscanf(cffp,"%s",fpr); /*ignore pid*/
    fscanf(cffp,"%s",fpr);
    fclose(cffp);
    err = gpgme_get_key(ctx,fpr,&(key[0]),0);
    if(err){
        printf("Error in gpgme_get_key\n");
        exit(1);
    }

    sendDir(fd, logfp, id1, inputDirPath,"",buffSize, ctx, key);

    /* Send 0 to terminate the transfer */
    uint16_t zero = 0;
    write(fd, &zero, 2);

    gpgme_release(ctx);
    close(fd);

    return 0;
}

void sendDir(int fd, FILE *logfp, const char *id1, const char *inputDirPath, const char *rPath, int buffSize, gpgme_ctx_t ctx, gpgme_key_t *key){
    /* rPath is a the path of the directory, relative to the input
     * directory. e.x. for a directory with path user1/inputDir/aa/bb/ ,
     * the inputDirPath will be user1/inputDir/ and rPath will be aa/bb/ */

    gpgme_error_t err;
    gpgme_data_t in, out;

    char *dirPath = pathcat(inputDirPath,rPath);

    struct dirent *currentFile;
    DIR *dir = opendir(dirPath);
    uint16_t fileNameLength;
    /* For every file... */
    while((currentFile = readdir(dir)) != NULL){

        /* Ignore . and .. directories */
        if (!strcmp(currentFile->d_name,".") || !strcmp(currentFile->d_name,".."))
                continue;

        /* fileRPath is relative to the inputDir. e.x. for a file with the 
         * path user1/inputDir/newDir/foo.txt its fileName will be 
         * newDir/foo.txt */
        char *fileRPath = malloc(sizeof(char)*(strlen(rPath) + strlen(currentFile->d_name)+3));
        strcpy(fileRPath,rPath);
        strcat(fileRPath,currentFile->d_name);

        char *filePath = pathcat(dirPath,currentFile->d_name);

        if (isDir(filePath)){
            /* Put / in the end of the name to indicate its a directory */
            int x = strlen(fileRPath);
            fileRPath[x] = '/';
            fileRPath[x+1] = '\0';
        }

        /* Write the file name length */
        fileNameLength = strlen(fileRPath);
        write(fd, &fileNameLength, 2);

        /* Write the file name */
        write(fd, fileRPath, fileNameLength + 1);

        if (isDir(filePath)){
            sendDir(fd, logfp, id1, inputDirPath, fileRPath, buffSize, ctx, key);
            continue;
        }

        /* -------------*/
        /* Encrypt file */
        int infd = open(filePath,O_RDONLY);
        if (infd == -1){
            printf("Error: Cannot open file %s\n",filePath);
            return;
        }

        /* data_t in gets linked with the fd */
        err = gpgme_data_new_from_fd(&in, infd);
        if (err){
            printf("Error in gpgme_data_new_from_fd\n");
            exit(1);
        }

        err = gpgme_data_new(&out);
        if (err){
            printf("Error in gpgme_data_new\n");
            exit(1);
        }

        printf("Encrypting file %s...\n",fileRPath);
        
        /* Encrypt data and save it to gpgme_data_t out */
        err = gpgme_op_encrypt(ctx, key, GPGME_ENCRYPT_ALWAYS_TRUST, in, out);
        if (err){
            printf("Error in gpgme_op_encrypt");
            exit(1);
        }

        /* Find the new data size */
        off_t end = gpgme_data_seek(out, 0, SEEK_END);
        off_t start = gpgme_data_seek(out, 0 , SEEK_SET);
        uint32_t fileSize = end-start;

        /* Write the file length to the pipe */
        write(fd, &fileSize, 4);

        /* Write the file */
        printf("Sending file %s...\n",fileRPath);
        int i;
        int n = (int) ceil((double)fileSize / buffSize);/* number of loops */
        char buffer[buffSize];
        size_t items;
        for (i=0; i<n; i++){
            items = gpgme_data_read(out, buffer, buffSize);
            if (items == -1){
                printf("Error in gpgm_data_read\n");
                exit(1);
            }
            write(fd, buffer, items);
        }
        close(infd);

        printf("Sending of file %s was complete.\n",fileRPath);

        /* Write a log file entry about the file */
        fprintf(logfp,"%s s %s %d\n",id1,fileRPath,fileSizeOf(filePath));

        gpgme_data_release(in);
        gpgme_data_release(out);
        free(fileRPath);
    }
    closedir(dir);
    free(dirPath);
}
