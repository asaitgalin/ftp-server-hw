#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "file_worker.h"

const char *rootDir = "./root";

fileWorker_t *fwInit() {
    fileWorker_t *worker = (fileWorker_t *)calloc(1, sizeof(fileWorker_t));
    if (!worker) {
        return NULL;
    }
    worker->pathLen = 1;
    worker->path = (char *)calloc(2, sizeof(char));
    strcpy(worker->path, "/");
    return worker;
}

char *fwGetCurrentDir(fileWorker_t *worker) {
    int size = worker->pathLen + 3;
    char *s = (char *)calloc(size, sizeof(char));
    if (!s) {
        return NULL;
    }
    snprintf(s, size, "\"%s\"", worker->path);
    return s;
}

