#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#ifndef INBUFSIZE
#define INBUFSIZE 4096
#endif

#ifndef LISTEN_QLEN
#define LISTEN_QLEN 32
#endif

#ifndef INIT_SESS_ARR_SIZE
#define INIT_SESS_ARR_SIZE 32
#endif


enum {port = 5555, buffer_size = 4096, count = 0};

struct client_session {
    int sd;                            /* socket descriptor */
    char buf[buffer_size];             /* buffer */
    int bd;                            /* amount of data in the buffer */
    char username[30], password[50];   /* client's username and password */
    int permissions[3];                /* if permissions[0]==0 then the client can download files,if permissions[1]==0 then the client also can upload files, 
                                        * if permissions[2]==0 then the client is admin */
};

struct client_session *make_new_session(int sd, struct sockaddr_in *from) {
    struct client_session *sess = malloc(sizeof(*sess));
    sess->sd = sd;
    sess->bd = 0; 
    return sess;
}



/* =========== server =========== */

struct server_str {
    int ls;
    FILE *res;
    struct client_session **sess_array;
    int sess_array_size;
};

static int server_init(struct server_str *serv, int port)
{
    int sock, i, opt;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        perror("socket");
        return 0;
    }
    opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if(-1 == bind(sock, (struct sockaddr*) &addr, sizeof(addr))) {
        perror("bind");
        return 0;
    }

    listen(sock, LISTEN_QLEN);

    serv->ls = sock;

    serv->sess_array = malloc(sizeof(*serv->sess_array) * INIT_SESS_ARR_SIZE);
    serv->sess_array_size = INIT_SESS_ARR_SIZE;
    for(i = 0; i < INIT_SESS_ARR_SIZE; i++)
        serv->sess_array[i] = NULL;

    return 1;
}

int authenticate(struct server_str *serv, const char *username, const char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
            return 0;
        }
    }
    return 1;
}

static int session_do_read(struct session *sess)
{
    int rc, bufp = sess->buf_used;
    rc = read(sess->fd, sess->buf + bufp, INBUFSIZE - bufp);
    if(rc <= 0) {
        sess->state = fsm_error;
        return 0;   /* this means "don't continue" for the caller */
    }
    sess->buf_used += rc;
    session_check_lf(sess);
    if(sess->buf_used >= INBUFSIZE) {
        /* we can't read further, no room in the buffer, no whole line yet */
        session_send_string(sess, "Line too long! Good bye...\n");
        sess->state = fsm_error;
        return 0;
    }
    if(sess->state == fsm_finish)
        return 0;
    return 1;
}

int server_go(struct server_str *serv)
{
    fd_set readfds, writefds;
    int i, sr, ssr, maxfd;
    for(;;) { /* ========= THE APPLICATION MAIN LOOP ========= */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(serv->ls, &readfds);
        maxfd = serv->ls;
        for(i = 0; i < serv->sess_array_size; i++) {
            if(serv->sess_array[i]) {
                FD_SET(i, &readfds);
                
                FD_SET(i, &writefds);
                if(i > maxfd) {
                    maxfd = i;
                }
            }
        }
        sr = select(maxfd+1, &readfds, &writefds, NULL, NULL);
        if(sr == -1) {
            perror("select");
            return 4;
        }
        if(FD_ISSET(serv->ls, &readfds))
            server_accept_client(serv);
        for(i = 0; i < serv->sess_array_size; i++) {
            if(serv->sess_array[i] && FD_ISSET(i, &readfds)) {
                ssr = session_do_read(serv->sess_array[i], &count);
                if(!ssr) {
                    server_remove_session(serv, i);
                }
            }
        }
    }
    return 0;
}




int main(int argc, char **argv)
{
    struct server_str server;

    if(argc != 2) {
        fprintf(stderr, "Usage: serv <dir_name>\n");
        return 1;
    }

    if(!server_init(&server, port))
        return 3;

    return server_go(&server);
}
