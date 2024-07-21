//Ahmadabusaleem
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

// Define the BMIResult structure
typedef struct {
    double total_bmi;
    int count;
} BMIResult;

// Function prototypes
double calculate_bmi(double height, double weight);
double naive_average_bmi(const char *filename);
double threaded_average_bmi(const char *filename, int num_threads);
double processed_average_bmi(const char *filename, int num_processes);
BMIResult process_bmi_chunk(FILE *file, int start_line, int num_lines);
void *bmi_thread_worker(void *arg);

// Calculate BMI
double calculate_bmi(double height, double weight) {
    return weight / (height * height);
}

// Naive approach to calculate average BMI
double naive_average_bmi(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Unable to open the file '%s'.\n", filename);
        return -1.0;
    }

    char line[256];
    double total_bmi = 0.0;
    int count = 0;

    // Skip the header line
    fgets(line, sizeof(line), file);

    // Process each line in the file
    while (fgets(line, sizeof(line), file)) {
        char gender[10];
        double height, weight;

        // Parse the line
        if (sscanf(line, "%9[^,],%lf,%lf", gender, &height, &weight) == 3) {
            height = height / 100.0; // Convert height from cm to meters
            double bmi = calculate_bmi(height, weight);
            total_bmi += bmi;
            count++;
        }
    }

    fclose(file);

    // Calculate the average BMI
    return count > 0 ? total_bmi / count : -1.0;
}

// Calculate BMI in a chunk
BMIResult process_bmi_chunk(FILE *file, int start_line, int num_lines) {
    char line[256];
    double total_bmi = 0.0;
    int count = 0;

    // Skip to the starting line
    for (int i = 0; i < start_line; i++) {
        fgets(line, sizeof(line), file);
    }

    // Process the specified number of lines
    for (int i = 0; i < num_lines; i++) {
        if (!fgets(line, sizeof(line), file)) {
            break;
        }

        char gender[10];
        double height, weight;

        // Parse the line
        if (sscanf(line, "%9[^,],%lf,%lf", gender, &height, &weight) == 3) {
            height = height / 100.0; // Convert height from cm to meters
            double bmi = calculate_bmi(height, weight);
            total_bmi += bmi;
            count++;
        }
    }

    // Return the result
    return (BMIResult){.total_bmi = total_bmi, .count = count};
}

// Define a struct for passing arguments to threads
typedef struct {
    const char *filename;
    int start_line;
    int num_lines;
} ThreadArgs;

// Function for thread to process a chunk of the file
void *bmi_thread_worker(void *arg) {
    ThreadArgs *thread_args = (ThreadArgs *)arg;
    const char *filename = thread_args->filename;
    int start_line = thread_args->start_line;
    int num_lines = thread_args->num_lines;

    // Open the file
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Unable to open the file '%s'.\n", filename);
        pthread_exit(NULL);
    }
    // Calculate BMI in the specified chunk
    BMIResult result = process_bmi_chunk(file, start_line, num_lines);
    // Close the file
    fclose(file);

    // Return the result as a pointer
    BMIResult *result_ptr = malloc(sizeof(BMIResult));
    if (!result_ptr) {
        printf("Error: Failed to allocate memory.\n");
        pthread_exit(NULL);
    }
    *result_ptr = result;
    // Exit the thread with the result
    pthread_exit(result_ptr);
}

// Function to calculate average BMI using multi-threading
double threaded_average_bmi(const char *filename, int num_threads) {
    pthread_t threads[num_threads];
    ThreadArgs thread_args[num_threads];
    int total_lines = count_lines_in_file(filename);
    int lines_per_thread = total_lines / num_threads;
    int remaining_lines = total_lines % num_threads;

    double total_bmi = 0.0;
    int total_count = 0;

    // Create threads to process chunks of the file
    for (int i = 0; i < num_threads; i++) {
        int start_line = i * lines_per_thread;
        int num_lines = lines_per_thread;
        if (i == num_threads - 1) {
            num_lines += remaining_lines;
        }

        thread_args[i] = (ThreadArgs){filename, start_line, num_lines};

        // Create the thread
        if (pthread_create(&threads[i], NULL, bmi_thread_worker, &thread_args[i]) != 0) {
            printf("Error: Failed to create thread %d.\n", i);
            exit(1);
        }
    }

    // Join threads and aggregate their results
    for (int i = 0; i < num_threads; i++) {
        BMIResult *result_ptr;
        if (pthread_join(threads[i], (void **)&result_ptr) != 0) {
            printf("Error: Failed to join thread %d.\n", i);
            exit(1);
        }

        // Add the result from the thread to the total
        total_bmi += result_ptr->total_bmi;
        total_count += result_ptr->count;

        // Free the result memory
        free(result_ptr);
    }

    // Calculate and return the average BMI
    return total_count > 0 ? total_bmi / total_count : -1.0;
}

// Multiprocessing approach
double processed_average_bmi(const char *filename, int num_processes) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Unable to open the file '%s'.\n", filename);
        return -1.0;
    }

    // Determine the total number of lines in the file
    int total_lines = count_lines_in_file(filename);
    int lines_per_process = total_lines / num_processes;
    int remaining_lines = total_lines % num_processes;
    // Rewind the file to the beginning
    rewind(file);

    double total_bmi = 0.0;
    int total_count = 0;
    pid_t *pids = malloc(num_processes * sizeof(pid_t));
    int pipes[num_processes][2];  // Create pipes for each child process

    // Create child processes
    for (int i = 0; i < num_processes; i++) {
        // Create pipes
        if (pipe(pipes[i]) == -1) {
            printf("Error: Failed to create pipe %d.\n", i);
            exit(1);
        }
        // Fork a child process
        pid_t pid = fork();
        if (pid < 0) {
            printf("Error: Failed to fork child process %d.\n", i);
            exit(1);
        } else if (pid == 0) {
            // Child process
            close(pipes[i][0]); // Close read end of the pipe
            // Calculate the number of lines to process
            int num_lines = lines_per_process + (i < remaining_lines ? 1 : 0);
            // Calculate the starting line
            int start_line = i * lines_per_process + (i < remaining_lines ? i : remaining_lines);
            // Open the file again in the child process
            FILE *file_child = fopen(filename, "r");
            if (!file_child) {
                printf("Error: Unable to open file '%s' in child process %d.\n", filename, i);
                exit(1);
            }
            // Calculate BMI in chunk
            BMIResult result = process_bmi_chunk(file_child, start_line, num_lines);
            // Close the file
            fclose(file_child);
            // Write the result to the pipe
            write(pipes[i][1], &result, sizeof(BMIResult));
            // Close the write end of the pipe
            close(pipes[i][1]);
            // Exit the child process
            exit(0);
        } else {
            // Parent process
            pids[i] = pid; // Store child process ID
            close(pipes[i][1]); // Close write end of the pipe in the parent process
        }
    }
    // Aggregate results from child processes
    for (int i = 0; i < num_processes; i++) {
        BMIResult result;
        // Wait for the child process to finish
        waitpid(pids[i], NULL, 0);
        // Read the result from the child process pipe
        read(pipes[i][0], &result, sizeof(BMIResult));
        // Close the read end of the pipe
        close(pipes[i][0]);
        // Aggregate the results
        total_bmi += result.total_bmi;
        total_count += result.count;
    }
    // Free the allocated memory for process IDs
    free(pids);
    // Calculate and return the average BMI
    return total_count > 0 ? total_bmi / total_count : -1.0;
}

// Function to count lines in the file
int count_lines_in_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: faile to open the file '%s'.\n", filename);
        return -1;
    }

    int line_count = 0;
    char line[256]; // Buffer to hold each line

    // Read each line and count them
    while (fgets(line, sizeof(line), file)) {
        line_count++;
    }
    // Close the file
    fclose(file);
    return line_count; // Return the total line count
}

// Main function
int main(int argc, char *argv[]) {
    const char *filename = "bmi.csv";
    int num_threads = 4; //  number of threads
    int num_processes = 4; //  number of processes

    if (argc > 1) {
        num_threads = atoi(argv[1]); // Convert the first argument to an integer for the number of threads
    }
    if (argc > 2) {
        num_processes = atoi(argv[2]); // Convert the second argument to an integer for the number of processes
    }

    // Measure the execution time of each approach
    clock_t start, end;
    double elapsed_time;

    // Naive approach
    start = clock();
    double average_bmi_naive = naive_average_bmi(filename);
    end = clock();
    elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Naive approach - Average BMI: %.2f\n", average_bmi_naive);
    printf("Naive approach execution time: %.6f seconds\n\n", elapsed_time);

    // Multithreading approach
    start = clock();
    double average_bmi_threaded = threaded_average_bmi(filename, num_threads);
    end = clock();
    elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Multithreading approach - Average BMI: %.2f\n", average_bmi_threaded);
    printf("Multithreading approach execution time: %.6f seconds\n\n", elapsed_time);

    // Multiprocessing approach
    start = clock();
    double average_bmi_processed = processed_average_bmi(filename, num_processes);
    end = clock();
    elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Multiprocessing approach - Average BMI: %.2f\n", average_bmi_processed);
    printf("Multiprocessing approach execution time: %.6f seconds\n", elapsed_time);

    return 0;
}
