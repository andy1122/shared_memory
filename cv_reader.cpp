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

void reader() {
    // Open the shared memory object
    int shm_fd = shm_open(SHARED_MEMORY_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Error opening shared memory" << std::endl;
        return;
    }

    // Map shared memory into the process's address space
    auto *shared_memory = static_cast<SharedMemory*>(mmap(nullptr, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
    if (shared_memory == MAP_FAILED) {
        std::cerr << "Error mapping shared memory" << std::endl;
        close(shm_fd);
        return;
    }

    while (true) {
        // Lock the shared memory for reading
        pthread_mutex_lock(&shared_memory->mutex);

        // Wait if the buffer is empty
        while (shared_memory->empty) {
            pthread_cond_wait(&shared_memory->reader_cv, &shared_memory->mutex);
        }

        // Read an integer from the buffer
        int value = shared_memory->buffer[shared_memory->reader_index];
        std::cout << "Reader read: " << value << std::endl;

        // Update the reader index
        shared_memory->reader_index = (shared_memory->reader_index + 1) % 100;

        // Update the full and empty flags
        shared_memory->full = false;
        shared_memory->empty = (shared_memory->reader_index == shared_memory->writer_index);

        // Notify the writer that space is available
        pthread_cond_signal(&shared_memory->writer_cv);

        // Unlock the shared memory
        pthread_mutex_unlock(&shared_memory->mutex);

        //std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate work
    }

    // Cleanup shared memory
    munmap(shared_memory, SHARED_MEMORY_SIZE);
    close(shm_fd);
}

int main(){
    reader();
}