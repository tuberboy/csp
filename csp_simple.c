#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define NUM_MESSAGES 10

// Define a channel structure
struct channel {
    int *buffer;
    int in;
    int out;
    sem_t mutex;
    sem_t items;
    sem_t spaces;
};

// Initialize the channel
void channel_init(struct channel *ch) {
    ch->buffer = (int *)malloc(BUFFER_SIZE * sizeof(int));
    if (ch->buffer == NULL) {
        perror("Error in channel_init");
        exit(EXIT_FAILURE);
    }

    ch->in = 0;
    ch->out = 0;
    sem_init(&ch->mutex, 0, 1);
    sem_init(&ch->items, 0, 0);
    sem_init(&ch->spaces, 0, BUFFER_SIZE);
}

// Send a message through the channel
void channel_send(struct channel *ch, int value) {
    sem_wait(&ch->spaces);
    sem_wait(&ch->mutex);

    ch->buffer[ch->in] = value;
    ch->in = (ch->in + 1) % BUFFER_SIZE;

    sem_post(&ch->mutex);
    sem_post(&ch->items);
}

// Receive a message from the channel
int channel_receive(struct channel *ch) {
    sem_wait(&ch->items);
    sem_wait(&ch->mutex);

    int value = ch->buffer[ch->out];
    ch->out = (ch->out + 1) % BUFFER_SIZE;

    sem_post(&ch->mutex);
    sem_post(&ch->spaces);

    return value;
}

// Free the channel resources
void channel_free(struct channel *ch) {
    free(ch->buffer);
    sem_destroy(&ch->mutex);
    sem_destroy(&ch->items);
    sem_destroy(&ch->spaces);
}

// Example processes

void *producer(void *arg) {
    struct channel *ch = (struct channel *)arg;

    for (int i = 0; i < NUM_MESSAGES; ++i) {
        printf("Producer sends: %d\n", i);
        channel_send(ch, i);
    }

    pthread_exit(NULL);
}

void *consumer(void *arg) {
    struct channel *ch = (struct channel *)arg;

    for (int i = 0; i < NUM_MESSAGES; ++i) {
        int value = channel_receive(ch);
        printf("Consumer receives: %d\n", value);
    }

    pthread_exit(NULL);
}

int main(void) {
    struct channel ch;
    channel_init(&ch);

    pthread_t producer_thread, consumer_thread;

    pthread_create(&producer_thread, NULL, producer, &ch);
    pthread_create(&consumer_thread, NULL, consumer, &ch);

    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    channel_free(&ch);

    return 0;
}
