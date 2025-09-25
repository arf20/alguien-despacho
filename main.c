#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <microhttpd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PORT        8888
#define RES_BUFF    65535
#define COOKIE_NAME "session"


struct Session {
    struct Session *next;
    char sid[9];
    unsigned int rc;
    int linuxers;
};

struct Request {
    struct Session *session;
    struct MHD_PostProcessor *pp;
    const char *post_url;
};

static char *index_format_template = NULL;

static int linuxers = 0;
static struct Session *sessions;



static struct Session *get_session(struct MHD_Connection *connection) {
    struct Session *ret;
    const char *cookie;

    cookie = MHD_lookup_connection_value(connection, MHD_COOKIE_KIND,
        COOKIE_NAME);
    if (cookie != NULL) {
        /* find existing session */
        ret = sessions;
        while (NULL != ret) {
            if (strcmp(cookie, ret->sid) == 0)
                break;
            ret = ret->next;
        }
        if (NULL != ret) {
            ret->rc++;
            return ret;
        }
    }
    /* create fresh session */
    ret = malloc(sizeof (struct Session));
    snprintf(ret->sid, 9, "%lx", random());
    ret->sid[8] = 0;
    ret->rc++;  
    ret->next = sessions;
    sessions = ret;
    return ret;
}

static enum MHD_Result post_iterator(
    void *cls,
	enum MHD_ValueKind kind,
	const char *key,
	const char *filename,
	const char *content_type,
	const char *transfer_encoding,
	const char *data, uint64_t off, size_t size
) {
    struct Request *request = cls;
    struct Session *session = request->session;

    printf("called %s\n", data);
    if (strcmp("linuxers", key) == 0) {
        printf("asdflinuxers=%s\n", data);
        session->linuxers = atoi(data);
        return MHD_YES;
    }
   
    fprintf(stderr, "unsupported key `%s'\n", key);
    return MHD_YES;
}

enum MHD_Result answer_to_connection(
    void *cls, struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **ptr
) {
    char buff[65535];

    const struct sockaddr_in **coninfo =
        (const struct sockaddr_in**)MHD_get_connection_info(
        connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);

    printf("%s - %s %s: ", inet_ntoa((*coninfo)->sin_addr), method, url);

    struct MHD_Response *response;
    struct Session *session;
    struct Request *request = *ptr;
    int ret;

    if (!request) {
        request = malloc(sizeof(struct Request));
        if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
            request->pp = MHD_create_post_processor(connection, 1024,
                 &post_iterator, request);
        }
    }

    if (!request->session) {
        request->session = get_session(connection);
        if (NULL == request->session) {
            fprintf(stderr, "Failed to setup session for `%s'\n", url);
            return MHD_NO; /* internal error */
        }
    }
    session = request->session;

    if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
        snprintf(buff, 65535, index_format_template, linuxers);
        response = MHD_create_response_from_buffer(strlen(buff), (void*)buff,
            MHD_RESPMEM_PERSISTENT);

        printf("%d\n", 200);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
    }
    else if (strcmp(method, "POST") == 0 && strcmp(url, "/update") == 0) {
        response = MHD_create_response_from_buffer(0, (void*)NULL, 0);
        /* evaluate POST data */
        //MHD_post_process(request->pp, upload_data, *upload_data_size);
        if (upload_data)
            sscanf(upload_data, "linuxers=%d", &linuxers);
        if (*upload_data_size) {
            *upload_data_size = 0;
            return MHD_YES;
        }

        /* done with POST data, serve response */
        MHD_destroy_post_processor(request->pp);
        request->pp = NULL;
    
        printf("%d\n", 200);
        ret = MHD_queue_response(connection, 200, response);
        MHD_destroy_response(response);

        linuxers = session->linuxers;
        printf("linuxers=%d\n", linuxers);
    } else {
        response = MHD_create_response_from_buffer(0, (void*)NULL, 0);
        printf("%d\n", 418);
        ret = MHD_queue_response(connection, 418, response);
        MHD_destroy_response(response);
    }
    return ret;
}

int main() {
    /* read index template file */
    FILE *tf = fopen("index.htm.tmpl", "r");
    if (!tf) {
        fprintf(stderr, "error opening index template file: %s\n",
            strerror(errno));
    }
    fseek(tf, 0, SEEK_END);
    size_t tfs = ftell(tf);
    rewind(tf);
    index_format_template = malloc(tfs);
    fread(index_format_template, 1, tfs, tf);
    fclose(tf);

    /* start server */
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_EPOLL,
        PORT, NULL, NULL,
        &answer_to_connection, NULL, MHD_OPTION_END);

    if (!daemon) {
        fprintf(stderr, "error starting libmicrohttpd daemon: \n");
        return 1;
    }

    while (1) {
        getc(stdin);
    }
}

