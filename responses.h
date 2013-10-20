#ifndef _FTP_RESPONSES_H_
#define _FTP_RESPONSES_H_

struct response_tag {
    char *data;
    int len;
};

typedef struct response_tag response_t;

// Reply codes

// Builder
response_t *BuildResponse(int code, const char *content);
void FreeResponse(response_t *resp);

#endif
