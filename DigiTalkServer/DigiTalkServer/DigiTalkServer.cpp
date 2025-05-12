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
    char buffer[BUFFER_SIZE];
    std::string username;

    // Аутентификация клиента
    int len = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (len <= 0) {
        closesocket(client_socket);
        return;
    }

    std::string auth(buffer, len);
    size_t sep = auth.find(':');
    if (sep == std::string::npos) {
        send(client_socket, "FAIL:Invalid format", 18, 0);
        closesocket(client_socket);
        return;
    }

    // Проверка, является ли запрос регистрацией
    bool is_register = false;
    if (auth.starts_with("REGISTER:")) {
        is_register = true;
        auth = auth.substr(9); // Удаление префикс REGISTER:
    }

    sep = auth.find(':');
    std::string user = auth.substr(0, sep);
    std::string pass = auth.substr(sep + 1);

    if (is_register) {
        // Проверка существования пользователя
        for (const auto& client : clients) {
            if (client.username == user) {
                send(client_socket, "FAIL:User already exists", 23, 0);
                closesocket(client_socket);
                return;
            }
        }

        // Регистрация нового пользователя
        send(client_socket, "OK:Registered successfully", 25, 0);
        closesocket(client_socket);
        return;
    }
    else {
        // Аутентификация
        username = user;
        send(client_socket, "OK", 2, 0);
    }

    // Добавление клиента в список подключенных
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({ client_socket, username});
    }

    // Основной цикл обработки команд от клиента
    while (true) {
        len = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (len <= 0) break;

        std::string command(buffer, len);
        if (command.starts_with("MSG:")) {
            // Обработка сообщения
            std::string msg = command.substr(4);
            size_t sep = msg.find(':');
            std::string recipient = msg.substr(0, sep);
            std::string content = msg.substr(sep + 1);

            // Пересылка сообщения получателю(ям)
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (recipient[0] == '#') {
                // Групповое сообщение
                for (const auto& client : clients) {
                    if (client.username != username) { // Не отправляем себе
                        send(client.socket, command.c_str(), command.size(), 0);
                    }
                }
            }
            else {
                // Личное сообщение
                for (const auto& client : clients) {
                    if (client.username == recipient) {
                        send(client.socket, command.c_str(), command.size(), 0);
                        break;
                    }
                }
            }
        }
        else if (command.starts_with("DEL:")) {
            // Уведомление всех клиентов об удалении
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto& client : clients) {
                send(client.socket, command.c_str(), command.size(), 0);
            }
        }
    }

    // Удаление клиента из списка при отключении
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [&](const Client& c) { return c.socket == client_socket; }), clients.end());
    }

    closesocket(client_socket);
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