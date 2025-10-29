#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct context {
  long *a;
  int size;
  long start;
  long end;
};

void *calc(void *arg) {
  struct context *ctx = (struct context *)arg;
  long *sum = malloc(sizeof(long));
  *sum = 0;
  for (int i = ctx->start; i < ctx->end; i++) {
    for (int j = 0; j < 10000; j++) {
      *sum += pow(cos(sin((float)ctx->a[i])), 10);
    }
  }
  return (void *)sum;
}

int main() {
  int proc_count = 1000;
  pthread_t threads[proc_count];
  struct context ctx[proc_count];
  long size = 1000000;
  long *a = malloc(size * sizeof(long));
  for (int i = 0; i < size; i++) {
    a[i] = i + 1;
  }
  for (int i = 0; i < proc_count; i++) {
    ctx[i].start = i * (size / proc_count);
    ctx[i].end = (i + 1) * (size / proc_count);
    ctx[i].a = a;
    pthread_create(&threads[i], NULL, calc, &ctx[i]);
  }
  long total_sum = 0;
  for (int i = 0; i < proc_count; i++) {
    long *thread_sum;
    pthread_join(threads[i], (void **)&thread_sum);
    total_sum += *thread_sum;
    printf("Thread %d sum: %ld\n", i, *thread_sum);
    free(thread_sum);
  }
  printf("Total sum: %ld\n", total_sum);
  long one_thread_sum = 0;
  for (int i = 0; i < size; i++) {
    one_thread_sum += a[i];
  }
  printf("One thread sum: %ld\n", one_thread_sum);
  free(a);

  return 0;
}
