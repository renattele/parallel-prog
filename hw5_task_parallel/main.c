#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "../core/util.h"

struct job {
    size_t id;
    unsigned long long limit;
    size_t pending;
    size_t result;
    pthread_mutex_t mutex;
};

struct task_node {
    struct job *job;
    unsigned long long start;
    unsigned long long end;
    struct task_node *next;
};

struct queue {
    struct task_node *head;
    struct task_node *tail;
    bool shutdown;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static void queue_init(struct queue *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
    queue->shutdown = false;
    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
        die("mutex init failed");
    if (pthread_cond_init(&queue->cond, NULL) != 0)
        die("cond init failed");
}

static void queue_destroy(struct queue *queue)
{
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

static void queue_push(struct queue *queue, struct task_node *node)
{
    node->next = NULL;
    if (queue->tail == NULL) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
}

static struct task_node *queue_pop(struct queue *queue)
{
    struct task_node *node = queue->head;
    if (node != NULL) {
        queue->head = node->next;
        if (queue->head == NULL)
            queue->tail = NULL;
    }
    return node;
}

static size_t count_primes_range(unsigned long long start, unsigned long long end)
{
    if (end < 2 || start > end)
        return 0;
    size_t total = 0;
    if (start < 2)
        start = 2;
    for (unsigned long long value = start; value <= end; value++) {
        bool prime = true;
        for (unsigned long long divisor = 2; divisor <= value / divisor; divisor++) {
            if (value % divisor == 0) {
                prime = false;
                break;
            }
        }
        if (prime)
            total++;
    }
    return total;
}

static void *worker_run(void *arg)
{
    struct queue *queue = arg;
    for (;;) {
        pthread_mutex_lock(&queue->mutex);
        while (!queue->shutdown && queue->head == NULL)
            pthread_cond_wait(&queue->cond, &queue->mutex);
        if (queue->shutdown && queue->head == NULL) {
            pthread_mutex_unlock(&queue->mutex);
            break;
        }
        struct task_node *task = queue_pop(queue);
        pthread_mutex_unlock(&queue->mutex);
        if (task != NULL) {
            struct job *job = task->job;
            size_t primes = count_primes_range(task->start, task->end);
            free(task);
            pthread_mutex_lock(&job->mutex);
            job->result += primes;
            job->pending--;
            bool done = job->pending == 0;
            size_t total = job->result;
            size_t id = job->id;
            unsigned long long limit = job->limit;
            pthread_mutex_unlock(&job->mutex);
            if (done) {
                printf("task %zu completed: %zu primes <= %llu\n", id, total, limit);
                fflush(stdout);
                pthread_mutex_destroy(&job->mutex);
                free(job);
            }
        }
    }
    return NULL;
}

static bool is_exit_token(const char *text)
{
    if (text == NULL)
        return false;
    if (strcmp(text, "q") == 0)
        return true;
    if (strcmp(text, "Q") == 0)
        return true;
    if (strcmp(text, "exit") == 0)
        return true;
    if (strcmp(text, "quit") == 0)
        return true;
    if (strcmp(text, "bye") == 0)
        return true;
    return false;
}

static unsigned long long parse_limit(const char *text, bool *ok)
{
    *ok = false;
    if (text == NULL)
        return 0;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return 0;
    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0)
        return 0;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return 0;
    *ok = true;
    return value;
}

int main(void)
{
    size_t workers = 8;
    struct queue queue;
    queue_init(&queue);
    pthread_t *threads = newarr(pthread_t, workers);
    for (size_t i = 0; i < workers; i++) {
        if (pthread_create(&threads[i], NULL, worker_run, &queue) != 0)
            die("pthread_create failed");
    }
    size_t next_id = 1;
    char buffer[256];
    for (;;) {
        if (fgets(buffer, sizeof buffer, stdin) == NULL)
            break;
        buffer[strcspn(buffer, "\r\n")] = '\0';
        char *line = buffer;
        while (isspace((unsigned char)*line))
            line++;
        char *tail = line + strlen(line);
        while (tail > line && isspace((unsigned char)tail[-1]))
            tail--;
        *tail = '\0';
        if (*line == '\0')
            continue;
        if (is_exit_token(line))
            break;
        bool ok = false;
        unsigned long long limit = parse_limit(line, &ok);
        if (!ok) {
            printf("invalid input\n");
            fflush(stdout);
            continue;
        }
        size_t id = next_id++;
        if (limit < 2) {
            printf("task %zu created for %llu\n", id, limit);
            printf("task %zu completed: 0 primes <= %llu\n", id, limit);
            fflush(stdout);
            continue;
        }
        struct job *job = new(struct job);
        job->id = id;
        job->limit = limit;
        job->pending = 0;
        job->result = 0;
        if (pthread_mutex_init(&job->mutex, NULL) != 0)
            die("mutex init failed");
        pthread_mutex_lock(&queue.mutex);
        size_t segments = workers;
        if (segments == 0)
            segments = 1;
        unsigned long long numbers = limit - 1;
        unsigned long long base = numbers / segments;
        unsigned long long extra = numbers % segments;
        unsigned long long start = 2;
        for (size_t i = 0; i < segments; i++) {
            unsigned long long chunk = base + (i < extra ? 1 : 0);
            if (chunk == 0)
                continue;
            struct task_node *task = new(struct task_node);
            task->job = job;
            task->start = start;
            task->end = start + chunk - 1;
            start = task->end + 1;
            queue_push(&queue, task);
            job->pending++;
        }
        if (job->pending == 0) {
            pthread_mutex_unlock(&queue.mutex);
            pthread_mutex_destroy(&job->mutex);
            free(job);
            printf("task %zu created for %llu\n", id, limit);
            printf("task %zu completed: 0 primes <= %llu\n", id, limit);
            fflush(stdout);
            continue;
        }
        printf("task %zu created for %llu\n", id, limit);
        fflush(stdout);
        pthread_cond_broadcast(&queue.cond);
        pthread_mutex_unlock(&queue.mutex);
    }
    pthread_mutex_lock(&queue.mutex);
    queue.shutdown = true;
    pthread_cond_broadcast(&queue.cond);
    pthread_mutex_unlock(&queue.mutex);
    for (size_t i = 0; i < workers; i++)
        pthread_join(threads[i], NULL);
    queue_destroy(&queue);
    free(threads);
    return 0;
}
