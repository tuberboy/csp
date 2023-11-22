#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>

struct channel_node {
    void *val;
    struct channel_node *next;
    struct channel_node *prev;
};

struct channel {
    struct channel_node *head;
    struct channel_node *tail;
    sem_t mutex;
    sem_t sent;
    sem_t received;
};

struct channel *channel_init(void) {
    struct channel *ch = malloc(sizeof(struct channel));
    if (ch == NULL) {
        perror("Error in channel_init");
        exit(EXIT_FAILURE);
    }

    ch->head = NULL;
    ch->tail = NULL;

    if (sem_init(&ch->mutex, 0, 1) == -1 ||
        sem_init(&ch->sent, 0, 0) == -1 ||
        sem_init(&ch->received, 0, 0) == -1) {
        perror("Error in semaphore initialization");
        exit(EXIT_FAILURE);
    }

    return ch;
}

void *channel_recv(struct channel *ch) {
    sem_wait(&ch->received);
    sem_wait(&ch->mutex);

    struct channel_node *node = ch->head;
    void *msg = node->val;

    if (ch->head == ch->tail) {
        ch->head = NULL;
        ch->tail = NULL;
    } else {
        ch->head = node->next;
        ch->head->prev = NULL;
    }

    free(node);

    sem_post(&ch->mutex);
    return msg;
}

void channel_wait(struct channel *ch, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        sem_wait(&ch->sent);
    }
}

void channel_send(struct channel *ch, void *msg) {
    sem_wait(&ch->mutex);

    struct channel_node *node = malloc(sizeof(struct channel_node));
    if (node == NULL) {
        perror("Error in channel_send");
        exit(EXIT_FAILURE);
    }

    node->val = msg;
    node->prev = ch->tail;
    node->next = NULL;

    if (ch->head == NULL) {
        ch->head = node;
        ch->tail = node;
    } else {
        ch->tail->next = node;
        ch->tail = node;
    }

    sem_post(&ch->received);
    sem_post(&ch->mutex);
}

size_t channel_length(struct channel *ch) {
    sem_wait(&ch->mutex);
    size_t len = 0;
    struct channel_node *node = ch->head;
    while (node != NULL) {
        node = node->next;
        ++len;
    }
    sem_post(&ch->mutex);
    return len;
}

void channel_free(struct channel *ch) {
    sem_wait(&ch->mutex);

    struct channel_node *current = ch->head;
    struct channel_node *next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    sem_post(&ch->mutex);

    sem_destroy(&ch->mutex);
    sem_destroy(&ch->sent);
    sem_destroy(&ch->received);
    free(ch);
}

void channel_select(struct channel *ch, sem_t *s1, sem_t *s2, size_t count);

sem_t *sem_select(sem_t *semaphores, size_t count);

void *pinger(void *p) {
    struct channel *ch = (struct channel *)p;
    channel_send(ch, "ping");
    channel_send(ch, "ping");
    printf("Channel length is %zd\n", channel_length(ch));

    // Use channel_select to wait for either sent or received signals
    channel_select(ch, &ch->sent, &ch->received, 2);

    char *pong = channel_recv(ch);
    printf("%s\n", pong);
    pong = channel_recv(ch);
    printf("%s\n", pong);

    printf("Channel length is %zd\n", channel_length(ch));

    pthread_exit(NULL);
    return NULL;
}

void *ponger(void *p) {
    struct channel *ch = (struct channel *)p;
    char *ping = channel_recv(ch);
    printf("%s\n", ping);
    ping = channel_recv(ch);
    printf("%s\n", ping);
    channel_send(ch, "pong");
    channel_send(ch, "pong");
    pthread_exit(NULL);
    return NULL;
}

void channel_select(struct channel *ch, sem_t *s1, sem_t *s2, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        sem_wait(&ch->mutex);
        sem_t *selected = sem_select((sem_t[]){*s1, *s2}, 2);
        sem_post(&ch->mutex);
        sem_wait(selected);
    }
}

sem_t *sem_select(sem_t *semaphores, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        int value;
        sem_getvalue(&semaphores[i], &value);
        if (value > 0) {
            return &semaphores[i];
        }
    }
    return NULL;
}

int main(void) {
    struct channel *ch = channel_init();
    pthread_t t1, t2;
    pthread_create(&t1, NULL, pinger, ch);
    pthread_create(&t2, NULL, ponger, ch);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    channel_free(ch);
    return 0;
}
