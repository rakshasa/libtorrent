#ifndef HTTP_H
#define HTTP_H

void http_get(const std::string& url);

void http_success(void* arg);

void http_failed(void* arg);

struct HttpNode;

extern std::list<HttpNode*> httpList;

#endif
