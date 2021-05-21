// @url RFC 1035 https://tools.ietf.org/html/rfc1035

#include "common.h"
#include <netdb.h>
#include <sys/epoll.h> // for epoll
#include <pthread.h> // for thread
#define MAX_EVENT_NUM 256

char version[] = "0.4.3";
unsigned char buf[0xFFF];
struct epoll_event ev, events[MAX_EVENT_NUM];

uint16_t cache_id;
uint16_t send_len;
uint16_t *ans = NULL;
int nfds,sockfd,rc;

struct sockaddr_in6 in_addr;
struct sockaddr_in out_addr;
int in_addr_len;
int out_socket;
int out_addr_len;

void error(char *msg) { log_s(msg); perror(msg); exit(1); }

void *query_dns()
{
    int16_t  i,n;
    if (!ans)
    {
        out_addr_len = sizeof(out_addr);
        n = sendto(out_socket, buf, send_len, 0, (struct sockaddr *) &out_addr,  out_addr_len);
        if (n < 0) { log_s("ERROR in sendto");  }

        int ck = 0;
        uint32_t pow, i;
        while (++ck < 13)
        {
            pow = 1; for (i=0; i<ck; i++) pow <<= 1;
            usleep(pow * 1000);
            n = recvfrom(out_socket, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *) &out_addr, &out_addr_len);
            if (n < 0) continue;

            cache_answer(buf, n);

            log_b("<--P", buf, n);
            if (cache_id != *((uint16_t*)buf)) continue;

            ans = (uint16_t*)buf;
            break;
        }
        if (!ck) log_s("<--P no answer");
        send_len = n;
    }

    // send answer back
    if (ans)
    {
        n = sendto(sockfd, ans, send_len, 0, (struct sockaddr *) &in_addr, in_addr_len);
        if (n < 0) log_s("ERROR in sendto back");
    }
}

void loop(int epfd)
{
    int16_t  i,n;
    memset((char *) &out_addr, 0, sizeof(out_addr));
    out_addr.sin_family = AF_INET;
    out_addr.sin_port   = htons(config.dns_port);
    inet_aton(config.dns_ip, (struct in_addr *)&out_addr.sin_addr.s_addr);
    out_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_socket < 0) error("ERROR opening socket out");

    while (1)
    {
        nfds = epoll_wait(epfd, events, MAX_EVENT_NUM, 500);
        for(i = 0; i < nfds; ++i) {
            if(events[i].data.fd < 0) continue;
            sockfd = events[i].data.fd;
            memset(buf, 0, sizeof(buf));
            // receive datagram
            in_addr_len = sizeof(in_addr);
            n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &in_addr, &in_addr_len);
            if(n < 1) {
                events[i].data.fd = -1;
            }
            // clear Additional section, becouse of EDNS: OPTION-CODE=000A add random bytes to the end of the question
            // EDNS: https://tools.ietf.org/html/rfc2671
            THeader* ptr = (THeader*)buf;
            if (ptr->ARCOUNT > 0)
            {
                ptr->ARCOUNT = 0;
                i = sizeof(THeader);
                while (buf[i] && i < n) i += buf[i] + 1;
                n = i + 1 + 4; // COMMENT: don't forget end zero and last 2 words
            }
            // also clear Z: it's strange, but dig util set it in 0x02
            ptr->Z = 0;

            parse_buf((THeader*)buf);

            cache_id = *((uint16_t*)buf);

            log_b("Q-->", buf, n);

            if (ans = (uint16_t *)cache_search(buf, &n))
            {
                ans[0] = cache_id;
                log_b("<--C", ans, n);
            }
            else
                cache_question(buf, n);
            send_len = n;

            //multi thread or single thread
            if (config.multi_thread == 1 ){
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                pthread_t tid;
                // resend to remote server
                int err = pthread_create(&tid, &attr, query_dns, NULL);
                if(err != 0) {
                    char s[0xFF];
                    sprintf(s, "\n can't create thread:[%s]", strerror(err));
                    error(s);
                }
            }else{
                query_dns();
            }


            memset(&ev, 0, sizeof(ev));
            ev.events = EPOLLIN ;
            ev.data.fd = sockfd;
            rc = epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            if(rc < 0)
                error("Cannot mod socket to epoll!\n");
        }
    }
}

int hostname_to_ip(char *hostname, char *ip, int len)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(hostname, NULL, &hints, &servinfo) != 0) return 0;

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if (p->ai_family == AF_INET6)
        {
            struct sockaddr_in6 *serveraddr = (struct sockaddr_in6 *)p->ai_addr;
            inet_ntop(AF_INET6, (struct in_addr *)&serveraddr->sin6_addr, ip, len);
            break;
        }
        else
        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in *serveraddr = (struct sockaddr_in *)p->ai_addr;
            inet_ntop(AF_INET, (struct in_addr *)&serveraddr->sin_addr, ip, len);
        }
    }
    freeaddrinfo(servinfo);
    return 1;
}

int server_init()
{
    int sock,epfd,rc;
    // convert domain to IP
    char buf[0xFF];
    if (hostname_to_ip(config.server_ip, buf, sizeof(buf)))
        config.server_ip = buf;

    // is ipv6?
    int is_ipv6 = 0, i = 0;
    while (config.server_ip[i]) if (config.server_ip[i++] == ':') { is_ipv6 = 1; break; }

    // create socket
    sock = socket(is_ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) error("ERROR opening socket");

    //epoll create and ctl
	epfd = epoll_create(MAX_EVENT_NUM);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN ;
    ev.data.fd = sock;
    rc = epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);
    if(rc < 0)
        error("Cannot add socket to epoll!\n");

    /* setsockopt: Handy debugging trick that lets
    * us rerun the server immediately after we kill it;
    * otherwise we have to wait about 20 secs.
    * Eliminates "ERROR on binding: Address already in use" error.
    */
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval));

    // bind
    if (is_ipv6)
    {
        struct sockaddr_in6 serveraddr;  /* server's addr */
        memset((char *) &serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin6_family = AF_INET6;
        serveraddr.sin6_port   = htons(config.server_port);
        inet_pton(AF_INET6, config.server_ip, (struct in_addr *)&serveraddr.sin6_addr.s6_addr);
        if (bind(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
            error("ERROR on binding ipv6");
    }
    else
    {
        struct sockaddr_in serveraddr;  /* server's addr */
        memset((char *) &serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port   = htons(config.server_port);
        inet_aton(config.server_ip, (struct in_addr *)&serveraddr.sin_addr.s_addr);
        if (bind(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
            error("ERROR on binding ipv4");
    }

    char s[0xFF];
    sprintf(s, "already bind on %s:%d", config.server_ip, config.server_port);
    log_s(s);

    return epfd;
}

int main(int argc, char **argv)
{
    if (argv[1] && 0 == strcmp(argv[1], "--version"))
    {
        printf("tinydns %s\nCompile Time: %s %s\nAuthor: CupIvan <mail@cupivan.ru>\nEditor: Nivrrex <nivrrex@gmail.com>\nLicense: MIT\n", version,__DATE__,__TIME__);
        exit(0);
    }

    if (argv[1] && 0 == strcmp(argv[1], "--help"))
    {
        help();
        exit(0);
    }

    config_load();
    int epfd = server_init();
    loop(epfd);
}
