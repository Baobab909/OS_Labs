#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <limits.h>

#define SIZE 4096
#define SHARED_FILE "shared.dat"
#define SEM_READY "/lab4_ready"
#define SEM_DONE "/lab4_done"

typedef struct {
    int finished;
    int error;
    char text[SIZE];
} SharedData;
int wait_sem(sem_t *sem) {
    while (sem_wait(sem) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            return -1;
        }
    }
    return 0;
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Output filename is missing\n");
        return 1;
    }
    int fd = open(SHARED_FILE, O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    SharedData *data = mmap(NULL, sizeof(SharedData),
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, 0);
    close(fd);
    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    sem_t *ready = sem_open(SEM_READY, 0);
    sem_t *done = sem_open(SEM_DONE, 0);
    if (ready == SEM_FAILED || done == SEM_FAILED) {
        perror("sem_open");
        if (ready != SEM_FAILED) sem_close(ready);
        if (done != SEM_FAILED) sem_close(done);
        munmap(data, sizeof(SharedData));
        return 1;
    }
    FILE *file = fopen(argv[1], "w");
    if (file == NULL) {
        perror("fopen");
        data->error = 2;
        sem_post(done);
        sem_close(ready);
        sem_close(done);
        munmap(data, sizeof(SharedData));
        return 1;
    }
    printf("Child started, PID: %d\n", getpid());
    fflush(stdout);
    int exit_code = 0;
    while (1) {
        if (wait_sem(ready) == -1) {
            exit_code = 1;
            break;
        }
        if (data->finished) {
            break;
        }
        char *word = strtok(data->text, " \t\n");
        if (word == NULL) {
            printf("Empty input\n");
            sem_post(done);
            continue;
        }
        errno = 0;
        char *end;
        long number = strtol(word, &end, 10);
        if (*end != '\0' || errno == ERANGE ||
            number < INT_MIN || number > INT_MAX) {
            printf("Invalid number\n");
            sem_post(done);
            continue;
        }
        int result = (int)number;
        int valid = 1;
        word = strtok(NULL, " \t\n");
        while (word != NULL) {
            errno = 0;
            long divisor = strtol(word, &end, 10);
            if (*end != '\0' || errno == ERANGE ||
                divisor < INT_MIN || divisor > INT_MAX) {
                printf("Invalid number\n");
                valid = 0;
                break;
            }
            if (divisor == 0) {
                data->error = 1;
                exit_code = 1;
                break;
            }
            if (result == INT_MIN && divisor == -1) {
                printf("Division overflow\n");
                valid = 0;
                break;
            }
            result = result / (int)divisor;
            word = strtok(NULL, " \t\n");
        }
        if (data->error == 1) {
            sem_post(done);
            break;
        }
        if (valid) {
            fprintf(file, "%d\n", result);
            fflush(file);
        }
        if (sem_post(done) == -1) {
            perror("sem_post");
            exit_code = 1;
            break;
        }
    }
    if (fclose(file) == EOF) {
        perror("fclose");
        exit_code = 1;
    }
    sem_close(ready);
    sem_close(done);
    munmap(data, sizeof(SharedData));
    return exit_code;
}