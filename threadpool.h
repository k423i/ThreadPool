#ifndef _THREADPOOL_H
#define _THREADPOOL_H
typedef struct ThreadPool ThreadPool;

// 创建线程池
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);

// 往线程池添加任务 中间形参是传入一个接受(void*)类型的func 这个func无返回值
// void* arg 无法转换为int 只能转换为int* 再*操作取值
void threadPoolAdd(ThreadPool* pool, void (*func)(void *), void* arg);

// 获取工作线程个数
int threadPoolBusyNum(ThreadPool* pool);

// 获取存活线程个数
int threadPoolLiveNum(ThreadPool* pool);

void* worker(void* arg);
void* manager(void* arg);
void threadExit(ThreadPool* pool);

#endif