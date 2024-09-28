#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "threadpool.h"

const int NUMBER = 2;
// 任务结构体
typedef struct Task{
    void (*function)(void* arg);
    void* arg;
}Task;

// 线程池结构体
struct ThreadPool{
    Task* taskQ;            // 任务队列
    int queueCapacity;      // 容量
    int queueSize;          // 当前任务个数
    int queueFront;         // 队头取数据
    int queueRear;          // 队尾放数据

    pthread_t managerID;    // 管理者线程ID
    pthread_t* threadIDs;   // 工作的线程ID
    
    int minNum;             // 最小线程数量
    int maxNum;             // 最大线程数量
    int busyNum;            // 正工作线程数量 工作线程一定存活
    int liveNum;            // 存活线程数量
    int exitNum;            // 要销毁的线程数量

    pthread_mutex_t mutexPool; // 锁整个的线程池
    pthread_mutex_t mutexBusy; // 锁busyNum变量
    pthread_cond_t notFull;    // 任务队列是不是满了
    pthread_cond_t notEmpty;   // 任务队列是不是空了
    
    int shutdown;           // 是否销毁线程池 销毁为1 反之为0
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do {
        if(!pool) {printf("malloc ThreadPool failed...\n"); break;}

        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if(!pool->threadIDs) {printf("malloc threadIDs failed...\n"); break;}

        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 || 
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
            pthread_cond_init(&pool->notFull, NULL) != 0 ) {
                printf("mutex or condition init failed...\n");
                break;
            }

        pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;
        pool->shutdown = 0;

        pthread_create(&pool->managerID, NULL, manager, pool);
        for(int i = 0; i < min; i ++) {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }

        return pool;
    } while(0);

    // 资源释放
    if(pool && pool->threadIDs) free(pool->threadIDs);
    if(pool && pool->taskQ) free(pool->taskQ);
    if(pool) free(pool);

    return NULL;
}

void* worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg; 

    // 线程池属于共享资源 需要上锁 访问 解锁
    while(1) {
        pthread_mutex_lock(&pool->mutexPool);

        // 当前任务队列是否为空
        while(pool->queueSize == 0 && !pool->shutdown) {
            // 任务数为0或者shutdown为1 无法进行 阻塞工作线程
            // 等待pthread_cond_signal(&pool->notEmpty)来唤醒
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            // 判断是否要销毁线程
            if(pool->exitNum > 0) {
                pool->exitNum --;
                if(pool->liveNum > pool->minNum) {
                    pool->liveNum --;
                    pthread_mutex_unlock(&pool->mutexPool);
                    // pthread_exit(NULL);
                    threadExit(pool);
                }
            }
        }

        // 判断线程池是否被关闭
        if(pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexPool);
            // pthread_exit(NULL);
            threadExit(pool);
        }

        // 开始消费 从任务队列取出一个Task
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        // 移动头节点
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize --;
        // 解锁
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool); // 结束从线程池取task 解锁

        // 执行任务
        printf("thread %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum ++;
        pthread_mutex_unlock(&pool->mutexBusy);
        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;

        printf("thread %ld end working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum --;
        pthread_mutex_unlock(&pool->mutexBusy);
    }

    return NULL;
}

void* manager(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while(!pool->shutdown) {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中任务的数量和线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        // 取出忙的线程数量
        // mutexBusy经常被访问 无需每次都锁上pool
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // 添加线程 活干不过来了
        // 任务的个数 > 存活线程数 && 存活线程数 < 最大线程数
        if(queueSize > liveNum && liveNum < pool->maxNum) {
            // 批量添加NUMBER个线程
            // 访问了pool->liveNum 需要加锁
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for(int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i ++) {
                if(pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter ++;
                    pool->liveNum ++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        // 销毁线程
        // 忙的线程 * 2 < 存活线程数 && 存活线程数 > 最小线程数
        if(busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);

            // 让工作的线程自杀
            for(int i = 0; i < NUMBER; i ++) {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPool* pool) {
    pthread_t tid = pthread_self();
    for(int i = 0; i < pool->maxNum; i ++) {
        if(pool->threadIDs[i] == tid) {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg) {
    pthread_mutex_lock(&pool->mutexPool);
    // 任务队列满了
    while(!pool->shutdown && pool->queueSize == pool->queueCapacity) {
        // 现在是满的 无法继续加任务（生产） 阻塞生产者线程 直到（pthread_cond_signal(&pool->notFull)）来唤醒
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    if(pool->shutdown) {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }

    // 添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize ++;

    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolLiveNum(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return liveNum;
}

int threadPoolDestroy(ThreadPool* pool) {
    if(!pool) return -1;

    // 关闭线程池
    pool->shutdown = 1;
    // 阻塞回收manager线程
    pthread_join(pool->managerID, NULL);
    // 唤醒阻塞的消费者线程
    for(int i = 0; i < pool->liveNum; i ++) {
        pthread_cond_signal(&pool->notEmpty);
    }
    if(pool->taskQ) free(pool->taskQ);
    if(pool->threadIDs) free(pool->threadIDs);
    
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);
    free(pool);
    pool = NULL;

    return 0;
}