#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0); // ipv4 tcp 0
    
    if(fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    inet_pton(AF_INET, "192.168.63.131", &saddr.sin_addr.s_addr);
    int ret = connect(fd, (struct sockaddr*)& saddr, sizeof(saddr));
    if(ret == -1) return -1;

    int number = 0;

    while(1) {
        char buffer[20];
        sprintf(buffer, "hello world : %d\n", number ++);
        send(fd, buffer, strlen(buffer) + 1, 0);

        memset(buffer, 0, sizeof(buffer));
        int len = recv(fd, buffer, sizeof(buffer), 0);
        if(len > 0) {
            printf("server message: %s", buffer);
        }
        else if(len == 0) {
            printf("server已断开连接");
            break;
        }
        else {perror("recv"); break;}
        sleep(1);
    }
    close(fd);
    // close(cfd);
    return 0;
}
