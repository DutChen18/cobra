#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_LINE 1024

static int open_clientfd(const char *host, const char *service) {
        struct addrinfo hints, *listp, *cur;
        int fd;
        int rc;

        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
        if ((rc = getaddrinfo(host, service, &hints, &listp) != 0)) {
                fprintf(stderr, "getaddrinfo failed. error(%d):\"%s\"\n", rc, gai_strerror(rc));
                return -1;
        }

        for (cur = listp; cur; cur = cur->ai_next) {
                if ((fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) < 0)
                        continue;

                if (connect(fd, cur->ai_addr, cur->ai_addrlen) >= 0)
                        break;
                close(fd);
        }

        freeaddrinfo(listp);
        if (!cur)
                return -1;
        return fd;
}

void error_and_exit() {
	perror("main");
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
}

void write_all(SSL* ssl, const void *buffer, size_t amount) {
	size_t nwritten = 0;

	while (nwritten < amount) {
		size_t n = 0;
		int rc = SSL_write_ex(ssl, (const char *) buffer + nwritten, amount - nwritten, &n);

		if (rc <= 0) {
			error_and_exit();
		}
		nwritten += n;
	}
}

int main(int argc, char **argv) {
        int clientfd;
        char *host, *port;

        if (argc != 3) {
                fprintf(stderr, "usage: %s host port\n", argv[0]);
                return EXIT_FAILURE;
        }
        host = argv[1];
        port = argv[2];

        clientfd = open_clientfd(host, port);
        if (clientfd == -1) {
                fprintf(stderr, "Failed to connect to %s:%s\n", argv[1], argv[2]);
                return EXIT_FAILURE;
        }

		const SSL_METHOD* method = TLS_client_method();
		SSL_CTX* ctx = SSL_CTX_new(method);
		if (!ctx)
			error_and_exit();

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
		if (SSL_CTX_set_default_verify_paths(ctx) == 0)
			error_and_exit();

		SSL* ssl = SSL_new(ctx);
		if (!ssl)
			error_and_exit();

		SSL_set_tlsext_host_name(ssl, host);
		SSL_set1_host(ssl, host);

		if (SSL_set_fd(ssl, clientfd) == 0)
			error_and_exit();

		if (SSL_connect(ssl) != 1)
			error_and_exit();

		char buffer[MAX_LINE];
		while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
			write_all(ssl, buffer, strlen(buffer));
		}

		char resp[MAX_LINE];
		size_t nread = 0;
		int rc = SSL_read_ex(ssl, resp, sizeof(resp), &nread);

		if (rc <= 0) {
			error_and_exit();
		}
		write(1, resp, nread);

		if (!SSL_shutdown(ssl)) {
			error_and_exit();
		}

		SSL_free(ssl);
		SSL_CTX_free(ctx);
		fprintf(stderr, "done");
        return EXIT_SUCCESS;
}

