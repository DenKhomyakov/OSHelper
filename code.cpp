#include <stdio.h>         // Стандартная библиотека ввода-вывода
#include <stdlib.h>        // Стандартная библиотека для общих функций
#include <unistd.h>        // Библиотека для работы с POSIX API
#include <signal.h>        // Библиотека для работы с сигналами
#include <arpa/inet.h>     // Библиотека для работы с сетевыми адресами
#include <sys/socket.h>    // Библиотека для работы с сокетами
#include <sys/types.h>     // Библиотека для работы с типами данных системных вызовов
#include <errno.h>         // Библиотека для работы с кодами ошибок
#include <iostream>        // Библиотека для работы с потоками ввода-вывода (C++)
#include <vector>          // Библиотека для работы с динамическими массивами (C++)

using std::cout;           // Использование пространства имен std для cout
using std::endl;           // Использование пространства имен std для endl
using std::vector;         // Использование пространства имен std для vector

volatile sig_atomic_t getSIGHUP = 0;  // Глобальная переменная для отслеживания сигнала SIGHUP

void handleSignal(int signum) {
    if (signum == SIGHUP) {  // Если получен сигнал SIGHUP
        getSIGHUP = 1;       // Устанавливаем флаг
        cout << "Received SIGHUP signal." << endl;  // Выводим сообщение о получении сигнала
    }
    else {
        cout << "Received signal: " << signum << endl;  // Выводим сообщение о получении другого сигнала
    }
}

int main() {
    struct sigaction sigAction;  // Структура для настройки обработчика сигналов
    sigAction.sa_handler = handleSignal;  // Устанавливаем обработчик сигналов
    sigAction.sa_flags = SA_RESTART;  // Флаг для автоматического перезапуска прерванных системных вызовов
    sigaction(SIGHUP, &sigAction, NULL);  // Регистрируем обработчик для сигнала SIGHUP

    int serverSocket;  // Дескриптор серверного сокета
    vector<int> clientSockets;  // Вектор для хранения дескрипторов клиентских сокетов

    struct sockaddr_in serverAddress;  // Структура для хранения адреса сервера
    struct sockaddr_in clientAddress;  // Структура для хранения адреса клиента
    socklen_t clientLen = sizeof(clientAddress);  // Размер структуры адреса клиента

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  // Создаем серверный сокет
        perror("Error creating socket");  // Выводим сообщение об ошибке
        exit(EXIT_FAILURE);  // Завершаем программу с кодом ошибки
    }

    serverAddress.sin_family = AF_INET;  // Устанавливаем семейство адресов (IPv4)
    serverAddress.sin_addr.s_addr = INADDR_ANY;  // Привязываем к любому доступному адресу
    serverAddress.sin_port = htons(8080);  // Устанавливаем порт (8080)

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {  // Привязываем сокет к адресу и порту
        perror("Error binding socket");  // Выводим сообщение об ошибке
        exit(EXIT_FAILURE);  // Завершаем программу с кодом ошибки
    }

    if (listen(serverSocket, 5) == -1) {  // Начинаем слушать входящие соединения
        perror("Error listening for connections");  // Выводим сообщение об ошибке
        exit(EXIT_FAILURE);  // Завершаем программу с кодом ошибки
    }

    cout << "Server listening on port 8080" << endl;  // Выводим сообщение о том, что сервер слушает порт 8080

    char messageBuffer[1024];  // Буфер для хранения сообщений

    sigset_t blockedMask, origMask;  // Маски для блокировки сигналов
    sigemptyset(&blockedMask);  // Инициализируем пустую маску
    sigaddset(&blockedMask, SIGHUP);  // Добавляем сигнал SIGHUP в маску
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);  // Блокируем сигнал SIGHUP

    while (1) {  // Бесконечный цикл
        fd_set readDescriptorSet;  // Набор дескрипторов для чтения
        FD_ZERO(&readDescriptorSet);  // Очищаем набор дескрипторов
        FD_SET(serverSocket, &readDescriptorSet);  // Добавляем серверный сокет в набор

        int maxFd = serverSocket;  // Максимальный дескриптор
        for (int clientSocket : clientSockets) {  // Проходим по всем клиентским сокетам
            FD_SET(clientSocket, &readDescriptorSet);  // Добавляем клиентский сокет в набор

            if (clientSocket > maxFd) {  // Если дескриптор клиентского сокета больше текущего максимального
                maxFd = clientSocket;  // Обновляем максимальный дескриптор
            }
        }

        struct timespec timeout;  // Структура для таймаута
        timeout.tv_sec = 1;  // Устанавливаем таймаут в 1 секунду
        timeout.tv_nsec = 0;  // Наносекунды (0)

        int result = pselect(maxFd + 1, &readDescriptorSet, NULL, NULL, &timeout, &origMask);  // Вызываем pselect для ожидания событий

        if (result == -1) {  // Если произошла ошибка
            if (errno == EINTR) {  // Если ошибка связана с прерыванием сигналом
                if (getSIGHUP) {  // Проверяем, был ли получен сигнал SIGHUP
                    cout << "pselect was interrupted by SIGHUP signal" << endl;  // Выводим сообщение
                    getSIGHUP = 0;  // Сбрасываем флаг
                } else {
                    cout << "pselect was interrupted by a signal other than SIGHUP" << endl;  // Выводим сообщение
                }
                continue;  // Продолжаем цикл
            }
            else {
                perror("Error in pselect");  // Выводим сообщение об ошибке
                break;  // Выходим из цикла
            }
        }

        if (FD_ISSET(serverSocket, &readDescriptorSet)) {  // Если серверный сокет готов к чтению
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientLen);  // Принимаем новое соединение

            if (clientSocket == -1) {  // Если произошла ошибка при принятии соединения
                perror("Error accepting connection");  // Выводим сообщение об ошибке
                continue;  // Продолжаем цикл
            }

            cout << "Accepted connection from " << inet_ntoa(clientAddress.sin_addr) << ", " << ntohs(clientAddress.sin_port) << endl;  // Выводим сообщение о принятом соединении
            clientSockets.push_back(clientSocket);  // Добавляем дескриптор клиентского сокета в вектор
        }

        for (auto it = clientSockets.begin(); it != clientSockets.end(); ++it) {  // Проходим по всем клиентским сокетам
            int clientSocket = *it;  // Получаем текущий дескриптор клиентского сокета

            if (FD_ISSET(clientSocket, &readDescriptorSet)) {  // Если клиентский сокет готов к чтению
                size_t bytesReceived = recv(clientSocket, messageBuffer, sizeof(messageBuffer), 0);  // Получаем данные от клиента

                if (bytesReceived > 0) {  // Если данные получены успешно
                    messageBuffer[bytesReceived] = '\0';  // Добавляем завершающий нулевой символ
                    cout << "Received bytes: " << bytesReceived << ", " << messageBuffer << endl;  // Выводим сообщение о полученных данных
                }
                else if (bytesReceived == 0) {  // Если соединение закрыто клиентом
                    cout << "Connection closed by client!" << endl;  // Выводим сообщение
                    close(clientSocket);  // Закрываем сокет

                    it = clientSockets.erase(it);  // Удаляем дескриптор сокета из вектора
                    if (it == clientSockets.end()) {  // Если достигли конца вектора
                        break;  // Выходим из цикла
                    }
                }
                else {
                    perror("Error receiving data!");  // Выводим сообщение об ошибке
                }
            }
        }
    }

    for (int clientSocket : clientSockets) {  // Проходим по всем клиентским сокетам
        close(clientSocket);  // Закрываем сокеты
    }

    close(serverSocket);  // Закрываем серверный сокет
    return 0;  // Завершаем программу
}
