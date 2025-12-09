#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>

// Конфигурация
#define PORT_A 9001
#define PORT_B 9002
#define IP_ADDR "127.0.0.1"
#define MAX_ITERATIONS 10
#define BUFFER_SIZE 1024

// Вспомогательная функция для обработки ошибок
void die(const char *s) {
    perror(s);
    exit(1);
}

// Функция инициализации сокета и привязки к порту
int setup_socket(int port) {
    int sockfd;
    struct sockaddr_in my_addr;

    // Создаем UDP сокет
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        die("socket");
    }

    // Настраиваем адрес
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = inet_addr(IP_ADDR);

    // Привязываем сокет к порту
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
        die("bind");
    }

    return sockfd;
}

// Процесс A: Начинает работу первым (READY -> work -> send -> SLEEP)
void process_a() {
    int sockfd = setup_socket(PORT_A);
    struct sockaddr_in dest_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(dest_addr);
    ssize_t recv_len;

    // Настройка адреса получателя (Процесс B)
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT_B);
    dest_addr.sin_addr.s_addr = inet_addr(IP_ADDR);

    printf("[Process A] Инициализация завершена. Порт: %d\n", PORT_A);
    
    // ВАЖНО: Даем процессу B немного времени на запуск и вызов recvfrom,
    // иначе первый UDP пакет может улететь в пустоту.
    sleep(1); 

    printf("[Process A] Начальное состояние: READY\n");

    for (int i = 0; i < MAX_ITERATIONS; i++) {
        printf("\n--- Итерация %d (Process A) ---\n", i + 1);
        
        // --- Состояние READY ---
        printf("[Process A] Работаю (READY)...\n");
        sleep(1); // Имитация полезной нагрузки
        
        // Отправка агента синхронизации
        printf("[Process A] Отправка агента синхронизации процессу B...\n");
        const char *msg = "TOKEN_FROM_A";
        if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
            die("sendto (A)");
        }

        // --- Переход в SLEEP ---
        // Блокируемся на ожидании ответа
        printf("[Process A] Переход в состояние SLEEP (жду пакета от B)...\n");
        fflush(stdout);

        recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&dest_addr, &addr_len);
        if (recv_len == -1) {
            die("recvfrom (A)");
        }

        buffer[recv_len] = '\0';
        
        printf("[Process A] Агент получен! Принято сообщение '%s'.\n", buffer);
        printf("Переход в READY.\n");
    }

    close(sockfd);
    printf("[Process A] Завершение работы.\n");
}

// Процесс B: Начинает с ожидания (SLEEP -> recv -> READY -> work -> send)
void process_b() {
    int sockfd = setup_socket(PORT_B);
    struct sockaddr_in dest_addr, src_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(src_addr);
    ssize_t recv_len;

    // Настройка адреса получателя (Процесс A)
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT_A);
    dest_addr.sin_addr.s_addr = inet_addr(IP_ADDR);

    printf("[Process B] Инициализация завершена. Порт: %d\n", PORT_B);
    printf("[Process B] Начальное состояние: SLEEP (жду пакета от A)\n");

    for (int i = 0; i < MAX_ITERATIONS; i++) {
        // --- Состояние SLEEP ---
        // Сразу блокируемся, так как B ведомый
        recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&src_addr, &addr_len);
        if (recv_len == -1) {
            die("recvfrom (B)");
        }

        buffer[recv_len] = '\0';
        
        // --- Переход в READY ---
        printf("\n--- Итерация %d (Process B) ---\n", i + 1);
        printf("[Process B] Агент получен! Принято сообщение '%s'.\n", buffer);
        printf("Переход в READY.\n");
        printf("[Process B] Работаю (READY)...\n");
        sleep(1); // Имитация полезной нагрузки

        // Отправка агента синхронизации обратно
        printf("[Process B] Отправка агента синхронизации процессу A...\n");
        const char *msg = "TOKEN_FROM_B";
        if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
            die("sendto (B)");
        }
        
        printf("[Process B] Переход в состояние SLEEP (жду пакета от A)...\n");
        fflush(stdout);
    }

    close(sockfd);
    printf("[Process B] Завершение работы.\n");
}

int main() {
    pid_t pid;

    printf("=== Запуск механизма пинг-понг через UDP ===\n");
    printf("Host: %s, Port A: %d, Port B: %d\n\n", IP_ADDR, PORT_A, PORT_B);

    // Создаем дочерний процесс
    pid = fork();

    if (pid == -1) {
        die("fork");
    }

    if (pid == 0) {
        // Дочерний процесс -> Process B
        process_b();
    } else {
        // Родительский процесс -> Process A
        process_a();

        // Ожидаем завершения дочернего процесса, чтобы не было зомби
        wait(NULL);
        printf("\n=== Механизм пинг-понг завершен ===\n");
    }

    return 0;
}
