#include <gpgme.h>
#include <sys/inotify.h>
#include "mirrorTypes.h"

/* Returns true if path is a directory */
bool isDir(const char *path);

/* Create and write a file in commonDir which contains the process id */
char *writeProcess(const char *commonDir, const char *id);

/* Synchronize this client with the rest clients that exist at this moment */
void synch(const char *commonDir, const char *inputDir, const char *mirrorDir, const char *myid, const char *buffSize);

/* Watch common Dir for any new clients and connect to them */
void watch(const char *commonDir, const char *inputDir, const char *mirrorDir, const char *myid, const char *buffSize);

/* Called by watch when a new client appears */
void handle_event(struct inotify_event *event, const char *commonDirPath, const char *inputDirPath, const char *mirrorDirPath, const char *id, const char *buffSize);

/* Connects two clients by creating sender and receiver processes */
void connectTo(const char *commonDir, const char *inputDir, const char *mirrorDir,  const char *id1, const char *id2, const char *buffSize);

/* Signal handlers*/
void transferFail(int signo); /* SIGUSR2/SIGALRM */
void sighandler(int signo);/* SIGINT/SIGQUIT */

/* Concatinate path a with path b */
char *pathcat(const char *a,const char *b);

/* Compute the size of a file */
int fileSizeOf(const char *path);

/* Recursive function for the deletion of a given directory and its subdirectories */
void deleteDir(const char *dirPath);

/* gpgme functions */
/* Generate key pair*/
gpgme_genkey_result_t generateKeyPair(gpgme_ctx_t ctx);

/* Print gpgme_data_t contents */
void print_data_t(gpgme_data_t dh);

/* Write gpgme_data_t contents to file decriptor */
void write_data_t(int fd, gpgme_data_t dh);

gpgme_error_t passphrase_cb (void *opaque, const char *uid_hint, const char *passphrase_info, int last_was_bad, int fd);
