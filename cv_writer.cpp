#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <thread>

const char *SHARED_MEMORY_NAME = "/my_shared_memory";
const size_t SHARED_MEMORY_SIZE = 4096;

struct SharedMemory {
    pthread_mutex_t mutex;           // Mutex for synchronization
    pthread_cond_t writer_cv;        // Condition variable for writer
    pthread_cond_t reader_cv;        // Condition variable for reader
    int buffer[100];                 // Buffer for storing integers
    int writer_index = 0;            // Writer index
    int reader_index = 0;            // Reader index
    bool full = false;               // Buffer full condition
    bool empty = true;               // Buffer empty condition
};

void writer() {
    // Open the shared memory object
    int shm_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Error opening shared memory" << std::endl;
        return;
    }

    // Set the size of the shared memory
    ftruncate(shm_fd, SHARED_MEMORY_SIZE);

    // Map shared memory into the process's address space
    auto *shared_memory = static_cast<SharedMemory*>(mmap(nullptr, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
    if (shared_memory == MAP_FAILED) {
        std::cerr << "Error mapping shared memory" << std::endl;
        close(shm_fd);
        return;
    }

    // Initialize the mutex and condition variables in shared memory
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t cond_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_condattr_init(&cond_attr);

    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&shared_memory->mutex, &mutex_attr);
    pthread_cond_init(&shared_memory->writer_cv, &cond_attr);
    pthread_cond_init(&shared_memory->reader_cv, &cond_attr);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate work

        // Lock the shared memory for writing
        pthread_mutex_lock(&shared_memory->mutex);

        // Wait if the buffer is full
        while (shared_memory->full) {
            pthread_cond_wait(&shared_memory->writer_cv, &shared_memory->mutex);
        }

        // Write an integer to the buffer
        shared_memory->buffer[shared_memory->writer_index] = shared_memory->writer_index;
        std::cout << "Writer wrote: " << shared_memory->buffer[shared_memory->writer_index] << std::endl;

        // Update the writer index
        shared_memory->writer_index = (shared_memory->writer_index + 1) % 100;

        // Update the full and empty flags
        shared_memory->full = (shared_memory->writer_index == shared_memory->reader_index);
        shared_memory->empty = false;

        // Notify the reader that data is available
        pthread_cond_signal(&shared_memory->reader_cv);

        // Unlock the shared memory
        pthread_mutex_unlock(&shared_memory->mutex);
    }

    // Cleanup shared memory
    munmap(shared_memory, SHARED_MEMORY_SIZE);
    close(shm_fd);
}

int main(){ 
    writer();
}