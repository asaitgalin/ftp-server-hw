#ifndef _FTP_FILE_WORKER_H_
#define _FTP_FILE_WORKER_H_

struct fileWorker_tag {
    char *path;
    int pathLen;
};

typedef struct fileWorker_tag fileWorker_t;

fileWorker_t *fwInit();
char *fwGetCurrentDir(fileWorker_t *worker);

#endif

