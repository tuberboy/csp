#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#define BUFFER_SIZE 100
#define PRODUCER_BATCH_SIZE 10
#define CONSUMER_BATCH_SIZE 10
#define NUM_MESSAGES 100

// Define a channel structure
struct channel {
    int *buffer;
    int in;
    int out;
    int producer_finished;  // Signal indicating the producer has finished
    pthread_mutex_t mutex;
    pthread_cond_t items;
    pthread_cond_t spaces;
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
    ch->producer_finished = 0;  // Initialize the signal
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->items, NULL);
    pthread_cond_init(&ch->spaces, NULL);
}

// Send a message through the channel
void channel_send(struct channel *ch, int value) {
    pthread_mutex_lock(&ch->mutex);

    while ((ch->in + 1) % BUFFER_SIZE == ch->out) {
        pthread_cond_wait(&ch->spaces, &ch->mutex);
    }

    ch->buffer[ch->in] = value;
    ch->in = (ch->in + 1) % BUFFER_SIZE;

    pthread_cond_signal(&ch->items);
    pthread_mutex_unlock(&ch->mutex);
}

// Receive a message from the channel
void channel_receive_batch(struct channel *ch, int *values, int count) {
    pthread_mutex_lock(&ch->mutex);

    while (ch->in == ch->out && !ch->producer_finished) {
        pthread_cond_wait(&ch->items, &ch->mutex);
    }

    for (int i = 0; i < count; ++i) {
        values[i] = ch->buffer[ch->out];
        ch->out = (ch->out + 1) % BUFFER_SIZE;
    }

    pthread_cond_signal(&ch->spaces);
    pthread_mutex_unlock(&ch->mutex);
}

// Free the channel resources
void channel_free(struct channel *ch) {
    free(ch->buffer);
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->items);
    pthread_cond_destroy(&ch->spaces);
}

// Example processes

void *producer(void *arg) {
    struct channel *ch = (struct channel *)arg;

    for (int i = 0; i < NUM_MESSAGES; i += PRODUCER_BATCH_SIZE) {
        int batch[PRODUCER_BATCH_SIZE];
        for (int j = 0; j < PRODUCER_BATCH_SIZE; ++j) {
            batch[j] = i + j;
            printf("Producer sends: %d\n", i + j);
        }

        for (int j = 0; j < PRODUCER_BATCH_SIZE; ++j) {
            channel_send(ch, batch[j]);
        }
    }

    // Signal that the producer has finished
    pthread_mutex_lock(&ch->mutex);
    ch->producer_finished = 1;
    pthread_cond_broadcast(&ch->items);
    pthread_mutex_unlock(&ch->mutex);

    pthread_exit(NULL);
}

void *consumer(void *arg) {
    struct channel *ch = (struct channel *)arg;

    while (1) {
        int batch[CONSUMER_BATCH_SIZE];
        channel_receive_batch(ch, batch, CONSUMER_BATCH_SIZE);

        for (int j = 0; j < CONSUMER_BATCH_SIZE; ++j) {
            printf("Consumer receives: %d\n", batch[j]);

            if (batch[j] == NUM_MESSAGES - 1) {
                // Exit when the last message is received
                pthread_exit(NULL);
            }
        }
    }
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
