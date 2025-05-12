#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

// Константы для настройки сервера
#define PORT 8080              // Порт, на котором будет работать сервер
#define BUFFER_SIZE 4096       // Размер буфера для приема сообщений

// Структура для хранения информации о клиенте
struct Client {
    SOCKET socket;       // Сокет клиента
    std::string username; // Имя пользователя
};

// Структура для хранения сообщений
struct Message {
    int id;               // Уникальный идентификатор сообщения
    std::string sender;    // Отправитель
    std::string recipient; // Получатель (имя пользователя или группы)
    std::string content;   // Текст сообщения
    bool is_group;        // Флаг, является ли получатель группой
    bool deleted;         // Флаг удаления сообщения
};

// Глобальные переменные
std::vector<Client> clients; // Список подключенных клиентов
std::map<std::string, std::vector<Message>> group_chats; // Групповые чаты
std::mutex clients_mutex, db_mutex; // Мьютексы для синхронизации доступа

// Хеш-функция для паролей
std::string hash_password(const std::string& password) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(password));
}

// Функция обработки клиентского подключения
void handle_client(SOCKET client_socket) {

}

int main() {
    // Инициализация Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // Создание сокета сервера
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Настройка адреса сервера
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Привязка сокета к адресу
    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Начало прослушивания подключений
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Основной цикл сервера - прием новых подключений
    while (true) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        // Создание отдельного потока для каждого нового клиента
        std::thread(handle_client, client_socket).detach();
    }

    // Завершение работы
    closesocket(server_socket);
    WSACleanup();
    return 0;
}