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

// ��������� ��� ��������� �������
#define PORT 8080              // ����, �� ������� ����� �������� ������
#define BUFFER_SIZE 4096       // ������ ������ ��� ������ ���������
#define DB_NAME "messenger.db" // ��� ����� ���� ������

// ��������� ��� �������� ���������� � �������
struct Client {
    SOCKET socket;       // ����� �������
    std::string username; // ��� ������������
};

// ��������� ��� �������� ���������
struct Message {
    int id;               // ���������� ������������� ���������
    std::string sender;    // �����������
    std::string recipient; // ���������� (��� ������������ ��� ������)
    std::string content;   // ����� ���������
    bool is_group;        // ����, �������� �� ���������� �������
    bool deleted;         // ���� �������� ���������
};

// ���������� ����������
extern std::vector<Client> clients; // ������ ������������ ��������
extern std::map<std::string, std::vector<Message>> group_chats; // ��������� ����
extern std::mutex clients_mutex, db_mutex; // �������� ��� ������������� �������
extern sqlite3* db;                  // ��������� �� ���� ������

// ��������� �������
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