#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>


int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Query the current send buffer size
    int current_buffer_size;
    socklen_t opt_len = sizeof(current_buffer_size);
    
    if (getsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &current_buffer_size, &opt_len) == 0) {
        // Print the current send buffer size
        printf("Current send buffer size: %d\n", current_buffer_size);
    } else {
        perror("getsockopt");
        // Handle the error
    }

    // Close the socket
    close(socket_fd);

    return 0;
}