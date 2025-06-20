#ifndef ERROR_H
#define ERROR_H

void send_error_response(int client_fd, int status_code, const char* reason);

#endif
