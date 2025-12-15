#ifndef TASKS_H
#define TASKS_H

#include <stddef.h>
#include <pthread.h>

enum task_status
{
    TASK_STATUS_CREATED,
    TASK_STATUS_RUNNING,
    TASK_STATUS_COMPLETED
};

struct task
{
    void (*func)(void *);
    void *ctx;
    enum task_status status;
    pthread_mutex_t status_lock;
    pthread_cond_t status_cond;
};

struct task_queue
{
    struct task **tasks;
    size_t head;
    size_t tail;
    size_t max_depth;
    size_t count;
    int shutdown;
    pthread_mutex_t lock;
    pthread_cond_t enqueue_cond;
    pthread_cond_t dequeue_cond;
};

struct task_sched
{
    struct task_queue task_queue;
    pthread_t *workers;
    size_t worker_count;
};

struct iter_data
{
    void *ctx;
    int i;
};

struct task *task_create(void (*func)(void *), void *ctx);
void task_init(struct task *task, void (*func)(void *), void *ctx);

void task_sched_init(struct task_sched *sched,
                     size_t worker_count,
                     size_t task_queue_max_depth);
void task_sched_uninit(struct task_sched *sched);

void run_task(struct task_sched *task_sched, struct task *task);
void await_task(struct task *task);

void parallel_for(int start,
                  int step,
                  int end,
                  void (*iter_func)(void *),
                  void *ctx);

void parallel_for_n(int start,
                    int step,
                    int end,
                    void (*iter_func)(void *),
                    void *ctx,
                    size_t worker_count);

#endif // TASKS_H
