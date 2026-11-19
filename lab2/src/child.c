#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 1;
    }
    FILE *file = fopen(argv[1], "w");
    if (file == NULL) {
        perror("File error");
        return 1;
    }
    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, stdin) != -1) {
        char *num = strtok(line, " \t\n");
        if (num == NULL) {
            continue;
        }
        int result = atoi(num);
        while ((num = strtok(NULL, " \t\n")) != NULL) {
            int x = atoi(num);
            if (x == 0) {
                fprintf(stderr, "Division by zero\n");
                free(line);
                fclose(file);
                return 2;
            }
            result = result / x;
        }
        fprintf(file, "%d\n", result);
    }
    free(line);
    fclose(file);
    return 0;
}