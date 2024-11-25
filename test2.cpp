#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

volatile sig_atomic_t getSIGHUP = 0;

void handleSignal(int signum) {
    if (signum == SIGHUP) {
        getSIGHUP = 1;
        cout << "Received SIGHUP signal." << endl;
    } else {
        cout << "Received signal: " << signum << endl;
    }
}

int main() {
    struct sigaction sigAction;
    sigAction.sa_handler = handleSignal;
    sigAction.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sigAction, NULL);

    int serverSocket;
    vector<int> clientSockets;

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

    if (listen(serverSocket, 5) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port 8080" << endl;

    char messageBuffer[1024];

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    while (1) {
        fd_set readDescriptorSet;
        FD_ZERO(&readDescriptorSet);
        FD_SET(serverSocket, &readDescriptorSet);

        int maxFd = serverSocket;
        for (int clientSocket : clientSockets) {
            FD_SET(clientSocket, &readDescriptorSet);
            if (clientSocket > maxFd) {
                maxFd = clientSocket;
            }
        }

        struct timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;

        int result = pselect(maxFd + 1, &readDescriptorSet, NULL, NULL, &timeout, &origMask);

        if (result == -1) {
            if (errno == EINTR) {
                cout << "pselect was interrupted by a signal" << endl;
                continue;
            } else {
                perror("Error in pselect");
                break;
            }
        }

        if (FD_ISSET(serverSocket, &readDescriptorSet)) {
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientLen);
            if (clientSocket == -1) {
                perror("Error accepting connection");
                continue;
            }

            cout << "Accepted connection from " << inet_ntoa(clientAddress.sin_addr) << ", " << ntohs(clientAddress.sin_port) << endl;
            clientSockets.push_back(clientSocket);
        }

        for (auto it = clientSockets.begin(); it != clientSockets.end(); ++it) {
            int clientSocket = *it;
            if (FD_ISSET(clientSocket, &readDescriptorSet)) {
                size_t bytesReceived = recv(clientSocket, messageBuffer, sizeof(messageBuffer), 0);

                if (bytesReceived > 0) {
                    messageBuffer[bytesReceived] = '\0';
                    cout << "Received bytes: " << bytesReceived << ", " << messageBuffer << endl;
                } else if (bytesReceived == 0) {
                    cout << "Connection closed by client!" << endl;
                    close(clientSocket);
                    it = clientSockets.erase(it);
                    if (it == clientSockets.end()) {
                        break;
                    }
                } else {
                    perror("Error receiving data!");
                }
            }
        }
    }

    for (int clientSocket : clientSockets) {
        close(clientSocket);
    }
    close(serverSocket);
    return 0;
}
