#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sqlite3.h>
#include <openssl/rand.h>

#pragma comment(lib, "Ws2_32.lib")

// Константы для настройки сервера
#define PORT 8080              // Порт, на котором будет работать сервер
#define BUFFER_SIZE 4096       // Размер буфера для приема сообщений
#define DB_NAME "messenger.db" // Имя файла базы данных

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
extern std::vector<Client> clients; // Список подключенных клиентов
extern std::map<std::string, std::vector<Message>> group_chats; // Групповые чаты
extern std::mutex clients_mutex, db_mutex; // Мьютексы для синхронизации доступа
extern sqlite3* db;                  // Указатель на базу данных

// Прототипы функций
void init_db();
std::string hash_password(const std::string& password);
bool verify_password(const std::string& password, const std::string& stored);
std::string list_contacts(SOCKET client_socket, const std::string& username);
std::string history_messages(SOCKET client_socket, const std::string& username, const std::string& command);
void notify_user(const std::string& username, const std::string& message);
std::string list_members(SOCKET client_socket, const std::string& command);
void create_group(SOCKET client_socket, const std::string& username, const std::string& command);
void forwarding_message(const std::string& username, const std::string& command);
void delete_message(const std::string& username, const std::string& command);
void invite_to_group(SOCKET client_socket, const std::string& username, const std::string& command);
void leave_group(SOCKET client_socket, const std::string& username, const std::string& command);
std::string authorization(SOCKET client_socket, std::string& username, std::string& auth);
void handle_client(SOCKET client_socket);