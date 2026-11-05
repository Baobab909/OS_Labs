#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <signal.h>

int main(void) {
    char filename[256];
    printf("enter filename: ");
    fflush(stdout);
    if (fgets(filename, sizeof(filename), stdin) == NULL) {
        return 1;
    }
    filename[strcspn(filename, "\n")] = '\0';
    if (filename[0] == '\0') {
        printf("empty filename\n");
        return 1;
    }
    int pipe1[2], pipe2[2];
    if (pipe(pipe1) == -1) {
        perror("pipe1");
        return 1;
    }
    if (pipe(pipe2) == -1) {
        perror("pipe2");
        close(pipe1[0]);
        close(pipe1[1]);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);
        return 1;
    }
    if (pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);
        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            perror("dup2");
            _exit(1);
        }
        if (dup2(pipe2[1], STDERR_FILENO) == -1) {
            perror("dup2");
            _exit(1);
        }
        close(pipe1[0]);
        close(pipe2[1]);
        execl("./child", "child", filename, NULL);
        perror("execl");
        _exit(1);
    }
    close(pipe1[0]);
    close(pipe2[1]);
    printf("enter numbers (ctrl+D to finish):\n");
    setvbuf(stdin, NULL, _IONBF, 0);
    char *line = NULL;
    size_t size = 0;
    int running = 1;
    int error = 0;
    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(pipe2[0], &fds);
        int maxfd = pipe2[0] > STDIN_FILENO ? pipe2[0] : STDIN_FILENO;
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            error = 1;
            break;
        }
        if (FD_ISSET(pipe2[0], &fds)) {
            char buffer[256];
            ssize_t n = read(pipe2[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                fprintf(stderr, "%s", buffer);
                error = 1;
                running = 0;
            } else if (n == 0) {
                running = 0;
            } else if (errno != EINTR) {
                perror("read");
                error = 1;
                running = 0;
            }
        }
        if (running && FD_ISSET(STDIN_FILENO, &fds)) {
            if (getline(&line, &size, stdin) == -1) {
                break;
            }
            size_t len = strlen(line);
            size_t sent = 0;
            while (sent < len) {
                ssize_t n = write(pipe1[1], line + sent, len - sent);
                if (n == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno != EPIPE) {
                        perror("write");
                    }
                    error = 1;
                    running = 0;
                    break;
                }
                sent += n;
            }
        }
    }
    free(line);
    close(pipe1[1]);
    char buffer[256];
    ssize_t n;
    while ((n = read(pipe2[0], buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, n, stderr);
        error = 1;
    }
    if (n == -1) {
        perror("read");
        error = 1;
    }
    close(pipe2[0]);
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = 1;
    }
    return error;
}