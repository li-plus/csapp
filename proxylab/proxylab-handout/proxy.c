#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* clang-format off */
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
/* clang-format on */

#define NUM_CACHE_OBJECTS (MAX_CACHE_SIZE / MAX_OBJECT_SIZE)

long get_clock() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

typedef struct {
    int valid;
    long clk;
    char key[MAXLINE];
    char value[MAX_OBJECT_SIZE];
    size_t value_size;
} cache_object_t;

typedef struct { cache_object_t objs[NUM_CACHE_OBJECTS]; } cache_t;

void cache_init(cache_t *cache) {
    for (int i = 0; i < NUM_CACHE_OBJECTS; i++) {
        cache_object_t *obj = &cache->objs[i];
        obj->valid = 0;
        obj->clk = 0;
        *obj->key = '\0';
        obj->value_size = 0;
    }
}

cache_object_t *cache_find(cache_t *cache, const char *key) {
    for (int i = 0; i < NUM_CACHE_OBJECTS; i++) {
        cache_object_t *obj = &cache->objs[i];
        if (obj->valid && strcmp(obj->key, key) == 0) {
            return obj;
        }
    }
    return NULL;
}

int cache_get(cache_t *cache, const char *key, char *value,
              size_t *value_size) {
    cache_object_t *obj;
    if ((obj = cache_find(cache, key)) == NULL) {
        return -1;
    }
    obj->clk = get_clock();
    memcpy(value, obj->value, obj->value_size);
    *value_size = obj->value_size;
    return 0;
}

void cache_put(cache_t *cache, const char *key, const char *value,
               size_t value_size) {
    cache_object_t *obj;

    // if key already exists, overwrite the value
    if ((obj = cache_find(cache, key)) != NULL) {
        memcpy(obj->value, value, value_size);
        obj->value_size = value_size;
        obj->clk = get_clock();
        return;
    }

    // if any slot available, insert the new entry
    for (int i = 0; i < NUM_CACHE_OBJECTS; i++) {
        obj = &cache->objs[i];
        if (!obj->valid) {
            obj->valid = 1;
            obj->clk = get_clock();
            strcpy(obj->key, key);
            memcpy(obj->value, value, value_size);
            obj->value_size = value_size;
            return;
        }
    }

    // no slots available, pick a victim to evict
    cache_object_t *victim = &cache->objs[0];
    for (int i = 1; i < NUM_CACHE_OBJECTS; i++) {
        obj = &cache->objs[i];
        if (obj->clk < victim->clk) {
            victim = obj;
        }
    }
    victim->clk = get_clock();
    strcpy(victim->key, key);
    memcpy(victim->value, value, value_size);
    victim->value_size = value_size;
}

void cache_print(cache_t *cache) {
    for (int i = 0; i < NUM_CACHE_OBJECTS; i++) {
        cache_object_t *obj = &cache->objs[i];
        printf("[object %d]: valid=%d, clk=%ld, key=%s, value_size=%zu\n", i,
               obj->valid, obj->clk, obj->key, obj->value_size);
    }
}

static cache_t cache;

typedef struct {
    sem_t mutex;
    sem_t w;
    int readcnt;
} rwlock_t;

void rwlock_init(rwlock_t *rwlock) {
    Sem_init(&rwlock->mutex, 0, 1);
    Sem_init(&rwlock->w, 0, 1);
    rwlock->readcnt = 0;
}

void rwlock_rdlock(rwlock_t *rwlock) {
    P(&rwlock->mutex);
    rwlock->readcnt++;
    if (rwlock->readcnt == 1) {
        P(&rwlock->w);
    }
    V(&rwlock->mutex);
}

void rwlock_rdunlock(rwlock_t *rwlock) {
    P(&rwlock->mutex);
    rwlock->readcnt--;
    if (rwlock->readcnt == 0) {
        V(&rwlock->w);
    }
    V(&rwlock->mutex);
}

void rwlock_wrlock(rwlock_t *rwlock) { P(&rwlock->w); }

void rwlock_wrunlock(rwlock_t *rwlock) { V(&rwlock->w); }

static rwlock_t cache_lock;

int parse_uri(const char *uri, char *host, char *path) {
#define RETURN_IF(cond)                                                        \
    if (cond) {                                                                \
        fprintf(stderr, "failed to parse uri: %s\n", uri);                     \
        return -1;                                                             \
    }

    static const char scheme[] = "http://";
    const char *s = uri;
    char *sub;
    RETURN_IF(strncasecmp(s, scheme, sizeof(scheme) - 1) != 0);
    s += sizeof(scheme) - 1;
    RETURN_IF((sub = strchr(s, '/')) == NULL);
    strncpy(host, s, sub - s);
    host[sub - s] = '\0';
    strcpy(path, sub);
    return 0;
#undef RETURN_IF
}

void parse_host(const char *host, char *hostname, char *port) {
    char *sub;
    strcpy(hostname, host);
    if ((sub = strchr(hostname, ':')) != NULL) {
        *sub = '\0';
        strcpy(port, sub + 1);
    } else {
        strcpy(port, "80");
    }
}

int read_requesthdrs(rio_t *rp, char *extra_hdrs, char *host) {
    char buf[MAXLINE], hdr_key[MAXLINE], hdr_val[MAXLINE];

    while (1) {
        if (rio_readlineb(rp, buf, MAXLINE) <= 0) {
            return -1;
        }
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }
        if (sscanf(buf, "%s %s", hdr_key, hdr_val) != 2) {
            fprintf(stderr, "failed to parse request header: %s", buf);
            return -1;
        }
        if (strcmp(hdr_key, "Host:") == 0) {
            strcpy(host, hdr_val); // overwrite host
            continue;
        }
        if (strcmp(hdr_key, "User-Agent:") == 0 ||
            strcmp(hdr_key, "Connection:") == 0 ||
            strcmp(hdr_key, "Proxy-Connection:") == 0) {
            continue;
        }
        strcpy(extra_hdrs, buf);
        extra_hdrs += strlen(buf);
    }
    return 0;
}

int write_requesthdrs(int fd, const char *host, const char *path,
                      const char *extra_hdrs) {
    char buf[MAXLINE];
    static const char hdr_fmt[] = "GET %s HTTP/1.0\r\n"
                                  "Host: %s\r\n"
                                  "%s" // user agent
                                  "Connection: close\r\n"
                                  "Proxy-Connection: close\r\n"
                                  "%s" // extra headers
                                  "\r\n";
    sprintf(buf, hdr_fmt, path, host, user_agent_hdr, extra_hdrs);
    size_t len = strlen(buf);
    if (rio_writen(fd, buf, len) != len) {
        fprintf(stderr, "failed to write request headers\n");
        return -1;
    }
    return 0;
}

void doit(int fd) {
#define RETURN_IF(cond)                                                        \
    if (cond) {                                                                \
        if (remote_fd > 0) {                                                   \
            Close(remote_fd);                                                  \
        }                                                                      \
        return;                                                                \
    }

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE],
        host[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE],
        extra_hdrs[MAXLINE], obj[MAX_OBJECT_SIZE];
    rio_t rio;
    int remote_fd = -1, rc;
    size_t obj_size;
    ssize_t n;

    /* Read request line and headers */
    rio_readinitb(&rio, fd);
    RETURN_IF(rio_readlineb(&rio, buf, MAXLINE) <= 0);
    printf("%s", buf);
    if (sscanf(buf, "%s %s %s", method, uri, version) != 3) {
        fprintf(stderr, "failed to parse http header: %s\n", buf);
        return;
    }
    if (strcasecmp(method, "GET") != 0) {
        fprintf(stderr, "unsupported method: %s\n", method);
        return;
    }
    RETURN_IF(parse_uri(uri, host, path) != 0);
    RETURN_IF(read_requesthdrs(&rio, extra_hdrs, host) != 0);

    // lookup cache
    rwlock_rdlock(&cache_lock);
    rc = cache_get(&cache, uri, obj, &obj_size);
    rwlock_rdunlock(&cache_lock);
    if (rc == 0) {
        // cache hit, return the cached object immediately
        RETURN_IF(rio_writen(fd, obj, obj_size) != obj_size);
        return;
    }

    parse_host(host, hostname, port);
    RETURN_IF((remote_fd = open_clientfd(hostname, port)) < 0);
    RETURN_IF(write_requesthdrs(remote_fd, host, path, extra_hdrs) != 0);

    // start tunnel
    obj_size = 0;
    do {
        RETURN_IF((n = rio_readn(remote_fd, buf, MAXLINE)) < 0);
        RETURN_IF(rio_writen(fd, buf, n) != n);
        if (obj_size + n <= MAX_OBJECT_SIZE) {
            memcpy(&obj[obj_size], buf, n);
        }
        obj_size += n;
    } while (n == MAXLINE);
    Close(remote_fd);

    // write cache
    if (obj_size <= MAX_OBJECT_SIZE) {
        rwlock_wrlock(&cache_lock);
        cache_put(&cache, uri, obj, obj_size);
        rwlock_wrunlock(&cache_lock);
    }
#undef RETURN_IF
}

void *thread(void *vargp) {
    int connfd = (intptr_t)vargp;
    Pthread_detach(Pthread_self());
    doit(connfd);
    Close(connfd);
    return NULL;
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    cache_init(&cache);
    rwlock_init(&cache_lock);

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port,
                    MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, &thread, (void *)(intptr_t)connfd);
    }
    return 0;
}
