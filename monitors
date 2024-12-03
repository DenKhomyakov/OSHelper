#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

int ready = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

void producer_up() {
    pthread_mutex_lock(&mutex);

    if (ready == 1) {
        pthread_mutex_unlock(&mutex);

        return;
    }

    ready = 1;
    printf("Provided\n");

    pthread_cond_signal(&cond_var);
    pthread_mutex_unlock(&mutex);
}

void consumer_up() {
    pthread_mutex_lock(&mutex);

    while (ready == 0) {
        pthread_cond_wait(&cond_var, &mutex);
        printf("Awoke\n");
    }

    ready = 0;
    printf("Consumed\n");

    pthread_mutex_unlock(&mutex);
}

void* producer(void* arg) {
    while (TRUE) {
        producer_up();
    }

    return NULL;
}

void* consumer(void* arg) {
    while (TRUE) {
        consumer_up();
    }

    return NULL;
}

int main() {
    pthread_t producer_thread;
    pthread_t consumer_thread;

    if (pthread_create(&producer_thread, NULL, producer, NULL) != 0) {
        perror("Failed to create producer thread!");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&consumer_thread, NULL, consumer, NULL) != 0) {
        perror("Failed to create consumer thread!");
        exit(EXIT_FAILURE);
    }

    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    return 0;
}
