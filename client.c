#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) return -1;

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    inet_pton(AF_INET, "192.168.63.131", &saddr.sin_addr.s_addr);

    int ret = connect(fd, (struct sockaddr*)& saddr, sizeof(saddr));
    if(ret == -1) return -1;

    int num = 0;
    while(1) {
        char buffer[20];
        sprintf(buffer, "client said : %d\n", num ++);
        send(fd, buffer, strlen(buffer) + 1, 0);

        memset(buffer, 0, sizeof(buffer));

        int len = recv(fd, buffer, sizeof(buffer), 0);
        if(len > 0) {
            printf("server said : %s", buffer);
        }
        else if(len == 0) {
            printf("server closed...");
            break;
        }
        else perror("recv");
        sleep(1);
    }
    close(fd);

    return 0;
}