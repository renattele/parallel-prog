#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "../core/util.h"

struct account {
    long balance;
    pthread_mutex_t lock;
};

struct task {
    struct account *accounts;
    size_t count;
    size_t transfers;
    size_t id;
};

static void *run(void *ptr)
{
    struct task *task = ptr;
    size_t from = task->id;
    unsigned int seed = (unsigned int)((from + 1) * 1103515245u);
    for (size_t i = 0; i < task->transfers; i++) {
        size_t to = from;
        while (to == from)
            to = rand_r(&seed) % task->count;
        unsigned int r = rand_r(&seed);
        long amount = (long)(r % 100) + 1;
        size_t first = from < to ? from : to;
        size_t second = from < to ? to : from;
        pthread_mutex_lock(&task->accounts[first].lock);
        pthread_mutex_lock(&task->accounts[second].lock);
        long available = task->accounts[from].balance;
        if (available > 0) {
            if (amount > available)
                amount = available;
            task->accounts[from].balance -= amount;
            task->accounts[to].balance += amount;
        }
        pthread_mutex_unlock(&task->accounts[second].lock);
        pthread_mutex_unlock(&task->accounts[first].lock);
    }
    return NULL;
}

int main(void)
{
    size_t count = 16;
    size_t transfers = 200000;
    long initial = 1000;
    struct account *accounts = newarr(struct account, count);
    for (size_t i = 0; i < count; i++) {
        accounts[i].balance = initial;
        if (pthread_mutex_init(&accounts[i].lock, NULL) != 0)
            die("mutex init failed");
    }
    pthread_t *threads = newarr(pthread_t, count);
    struct task *tasks = newarr(struct task, count);
    long start_total = (long)count * initial;
    for (size_t i = 0; i < count; i++) {
        tasks[i].accounts = accounts;
        tasks[i].count = count;
        tasks[i].transfers = transfers;
        tasks[i].id = i;
        if (pthread_create(&threads[i], NULL, run, &tasks[i]) != 0)
            die("pthread_create failed");
    }
    for (size_t i = 0; i < count; i++)
        pthread_join(threads[i], NULL);
    long final_total = 0;
    for (size_t i = 0; i < count; i++)
        final_total += accounts[i].balance;
    for (size_t i = 0; i < count; i++)
        pthread_mutex_destroy(&accounts[i].lock);
    free(tasks);
    free(threads);
    free(accounts);
    if (final_total != start_total) {
        fprintf(stderr, "total mismatch: %ld vs %ld\n", start_total, final_total);
        return EXIT_FAILURE;
    }
    printf("total: %ld accounts: %zu transfers: %zu\n", final_total, count, transfers);
    return EXIT_SUCCESS;
}
