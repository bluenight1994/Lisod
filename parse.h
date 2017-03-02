#ifndef _PARSE_H_
#define _PARSE_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lisod.h"

#define SUCCESS 0
#define FIELD_SIZE    4096

//Header field
typedef struct
{
	char header_name[4096];
	char header_value[4096];
} Request_header;

//HTTP Request Header
typedef struct
{
	char http_version[50];
	char http_method[50];
	char http_uri[4096];
	Request_header *headers;
	int header_count;
} Request;

typedef struct {
    int  content_len;
    int  keep_alive;
    int  is_valid;
    char method[FIELD_SIZE];
    char version[FIELD_SIZE];
    char uri[FIELD_SIZE];
    char filename[FIELD_SIZE];
} HTTPContext;

Request* parse(char *buffer, int size, HTTPContext *context);

#endif
