#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "threadpool.h"
// #include "threadpool.c"

struct SockInfo{
    struct sockaddr_in addr;
    int fd;
};
// struct SockInfo infos[512];

typedef struct PoolInfo{
    ThreadPool* p;
    int fd;
}PoolInfo;

void working(void* arg);
void acceptConn(void* arg);

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if(fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999); // 主机port转为网络字节序
    saddr.sin_addr.s_addr = INADDR_ANY;
    int ret = bind(fd, (struct sockaddr*)& saddr, sizeof(saddr));
    if(ret == -1) return -1;

    ret = listen(fd, 128);
    if(ret == -1) return -1;

    // int max = sizeof(infos) / sizeof(infos[0]);
    // for(int i = 0; i < max; i ++) {
    //     bzero(&infos[i], sizeof(infos[i]));
    //     infos[i].fd = -1;
    // }

    ThreadPool* pool = threadPoolCreate(3, 8, 100);
    PoolInfo* info = (PoolInfo*)malloc(sizeof(PoolInfo));
    info->p = pool;
    info->fd = fd;
    threadPoolAdd(pool, acceptConn, info);

    pthread_exit(NULL);

    return 0;
}

void acceptConn(void* arg) {
    int addrlen = sizeof(struct sockaddr_in);
    // int fd = *(int*)arg;
    // ThreadPool* pool = ()
    PoolInfo* poolinfo = (PoolInfo*)arg;
    int fd = poolinfo->fd;
    ThreadPool* pool = poolinfo->p;
    while(1) {
        struct SockInfo* pinfo;
        pinfo = (struct SockInfo*)malloc(sizeof(struct SockInfo));

        pinfo->fd = accept(fd, (struct sockaddr*)& pinfo->addr, &addrlen);
        if(pinfo->fd == -1) {perror("accept"); break;}

        // 添加通信的任务
        threadPoolAdd(pool, working, pinfo);
    }
    close(fd);

}

void working(void* arg) {

    struct SockInfo* pinfo = (struct SockInfo*) arg;

    char ip[32];
    printf("clientIP:%s, clientPORT:%d\n", inet_ntop(AF_INET, &pinfo->addr.sin_addr.s_addr, ip, sizeof(ip)), ntohs(pinfo->addr.sin_port));

    while(1) {
        char buffer[1024];
        int len = recv(pinfo->fd, buffer, sizeof(buffer), 0);
        if(len > 0) {
            printf("client message: %s", buffer);
            send(pinfo->fd, buffer, len, 0);
        }
        else if(len == 0) {
            printf("client已断开连接");
            break;
        }
        else perror("recv");
    }
    
    close(pinfo->fd);
    // pinfo->fd = -1;
    // return NULL;
}