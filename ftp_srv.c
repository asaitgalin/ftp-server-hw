#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "responses.h"
#include "file_worker.h"
#include "constants.h"

// TODO: Do not forget to close and shutdown all socket descriptors!

const int ftpCmdPort = 21;
const int ftpDataPort = 20;

const int maxCmdSize = 128;

enum ftpMode {
    fmUnset = 0,
    fmPassive,
    fmActive
};

struct threadData_tag {
    in_addr_t srvAddr;
    int cmdSock;
};

typedef struct threadData_tag threadData_t;

struct clientData_tag {
    enum ftpMode mode;
    int cmdSock;
    // for passive mode
    int dataSock;
    // for active mode
    in_addr_t dataAddr;
    int dataPort;
};

typedef struct clientData_tag clientData_t;

void ThreadError(const char *msg) {
    fprintf(stderr, "%s:\n%s", msg, strerror(errno));
    pthread_exit(NULL);
}

void SendResponse(int sock, int code, const char *content) {
    response_t *resp = BuildResponse(code, content);
    if (!resp) {
        pthread_exit(NULL);
    }
    if (send(sock, resp->data, resp->len, 0) == -1) {
        FreeResponse(resp);
        pthread_exit(NULL);
    }
    FreeResponse(resp);
}

void SendPasvResponse(int cmdSock, struct sockaddr_in *addr) {
    char buf[maxCmdSize]; 
    char ip[INET_ADDRSTRLEN];
    int p1, p2;
    int i = 0;
    int port = ntohs(addr->sin_port);
    p1 = port >> 8;
    p2 = port & 255;
    memset(ip, 0, sizeof(ip));
    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
    while (ip[i] != '\0') {
        if (ip[i] == '.') {
            ip[i] = ',';
        }
        ++i;
    }
    snprintf(buf, maxCmdSize, "Entering Passive Mode (%s,%d,%d)", ip, p1, p2);
    SendResponse(cmdSock, codePasvMode, buf); 
}


int GetPassiveDataChannel(in_addr_t srvAddr, struct sockaddr_in *outAddr) {
    struct sockaddr_in addr;
    socklen_t addrLen;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        ThreadError("Failed to create data channel socket");
    }
    addr.sin_family = AF_INET;
    addr.sin_port = 0; // pick random free port
    addr.sin_addr.s_addr = srvAddr;
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        ThreadError("Failed to bind data channel");
    }
    addrLen = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addrLen) == -1) {
        ThreadError("getsockname() failed");
    }
    if (listen(fd, 2) == -1) {
        ThreadError("Failed to set socket to listen mode");
    }
    memcpy(outAddr, &addr, sizeof(addr));
    return fd; 
}

void ParsePortCommand(const char *data, in_addr_t *outAddr, int *outPort) {
    int a1, a2, a3, a4;
    int p1, p2;
    char ipAddr[INET_ADDRSTRLEN];
    memset(ipAddr, 0, sizeof(ipAddr));
    sscanf(data, "%d,%d,%d,%d,%d,%d", &a1, &a2, &a3, &a4, &p1, &p2);
    snprintf(ipAddr, INET_ADDRSTRLEN, "%d.%d.%d.%d", a1, a2, a3, a4);
    *outAddr = inet_addr(ipAddr);
    *outPort = p1 * 256 + p2;
}

void *ClientProc(void *param) {
    char cmdBuf[maxCmdSize];
    threadData_t data = *(threadData_t *)param;
    
    // Socket data for work with client
    clientData_t client;
    client.mode = fmUnset;
    client.cmdSock = data.cmdSock;
    client.dataSock = -1;

    // File worker
    fileWorker_t *worker = fwInit();
    if (!worker) {
        pthread_exit(NULL);
    }

    SendResponse(client.cmdSock, codeReady, "FTP server ready.");
    memset(cmdBuf, 0, sizeof(cmdBuf));
    while (1) {
        char *cmd = NULL;
        if (recv(client.cmdSock, cmdBuf, maxCmdSize, 0) == -1) {
            pthread_exit(NULL);
        }
        cmd = strtok(cmdBuf, " \r\n");
        if (!strcmp(cmd, "USER")) {
            // TODO: Check user and maybe ask for password.
            SendResponse(client.cmdSock, codeLoginOk, "Login successful.");
        } else if (!strcmp(cmd, "SYST")) {
            SendResponse(client.cmdSock, codeSysType, "UNIX Type: L8");
        } else if (!strcmp(cmd, "NOOP")) {
            SendResponse(client.cmdSock, codeOk, "Ok");
        } else if (!strcmp(cmd, "FEAT")) {
            SendResponse(client.cmdSock, codeSysStatus, "No extensions supported.");
        } else if (!strcmp(cmd, "PWD")) {
            char *dir = fwGetCurrentDir(worker);
            if (!dir) {
                pthread_exit(NULL);
            }
            SendResponse(client.cmdSock, codePathCreated, dir);
            free(dir);
        } else if (!strcmp(cmd, "TYPE")) {
            // TODO: Switch between text and binary mode.
            SendResponse(client.cmdSock, codeOk, "Type set to I");
        } else if (!strcmp(cmd, "PASV")) {
            struct sockaddr_in connData;
            client.mode = fmPassive;
            client.dataSock = GetPassiveDataChannel(data.srvAddr, &connData);
            SendPasvResponse(client.cmdSock, &connData);
        } else if (!strcmp(cmd, "PORT")) {
            client.mode = fmActive;
            char *args = strtok(NULL, " \r\r");
            ParsePortCommand(args, &client.dataAddr, &client.dataPort);
            SendResponse(client.cmdSock, codeOk, "PORT command successful.");
        } else if (!strcmp(cmd, "QUIT")) {
            shutdown(client.cmdSock, SHUT_RDWR);
            close(client.cmdSock);
            close(client.dataSock);
            pthread_exit(NULL);
        } else {
            SendResponse(client.cmdSock, codeUnkCmd, "Unknown command.");
        }
    }
}

void AcceptConnections(int srvSock, in_addr_t address) {
    threadData_t tData;
    // Client info
    struct sockaddr_in client;
    socklen_t clientSize;
    pthread_t clientThread;
    printf("Waiting for FTP clients...\n");
    while (1) {
        tData.srvAddr = address;
        tData.cmdSock = accept(srvSock, (struct sockaddr *)&client, &clientSize);
        if (tData.cmdSock > 0) {
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client.sin_addr, buf, INET_ADDRSTRLEN);
            printf("Client connected: %s:%d\n", buf, ntohs(client.sin_port));
            pthread_create(&clientThread, NULL, &ClientProc, &tData); 
        }
    }
}

void RunFTPServer(const char *ipAddr) {
    struct sockaddr_in srvAddr;
    int srvSock = socket(AF_INET, SOCK_STREAM, 0);
    if (srvSock == -1) {
        perror("Failed to create socket descriptor");
        exit(-1);
    }
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(ftpCmdPort);
    srvAddr.sin_addr.s_addr = inet_addr(ipAddr);
    if (bind(srvSock, (const struct sockaddr *)&srvAddr, sizeof(srvAddr)) == -1) {
        perror("Failed to bind socket to address");
        exit(-1);
    }
    if (listen(srvSock, SOMAXCONN) == -1) {
        perror("Failed to set socket to listen mode");
        exit(-1);
    }
    AcceptConnections(srvSock, srvAddr.sin_addr.s_addr);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("No address specified.\nUsage: ftp_srv ip_addr\n");
        exit(0);
    }
    // Passing ip address
    RunFTPServer(argv[1]);
    return 0;
}

