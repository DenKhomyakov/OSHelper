#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <iostream>

using std::cout;
using std::endl;

volatile sig_atomic_t getSIGHUP = 0;
int acceptedSocket = -1;

void handleSignal(int signum) {
    if (signum == SIGHUP) {
        getSIGHUP = 1;
    }
    else {
        cout << "Received signal: " << signum << endl;
    }
}

int main() {
    struct sigaction sigAction;
    sigAction.sa_handler = handleSignal;
    sigAction.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sigAction, NULL);

    int serverSocket;
    int clientSocket;

    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    socklen_t clientLen = sizeof(clientAddress);

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8080);

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 1) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port 8080" << endl;

    char messageBuffer[1024];

    fd_set readDescriptorSet;
    FD_ZERO(&readDescriptorSet);

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    while (1) {
        if (getSIGHUP) {
            cout << "Received SIGHUP signal. Closing the server!" << endl;

            if (acceptedSocket != -1) {
                close(acceptedSocket);
            }

            close(serverSocket);
            exit(0);
        }

        FD_SET(serverSocket, &readDescriptorSet);

        if (acceptedSocket != -1) {
            FD_SET(acceptedSocket, &readDescriptorSet);
        }

        struct timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;

        int result = pselect(serverSocket + 1, &readDescriptorSet, NULL, NULL, &timeout, &origMask);

        if (result == -1) {
            if (errno == EINTR) {
                cout << "pselect was interrupted by a signal" << endl;
                continue;
            }
            else {
                perror("Error in pselect");
                break;
            }
        }

        if (FD_ISSET(serverSocket, &readDescriptorSet)) {
            if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientLen)) == -1) {
                perror("Error accepting connection");
                continue;
            }

            cout << "Accepted connection from " << inet_ntoa(clientAddress.sin_addr) << ", " << ntohs(clientAddress.sin_port) << endl;

            if (acceptedSocket == -1) {
                acceptedSocket = clientSocket;
            }
            else {
                close(clientSocket);
            }
        }

        if (acceptedSocket != -1 && FD_ISSET(acceptedSocket, &readDescriptorSet)) {
            size_t bytesReceived = recv(acceptedSocket, messageBuffer, sizeof(messageBuffer), 0);

            if (bytesReceived > 0) {
                messageBuffer[bytesReceived] = '\0';
                cout << "Received bytes: " << bytesReceived << ", " << messageBuffer << endl;
            }
            else if (bytesReceived == 0) {
                cout << "Connection closed by client!" << endl;
                close(acceptedSocket);
                acceptedSocket = -1;
            }
            else {
                perror("Error receiving data!");
            }
        }
    }

    close(serverSocket);
    return 0;
}