/*
 * net.h -- fetch a resource over HTTPS with wolfSSL.
 */

#ifndef AFR_NET_H
#define AFR_NET_H

#include <stddef.h>

/*
 * Fetch https://host/path and return the response body as a fresh string.
 * ca_file is a PEM bundle for the verification of the server certificate,
 * or NULL to try the well known bundles of the system.
 * Return 0 on success and store the body in *body. On failure return a
 * negative code and fill err with a reason. The caller frees *body.
 */
int afr_net_fetch(const char *host, const char *path, const char *ca_file,
                  char **body, char *err, size_t errsz);

#endif
