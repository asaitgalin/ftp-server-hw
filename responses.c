#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "responses.h"

// Response template
const char *rspPattern = "%d %s\r\n";
const int rspCodeLen = 3;
const int rspEndLen = 2; // backslash r and n

response_t *BuildResponse(int code, const char *content) {
    int responseLen = rspCodeLen + strlen(content) + rspEndLen + 2; 
    char *s = (char *)calloc(responseLen, sizeof(char));
    if (!s) {
        return NULL;
    }
    response_t *resp = (response_t *)calloc(1, sizeof(response_t));
    if (!resp) {
        free(s);
        return NULL;
    }
    snprintf(s, responseLen, rspPattern, code, content);
    resp->data = s;
    resp->len = responseLen - 1; // - null terminating char
    return resp;
}

void FreeResponse(response_t *resp) {
    if (!resp) {
        return;
    }
    if (resp->data) {
        free(resp->data);
    }
}

