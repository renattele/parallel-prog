#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct account {
  pthread_mutex_t lock;
  long balance;
  int id;
};

struct worker_context {
  struct account *accounts;
  int account_index;
  int account_count;
  int transfer_attempts;
  long max_transfer;
};

static pthread_mutex_t rng_lock = PTHREAD_MUTEX_INITIALIZER;

static long random_range(long max_value) {
  pthread_mutex_lock(&rng_lock);
  long result = rand();
  pthread_mutex_unlock(&rng_lock);
  return result % max_value;
}

void perform_transfer(struct account *from, struct account *to, long amount) {
  struct account *first = from;
  struct account *second = to;
  if (to->id < from->id) {
    first = to;
    second = from;
  }

  pthread_mutex_lock(&first->lock);
  pthread_mutex_lock(&second->lock);

  if (from->balance >= amount) {
    from->balance -= amount;
    to->balance += amount;
  }

  pthread_mutex_unlock(&second->lock);
  pthread_mutex_unlock(&first->lock);
}

void *account_worker(void *arg) {
  struct worker_context *ctx = (struct worker_context *)arg;
  if (ctx->account_count < 2) {
    return NULL;
  }

  for (int i = 0; i < ctx->transfer_attempts; i++) {
    int target = ctx->account_index;
    while (target == ctx->account_index) {
      target = (int)random_range(ctx->account_count);
    }

    long amount = random_range(ctx->max_transfer) + 1;
    struct account *from = &ctx->accounts[ctx->account_index];
    struct account *to = &ctx->accounts[target];
    perform_transfer(from, to, amount);
  }

  return NULL;
}

int main() {
  const int account_count = 2;
  const long initial_balance = 10000;
  const int transfer_attempts = 100000;
  const long max_transfer = 100;

  struct account accounts[account_count];
  pthread_t workers[account_count];
  struct worker_context contexts[account_count];

  for (int i = 0; i < account_count; i++) {
    pthread_mutex_init(&accounts[i].lock, NULL);
    accounts[i].balance = initial_balance;
    accounts[i].id = i;
  }

  srand((unsigned int)time(NULL));

  for (int i = 0; i < account_count; i++) {
    contexts[i].accounts = accounts;
    contexts[i].account_index = i;
    contexts[i].account_count = account_count;
    contexts[i].transfer_attempts = transfer_attempts;
    contexts[i].max_transfer = max_transfer;
    pthread_create(&workers[i], NULL, account_worker, &contexts[i]);
  }

  for (int i = 0; i < account_count; i++) {
    pthread_join(workers[i], NULL);
  }

  long total = 0;
  for (int i = 0; i < account_count; i++) {
    total += accounts[i].balance;
    printf("Account %d balance: %ld\n", accounts[i].id, accounts[i].balance);
    pthread_mutex_destroy(&accounts[i].lock);
  }
  printf("Total balance: %ld\n", total);

  return 0;
}
