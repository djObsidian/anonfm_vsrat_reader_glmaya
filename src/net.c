/*
 * net.c -- fetch a resource over HTTPS with wolfSSL.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "net.h"
#include "util.h"
#include "ca_bundle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wlong-long"
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#pragma GCC diagnostic ignored "-Wcpp"
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#include <wolfssl/ssl.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET afr_sock;
#define AFR_INVALID_SOCKET INVALID_SOCKET
#define afr_close_socket closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
typedef int afr_sock;
#define AFR_INVALID_SOCKET (-1)
#define afr_close_socket close
#endif

#define AFR_PORT "443"
#define AFR_TIMEOUT 15

static void afr_err(char *err, size_t errsz, const char *msg)
{
    if (errsz > 0) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

static void afr_net_init(void)
{
#ifdef _WIN32
    static int done = 0;
    WORD wsa_ver = 0x0202;
    WSADATA wsa;

    if (done) {
        return;
    }
    if (WSAStartup(wsa_ver, &wsa) != 0) {
        fprintf(stderr, "anonfm-reader: WSAStartup failed\n");
        exit(1);
    }
    done = 1;
#endif
}

static void afr_sock_timeout(afr_sock fd)
{
#ifdef _WIN32
    DWORD ms = (DWORD)AFR_TIMEOUT * 1000;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof(ms));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&ms, sizeof(ms));
#else
    struct timeval tv;

    tv.tv_sec = AFR_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
#endif
}

static afr_sock afr_tcp_connect(const char *host, char *err, size_t errsz)
{
    struct addrinfo hints;
    struct addrinfo *list;
    struct addrinfo *ai;
    afr_sock fd = AFR_INVALID_SOCKET;

    afr_net_init();
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host, AFR_PORT, &hints, &list) != 0) {
        afr_err(err, errsz, "cannot resolve the host name");
        return AFR_INVALID_SOCKET;
    }
    for (ai = list; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == AFR_INVALID_SOCKET) {
            continue;
        }
        afr_sock_timeout(fd);
        if (connect(fd, ai->ai_addr, (int)ai->ai_addrlen) == 0) {
            break;
        }
        afr_close_socket(fd);
        fd = AFR_INVALID_SOCKET;
    }
    if (fd == AFR_INVALID_SOCKET) {
        freeaddrinfo(list);
        afr_err(err, errsz, "cannot connect to the server");
        return AFR_INVALID_SOCKET;
    }
    freeaddrinfo(list);
    return fd;
}

/* Build or reuse the TLS context. Load a CA bundle when one is available. */
static WOLFSSL_CTX *afr_tls_ctx(const char *ca_file, char *err, size_t errsz)
{
    static WOLFSSL_CTX *ctx = NULL;
    static int failed = 0;
    int loaded = 0;

    if (ctx != NULL) {
        return ctx;
    }
    if (failed) {
        afr_err(err, errsz, "TLS context is not available");
        return NULL;
    }
    if (wolfSSL_Init() != SSL_SUCCESS) {
        failed = 1;
        afr_err(err, errsz, "wolfSSL_Init failed");
        return NULL;
    }
    ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    if (ctx == NULL) {
        failed = 1;
        wolfSSL_Cleanup();
        afr_err(err, errsz, "cannot create the TLS context");
        return NULL;
    }
    /* Load the trust anchors, in this order:
     * 1. the PEM bundle given with -C,
     * 2. the anchors embedded in the binary,
     * 3. the system store of Windows or the system bundles. */
    if (ca_file != NULL) {
        loaded = wolfSSL_CTX_load_verify_locations(ctx, ca_file, NULL)
                 == SSL_SUCCESS;
        if (!loaded) {
            fprintf(stderr, "anonfm-reader: cannot load the CA file %s\n",
                    ca_file);
        }
    }
    if (!loaded) {
        loaded = wolfSSL_CTX_load_verify_buffer(ctx,
            (const unsigned char *)afr_ca_bundle,
            (long)(sizeof(afr_ca_bundle) - 1),
            WOLFSSL_FILETYPE_PEM) == SSL_SUCCESS;
    }
    if (!loaded && ca_file == NULL) {
#ifdef _WIN32
        /* The system store of Windows holds the root certificates. */
        loaded = wolfSSL_CTX_load_system_CA_certs(ctx) == SSL_SUCCESS;
#endif
        if (!loaded) {
            static const char *bundles[] = {
                "/etc/ssl/certs/ca-certificates.crt",
                "/etc/pki/tls/certs/ca-bundle.crt",
                "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
                "/etc/ssl/cert.pem",
                NULL
            };
            int i;

            for (i = 0; bundles[i] != NULL && !loaded; i++) {
                loaded = wolfSSL_CTX_load_verify_locations(ctx, bundles[i],
                                                          NULL)
                         == SSL_SUCCESS;
            }
        }
    }
    if (loaded) {
        wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    } else {
        wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
        fprintf(stderr, "anonfm-reader: warning: the server certificate is"
                        " not verified, no CA bundle found\n");
    }
    return ctx;
}

static void afr_tls_close(WOLFSSL *ssl, afr_sock fd)
{
    wolfSSL_free(ssl);
    afr_close_socket(fd);
}

int afr_net_fetch(const char *host, const char *path, const char *ca_file,
                  char **body, char *err, size_t errsz)
{
    afr_buf req;
    afr_buf res;
    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;
    afr_sock fd;
    const char *head;
    const char *status;
    char *payload;

    *body = NULL;
    ctx = afr_tls_ctx(ca_file, err, errsz);
    if (ctx == NULL) {
        return -1;
    }
    fd = afr_tcp_connect(host, err, errsz);
    if (fd == AFR_INVALID_SOCKET) {
        return -1;
    }
    ssl = wolfSSL_new(ctx);
    if (ssl == NULL) {
        afr_err(err, errsz, "cannot create the TLS session");
        afr_close_socket(fd);
        return -1;
    }
    if (wolfSSL_set_fd(ssl, (int)fd) != SSL_SUCCESS) {
        afr_err(err, errsz, "cannot attach the socket to the TLS session");
        wolfSSL_free(ssl);
        afr_close_socket(fd);
        return -1;
    }
    if (wolfSSL_connect(ssl) != SSL_SUCCESS) {
        int code = wolfSSL_get_error(ssl, -1);

        afr_snprintf(err, errsz, "TLS handshake failed, wolfSSL error %d", code);
        afr_tls_close(ssl, fd);
        return -1;
    }

    /* Compose a plain HTTP/1.0 request. The server closes the connection
     * after the response, so no chunked encoding can appear. */
    afr_buf_init(&req);
    afr_buf_add(&req, "GET ", 4);
    afr_buf_add(&req, path, strlen(path));
    afr_buf_add(&req, " HTTP/1.0\r\nHost: ", 17);
    afr_buf_add(&req, host, strlen(host));
    afr_buf_add(&req,
                "\r\nUser-Agent: anonfm-reader/1.0\r\nAccept: text/plain, */*\r\n"
                "Accept-Encoding: identity\r\nConnection: close\r\n\r\n",
                strlen("\r\nUser-Agent: anonfm-reader/1.0\r\nAccept: text/plain, */*\r\n"
                       "Accept-Encoding: identity\r\nConnection: close\r\n\r\n"));
    if (wolfSSL_write(ssl, req.data, (int)req.len) != (int)req.len) {
        afr_buf_free(&req);
        afr_err(err, errsz, "cannot send the request");
        afr_tls_close(ssl, fd);
        return -1;
    }
    afr_buf_free(&req);

    afr_buf_init(&res);
    for (;;) {
        char chunk[8192];
        int n = wolfSSL_read(ssl, chunk, (int)sizeof(chunk));

        if (n > 0) {
            afr_buf_add(&res, chunk, (size_t)n);
            continue;
        }
        if (n == 0) {
            break;
        }
        {
            int code = wolfSSL_get_error(ssl, n);

            if (code == SSL_ERROR_SYSCALL
                || code == SSL_ERROR_ZERO_RETURN
#ifdef SSL_ERROR_CONN_LOST
                || code == SSL_ERROR_CONN_LOST
#endif
                ) {
                break; /* The peer closed the connection. */
            }
            afr_buf_free(&res);
            afr_snprintf(err, errsz, "connection lost, wolfSSL error %d", code);
            afr_tls_close(ssl, fd);
            return -1;
        }
    }
    afr_tls_close(ssl, fd);

    /* Split the response into the header and the body. */
    if (res.data == NULL) {
        afr_buf_free(&res);
        afr_err(err, errsz, "the server sent an empty response");
        return -1;
    }
    head = strstr(res.data, "\r\n\r\n");
    if (head == NULL) {
        afr_buf_free(&res);
        afr_err(err, errsz, "the server sent no response header");
        return -1;
    }
    head += 4;
    status = res.data;
    if (strncmp(status, "HTTP/", 5) != 0) {
        afr_buf_free(&res);
        afr_err(err, errsz, "the server sent no HTTP response");
        return -1;
    }
    {
        /* The status code is the second token of the status line. */
        const char *sp = strchr(status, ' ');
        int http_code;

        if (sp == NULL) {
            afr_buf_free(&res);
            afr_err(err, errsz, "bad status line from the server");
            return -1;
        }
        http_code = atoi(sp + 1);
        if (http_code != 200) {
            afr_buf_free(&res);
            afr_snprintf(err, errsz, "the server answered with HTTP %d",
                         http_code);
            return -1;
        }
    }
    payload = afr_strdup(head);
    afr_buf_free(&res);
    *body = payload;
    return 0;
}
