#include "tasks.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#include "../core/util.h"

void task_init(struct task *task, void (*func)(void *), void *ctx)
{
    memset(task, 0, sizeof(struct task));

    task->func = func;
    task->ctx = ctx;
    task->status = TASK_STATUS_CREATED;
    pthread_mutex_init(&task->status_lock, NULL);
    pthread_cond_init(&task->status_cond, NULL);
}

struct task *task_create(void (*func)(void *), void *ctx)
{
    struct task *task = new(struct task);
    task_init(task, func, ctx);
    return task;
}

#define TASK_QUEUE_DEFAULT_MAX_DEPTH 32

static struct task_queue task_queue_blank = {};

void task_queue_init(struct task_queue *task_queue, size_t max_depth)
{
    memcpy(task_queue, &task_queue_blank, sizeof(struct task_queue));

    task_queue->max_depth = max_depth == 0
        ? TASK_QUEUE_DEFAULT_MAX_DEPTH
        : max_depth;

    task_queue->tasks = newarr(struct task *, task_queue->max_depth);

    pthread_mutex_init(&task_queue->lock, NULL);
    pthread_cond_init(&task_queue->enqueue_cond, NULL);
    pthread_cond_init(&task_queue->dequeue_cond, NULL);
}

void task_queue_move_next(struct task_queue *task_queue, size_t *index)
{
    size_t temp = *index + 1;

    if (temp == task_queue->max_depth)
        temp = 0;

    *index = temp;
}

void task_queue_enqueue(struct task_queue *task_queue, struct task *task)
{
    pthread_mutex_lock(&task_queue->lock);

    while (task_queue->count == task_queue->max_depth)
    {
        pthread_cond_wait(&task_queue->enqueue_cond, &task_queue->lock);
    }

    task_queue->tasks[task_queue->tail] = task;
    task_queue_move_next(task_queue, &task_queue->tail);
    ++task_queue->count;

    pthread_cond_signal(&task_queue->dequeue_cond);
    pthread_mutex_unlock(&task_queue->lock);
}

struct task *task_queue_dequeue(struct task_queue *task_queue)
{
    pthread_mutex_lock(&task_queue->lock);

    while (task_queue->count == 0 && !task_queue->shutdown)
    {
        pthread_cond_wait(&task_queue->dequeue_cond, &task_queue->lock);
    }

    if (task_queue->shutdown && task_queue->count == 0)
    {
        pthread_mutex_unlock(&task_queue->lock);
        return NULL;
    }

    struct task *removed = task_queue->tasks[task_queue->head];
    task_queue_move_next(task_queue, &task_queue->head);
    --task_queue->count;

    pthread_cond_signal(&task_queue->enqueue_cond);
    pthread_mutex_unlock(&task_queue->lock);

    return removed;
}

void update_task_status(struct task *task, enum task_status new_status)
{
    pthread_mutex_lock(&task->status_lock);
    task->status = new_status;
    pthread_cond_broadcast(&task->status_cond);
    pthread_mutex_unlock(&task->status_lock);
}

void *worker_routine(void *ctx)
{
    struct task_queue *task_queue = ctx;

    while (1)
    {
        struct task *task = task_queue_dequeue(task_queue);
        if (task == NULL)
            break;
        update_task_status(task, TASK_STATUS_RUNNING);
        task->func(task->ctx);
        update_task_status(task, TASK_STATUS_COMPLETED);
    }

    return NULL;
}

static struct task_sched task_sched_blank = {};

void task_sched_init(struct task_sched *task_sched, size_t worker_count, size_t task_queue_max_depth)
{
    memcpy(task_sched, &task_sched_blank, sizeof(struct task_sched));

    task_sched->worker_count = worker_count == 0
        ? get_nprocs()
        : worker_count;

    task_queue_init(&task_sched->task_queue, task_queue_max_depth);

    task_sched->workers = newarr(pthread_t, task_sched->worker_count);

    for (size_t i = 0; i < task_sched->worker_count; i++)
    {
        pthread_create(&task_sched->workers[i], NULL, worker_routine, &task_sched->task_queue);
    }
}

void run_task(struct task_sched *task_sched, struct task *task)
{
    task_queue_enqueue(&task_sched->task_queue, task);
}

void await_task(struct task *task)
{
    pthread_mutex_lock(&task->status_lock);

    while (task->status != TASK_STATUS_COMPLETED)
    {
        pthread_cond_wait(&task->status_cond, &task->status_lock);
    }

    pthread_mutex_unlock(&task->status_lock);
}

void parallel_for_n(int start,
                    int step,
                    int end,
                    void (*iter_func)(void *),
                    void *ctx,
                    size_t worker_count)
{
    struct task_sched task_sched;
    task_sched_init(&task_sched, worker_count, 0);

    int iter_count = ceil((end - start) / (double)step);
    struct iter_data *iter_data = newarr(struct iter_data, iter_count);
    struct task *tasks = newarr(struct task, iter_count);

    for (int i = 0; i < iter_count; i++)
    {
        iter_data[i].ctx = ctx;
        iter_data[i].i = start + i * step;

        task_init(&tasks[i], iter_func, &iter_data[i]);
        run_task(&task_sched, &tasks[i]);
    }

    for (int i = 0; i < iter_count; i++)
    {
        await_task(&tasks[i]);
    }

    pthread_mutex_lock(&task_sched.task_queue.lock);
    task_sched.task_queue.shutdown = 1;
    pthread_cond_broadcast(&task_sched.task_queue.dequeue_cond);
    pthread_mutex_unlock(&task_sched.task_queue.lock);

    for (size_t i = 0; i < task_sched.worker_count; i++)
    {
        pthread_join(task_sched.workers[i], NULL);
    }

    free(task_sched.workers);
    free(task_sched.task_queue.tasks);
    free(tasks);
    free(iter_data);
}

void parallel_for(int start,
                  int step,
                  int end,
                  void (*iter_func)(void *),
                  void *ctx)
{
    parallel_for_n(start, step, end, iter_func, ctx, 0);
}
