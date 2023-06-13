#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

typedef struct {
    int x;
    int y;
    int destroyed;
} Coordinates;

void handle_client(int client_socket, int client_id, Coordinates* enemy_coordinates, int field_size, int num_mortars) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    // Генерация расположения минометов противника
    srand(time(NULL) + client_id);
    Coordinates enemy_mortars[num_mortars];
    for (int i = 0; i < num_mortars; i++) {
        enemy_mortars[i].x = rand() % field_size;
        enemy_mortars[i].y = rand() % field_size;
        enemy_mortars[i].destroyed = 0;
    }

    while (1) {
        // Принимаем координаты от клиента
        bytes_received = recvfrom(client_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_address, &client_address_length);
        if (bytes_received <= 0) {
            printf("Клиент %d отключился\n", client_id);
            break;
        }

        // Распаковываем координаты из буфера
        Coordinates coordinates;
        memcpy(&coordinates, buffer, sizeof(Coordinates));

        // Проверяем попадание по минометам противника
        int hit = 0;
        for (int i = 0; i < num_mortars; i++) {
            if (coordinates.x == enemy_mortars[i].x && coordinates.y == enemy_mortars[i].y) {
                hit = 1;
                enemy_mortars[i].destroyed = 1;
                break;
            }
        }

        // Отправляем информацию о попадании другому клиенту
        if (sendto(client_id == 0 ? client_socket : enemy_coordinates[0].x, &hit, sizeof(int), 0, (struct sockaddr*)&client_address, client_address_length) < 0) {
            perror("Ошибка отправки данных");
            break;
        }

        // Проверяем условие завершения игры
        int destroyed_mortars = 0;
        for (int i = 0; i < num_mortars; i++) {
            if (enemy_mortars[i].destroyed == 1) {
                destroyed_mortars++;
            }
        }
        if (destroyed_mortars == num_mortars) {
            // Отправляем сообщение о завершении игры клиентам
            int game_over = 1;
            if (sendto(client_socket, &game_over, sizeof(int), 0, (struct sockaddr*)&client_address, client_address_length) < 0) {
                perror("Ошибка отправки данных");
            }
            if (sendto(enemy_coordinates[0].x, &game_over, sizeof(int), 0, (struct sockaddr*)&client_address, client_address_length) < 0) {
                perror("Ошибка отправки данных");
            }
            break;
        }
    }

    // Закрываем соединение с клиентом
    close(client_socket);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Использование: %s <ip> <порт> <количество минометов> <размер поля>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int num_mortars = atoi(argv[3]);
    int field_size = atoi(argv[4]);

    if (num_mortars <= 0 || field_size <= 0) {
        printf("Неверные аргументы командной строки\n");
        return 1;
    }

    int server_socket, client_socket, client_id;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length = sizeof(client_address);

    // Создаем сокет сервера
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("Не удалось создать сокет сервера");
        return 1;
    }

    // Настраиваем адрес сервера
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(server_port);

    // Привязываем сокет к адресу сервера
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Не удалось привязать сокет к адресу");
        return 1;
    }
    printf("Сервер запущен. Ожидание клиентов...\n");

    Coordinates* enemy_coordinates = malloc(sizeof(Coordinates));
    for (client_id = 0; client_id < MAX_CLIENTS; ++client_id) {
        // Принимаем соединение от клиента
        client_socket = recvfrom(server_socket, NULL, 0, 0, (struct sockaddr*)&client_address, &client_address_length);
        if (client_socket < 0) {
            perror("Ошибка при принятии соединения");
            return 1;
        }

        printf("Принято новое соединение от клиента %d\n", client_id);

        // Обработка клиента в отдельном потоке или процессе
        if (fork() == 0) {
            // Дочерний процесс - обслуживание клиента
            close(server_socket);
            if (sendto(client_socket, &field_size, sizeof(int), 0, (struct sockaddr*)&client_address, client_address_length) < 0) {
                perror("Ошибка отправки данных");
                return 1;
            }
            handle_client(client_socket, client_id, enemy_coordinates, field_size, num_mortars);
            free(enemy_coordinates);
            exit(0);
        } else {
            // Родительский процесс - продолжение прослушивания
            close(client_socket);
        }
    }

    free(enemy_coordinates);

    // Закрываем сокет сервера
    close(server_socket);

    return 0;
}