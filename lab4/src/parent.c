#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
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

int main() {
    char filename[256];
    printf("Enter output filename: ");
    fflush(stdout);
    if (fgets(filename, sizeof(filename), stdin) == NULL) {
        return 1;
    }
    filename[strcspn(filename, "\n")] = '\0';
    if (filename[0] == '\0') {
        printf("Empty filename\n");
        return 1;
    }
    int fd = open(SHARED_FILE, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    if (ftruncate(fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(fd);
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
    memset(data, 0, sizeof(SharedData));
    sem_unlink(SEM_READY);
    sem_unlink(SEM_DONE);
    sem_t *ready = sem_open(SEM_READY, O_CREAT | O_EXCL, 0600, 0);
    sem_t *done = sem_open(SEM_DONE, O_CREAT | O_EXCL, 0600, 0);
    if (ready == SEM_FAILED || done == SEM_FAILED) {
        perror("sem_open");
        if (ready != SEM_FAILED) sem_close(ready);
        if (done != SEM_FAILED) sem_close(done);
        sem_unlink(SEM_READY);
        sem_unlink(SEM_DONE);
        munmap(data, sizeof(SharedData));
        return 1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        sem_close(ready);
        sem_close(done);
        sem_unlink(SEM_READY);
        sem_unlink(SEM_DONE);
        munmap(data, sizeof(SharedData));
        return 1;
    }
    if (pid == 0) {
        execl("./child", "child", filename, NULL);
        perror("execl");
        _exit(1);
    }
    printf("Parent PID: %d\n", getpid());
    printf("Child PID: %d\n", pid);
    char input[SIZE];
    int result = 0;
    while (1) {
        printf("Enter numbers (Ctrl+D to finish): ");
        fflush(stdout);
        if (fgets(input, SIZE, stdin) == NULL) {
            data->finished = 1;
            sem_post(ready);
            break;
        }
        strcpy(data->text, input);
        if (sem_post(ready) == -1) {
            perror("sem_post");
            result = 1;
            break;
        }
        if (wait_sem(done) == -1) {
            result = 1;
            break;
        }
        if (data->error == 1) {
            printf("Division by zero. Stopping.\n");
            result = 1;
            break;
        }
    }
    if (result != 0) {
        kill(pid, SIGTERM);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        result = 1;
    } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        result = 1;
    }
    sem_close(ready);
    sem_close(done);
    sem_unlink(SEM_READY);
    sem_unlink(SEM_DONE);
    munmap(data, sizeof(SharedData));
    return result;
}