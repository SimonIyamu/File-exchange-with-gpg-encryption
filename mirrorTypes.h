#define ID_LENGTH 11
#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )
#define FPR_LENGTH 41
#define PID_LENGTH 4

typedef enum { false, true } bool;
