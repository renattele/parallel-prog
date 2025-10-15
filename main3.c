#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

struct context {
  pthread_mutex_t *lock;
  long *counter;
};

void *calc(void *arg) {
  struct context *ctx = (struct context *)arg;
  for (int i = 0; i < 10000; i++) {
    pthread_mutex_lock(ctx->lock);
    (*ctx->counter)++;
    pthread_mutex_unlock(ctx->lock);
  }
  return nullptr;
}

int main() {
  long counter = 0;
  int proc_count = 5;
  pthread_t threads[proc_count];
  pthread_mutex_t lock;
  pthread_mutex_init(&lock, NULL);
  struct context ctx;
  ctx.lock = &lock;
  ctx.counter = &counter;

  for (int i = 0; i < proc_count; i++) {
    pthread_create(&threads[i], NULL, calc, &ctx);
  }
  for (int i = 0; i < proc_count; i++) {
    pthread_join(threads[i], NULL);
  }
  printf("Counter: %ld\n", counter);
  return 0;
}
