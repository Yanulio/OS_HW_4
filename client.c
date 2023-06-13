#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>

#define BUFFER_SIZE 1024

typedef struct {
    int x;
    int y;
} Coordinates;

Coordinates generate_coordinates(int field_size) {
    Coordinates coordinates;
    coordinates.x = rand() % field_size;
    coordinates.y = rand() % field_size;
    return coordinates;
}

void play_game(int server_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int field_size;
    bytes_received = recvfrom(server_socket, &field_size, sizeof(int), 0, NULL, NULL);
    if (bytes_received <= 0) {
        printf("Сервер разорвал соединение\n");
        return;
    }
    // Генерация расположения минометов противника
    srand(time(NULL));
    // Инициализация логики игры на клиенте
    int game_over = 0;

    while (!game_over) {
        Coordinates coordinates = generate_coordinates(field_size);

        // Отправляем координаты серверу
        memcpy(buffer, &coordinates, sizeof(Coordinates));
        if (sendto(server_socket, buffer, sizeof(Coordinates), 0, NULL, 0) < 0) {
            perror("Ошибка отправки данных");
            break;
        }

        // Получаем информацию о попадании от сервера
        int hit;
        bytes_received = recvfrom(server_socket, &hit, sizeof(int), 0, NULL, NULL);
        if (bytes_received <= 0) {
            printf("Сервер разорвал соединение\n");
            break;
        }

        // Обрабатываем результат попадания
        if (hit) {
            printf("Попадание по миномету противника!\n");
        } else {
            printf("Промах!\n");
        }

        // Проверяем условие завершения игры
        bytes_received = recvfrom(server_socket, &game_over, sizeof(int), 0, NULL, NULL);
        if (bytes_received <= 0) {
            printf("Сервер разорвал соединение\n");
            break;
        }
    }

    // Закрываем соединение с сервером
    close(server_socket);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Использование: %s <адрес сервера> <порт>\n", argv[0]);
        return 1;
    }

    char* server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int server_socket;
    struct sockaddr_in server_address;

    // Создаем сокет клиента
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("Не удалось создать сокет клиента");
        return 1;
    }

    // Настраиваем адрес сервера
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(server_port);

    printf("Соединение с сервером установлено\n");

    play_game(server_socket);

    return 0;
}