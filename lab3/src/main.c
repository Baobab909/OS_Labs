#include <stdio.h>
#include <iostream>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    double *input;
    double *output;
    int size;
    int window;
    int start;
    int end;
} ThreadData;

void *filter_part(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int n = data->size;
    int radius = data->window / 2;
    for (int i = data->start; i < data->end; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0;
            for (int x = -radius; x <= radius; x++) {
                for (int y = -radius; y <= radius; y++) {
                    int row = i + x;
                    int col = j + y;
                    if (row >= 0 && row < n && col >= 0 && col < n) {
                        sum += data->input[row * n + col];
                    }
                }
            }
            data->output[i * n + j] = sum / (data->window * data->window);
        }
    }
    return NULL;
}
void fill_matrix(double *matrix, int n) {
    for (int i = 0; i < n * n; i++) {
        matrix[i] = (double)(i % 100);
    }
}
double get_time() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1000000000.0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s size K window threads\n", argv[0]);
        return 1;
    }
    int n = atoi(argv[1]);
    int k = atoi(argv[2]);
    int window = atoi(argv[3]);
    int threads_count = atoi(argv[4]);
    if (n <= 0 || k <= 0 || window <= 0 || window % 2 == 0 || threads_count <= 0) {
        printf("Invalid arguments\n");
        return 1;
    }
    if (threads_count > n) threads_count = n;
    double *matrix = malloc((size_t)n * n * sizeof(double));
    double *result = malloc((size_t)n * n * sizeof(double));
    if (matrix == NULL || result == NULL) {
        printf("Memory allocation error\n");
        free(matrix);
        free(result);
        return 1;
    }
    fill_matrix(matrix, n);
    pthread_t *threads = malloc(threads_count * sizeof(pthread_t));
    ThreadData *data = malloc(threads_count * sizeof(ThreadData));
    if (threads == NULL || data == NULL) {
        printf("Memory allocation error\n");
        free(matrix);
        free(result);
        free(threads);
        free(data);
        return 1;
    }
    double start_time = get_time();
    for (int step = 0; step < k; step++) {
        int created = 0;
        for (int i = 0; i < threads_count; i++) {
            data[i].input = matrix;
            data[i].output = result;
            data[i].size = n;
            data[i].window = window;
            data[i].start = i * n / threads_count;
            data[i].end = (i + 1) * n / threads_count;
            if (pthread_create(&threads[i], NULL, filter_part, &data[i]) != 0) {
                printf("Thread creation error\n");
                for (int j = 0; j < created; j++) {
                    pthread_join(threads[j], NULL);
                }
                free(matrix);
                free(result);
                free(threads);
                free(data);
                return 1;
            }
            created++;
        }
        for (int i = 0; i < threads_count; i++) {
            pthread_join(threads[i], NULL);
        }
        double *temp = matrix;
        matrix = result;
        result = temp;
    }
    double end_time = get_time();
    printf("Matrix size: %d x %d\n", n, n);
    printf("Iterations: %d\n", k);
    printf("Window size: %d\n", window);
    printf("Threads: %d\n", threads_count);
    printf("Execution time: %.6f seconds\n", end_time - start_time);
    printf("Result sample: %.4f\n", matrix[(n / 2) * n + n / 2]);
    free(matrix);
    free(result);
    free(threads);
    free(data);
    return 0;
}