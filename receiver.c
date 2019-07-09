#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gpgme.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mirrorInterface.h"

extern char *commonDir, *inputDir, *mirrorDir, *logFileName, *id, *id2, *buffSize;
extern pid_t rpid,spid;

int main(int argc, char *argv[]){
    printf("Im the receiver of %d. My id is %d\n",getppid(),getpid());
    rpid = getpid();
   
    static struct sigaction act;
    act.sa_handler = transferFail;
    sigemptyset(&(act.sa_mask));
    sigaction(SIGALRM, &act, NULL);

    if(argc != 9){
        printf("Wrong ammount of arguments\n");
        exit(2);
    }

    commonDir = argv[1];
    mirrorDir = argv[2];
    inputDir = argv[3];
    id = argv[4];
    id2 = argv[5];
    buffSize = argv[6];
    logFileName = argv[7];
    spid = atoi(argv[8]);
    int buffSz = atoi(buffSize);

    gpgme_ctx_t ctx;
    gpgme_error_t err;
    gpgme_data_t in, out;
    gpgme_engine_info_t info;
    gpgme_genkey_result_t genkey_res;

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

    /* Generate Key Pair (Public and private) */
    printf("Generating Key Pair...\n");
    genkey_res = generateKeyPair(ctx);
    printf("Keys generated successfully.\n");

    /* Write public key to the commonDir file */
    char *commonFileName = malloc(sizeof(char)*(strlen(commonDir)+strlen(id)+5));
    sprintf(commonFileName,"%s/%s.id",commonDir,id);
    FILE *cffp = fopen(commonFileName,"a");
    if (!cffp){
        printf("Error: Could not open %s\n",commonFileName);
        exit(1);
    }
    fprintf(cffp,"%s\n",genkey_res->fpr);
    fclose(cffp);

    /* Creating the named pipe */
    char *fifoPath = malloc(sizeof(char)*(strlen(commonDir) + 2*ID_LENGTH + 11));
    sprintf(fifoPath,"%s/%s_to_%s.fifo",commonDir,id2,id);

    if (mkfifo(fifoPath,0666) == -1 && errno != EEXIST){
        printf("Error %d in mkfifo of %s\n",errno,fifoPath);
        /* Sending  SIGUSR2 to parent process, in order to try again */
        kill(getppid(),SIGUSR2); 
        exit(2);
    }

    /* Create mirrorDir/id directory */
    char *dirPath = pathcat(mirrorDir, id2);
    mkdir(dirPath, 0777);

    /* Prepare to read data from the pipe */
    int fd;
    char buffer[buffSz], *fileName;
    fd = open(fifoPath,O_RDONLY);
    if (fd == -1){
        printf("Error in opening %s\n",fifoPath);
        /* Sending  SIGUSR2 to parent process, in order to try again */
        kill(getppid(),SIGUSR2); 
        exit(2);
    }

    /* Open the log file for writing */
    FILE *logfp = fopen(logFileName,"a");
    if (logfp==NULL)
        printf("Cannot open file %s to write.\n",logFileName);

    uint16_t fileNameLength=1;
    uint32_t fileSize;

    /* Read the file name length */
    read(fd, &fileNameLength, 2);
    while(fileNameLength){

        /* Read the file name */
        fileName = malloc(sizeof(char)*(fileNameLength+1));
        read(fd, fileName, fileNameLength + 1);
        char *mirrorFilePath = pathcat(dirPath, fileName);

        /* If the last char is /. In other words if it's a directory */
        if(fileName[strlen(fileName)-1] == '/'){
            int r;
            if ((r = mkdir(mirrorFilePath,0777)) < 0){
                printf("Error %d mkdir\n",errno);
            }
        }
        else{

            /* Read the file length */
            read(fd, &fileSize, 4);

            /* Read the file, decrypt it and write it in its mirror */
            printf("Receiving file %s...\n",fileName);
            int outfd = open(mirrorFilePath, O_WRONLY | O_CREAT, 0644);
            if (outfd == -1){
                printf("Error %d Cannot open file %s\n",errno,mirrorFilePath);
                /* Sending  SIGUSR2 to parent process, in order to try again */
                kill(getppid(),SIGUSR2); 
                exit(1);
            }

            gpgme_data_new(&in);

            int i;
            size_t items;
            int n = (int) ceil((double)fileSize / buffSz);

            /* for i < n-1 so that the last buffer is excluded */
            for (i=0; i<n-1; i++){
                /* SIGALRM if nothing has been sent for 30 seconds */
                alarm(30);

                /* Read data */
                items = read(fd, buffer, buffSz);

                /* Turn off alarm */
                alarm(0);

                gpgme_data_write(in, buffer, items);
            }

            /* The last buffer might have less than buffSize bytes */
            if (n){ /* If file is not empty */
                if (fileSize % buffSz == 0)
                    items = read(fd, buffer, buffSz);
                else
                    items = read(fd, buffer, fileSize % buffSz);
                gpgme_data_write(in, buffer, items);
            }

            //printf("Before decryption: \n");
            //print_data_t(in);
            gpgme_data_seek(in, 0, SEEK_SET);

            /* Decrypting received data */
            printf("Decrypting file %s...\n",fileName);
            gpgme_data_new(&out);
            gpgme_op_decrypt(ctx,in,out);

            //printf("After decryption: \n");
            //print_data_t(out);

            write_data_t(outfd,out);

            close(outfd);
            gpgme_data_release(in);
            gpgme_data_release(out);
            printf("Receiving of file %s was complete.\n",fileName);

            /* Write a log file entry about the file */
            fprintf(logfp,"%s r %s %d\n",id,fileName,fileSizeOf(mirrorFilePath));
        }

        free(fileName);
        free(mirrorFilePath);

        /* Read the file name length */
        read(fd, &fileNameLength, 2);
    }
    free(dirPath);

    gpgme_release(ctx);
    fclose(logfp);
    close(fd);
    unlink(fifoPath);
    printf("Finished the receiver of %d. My id is %d\n",getppid(),getpid());
    return 0;
}
