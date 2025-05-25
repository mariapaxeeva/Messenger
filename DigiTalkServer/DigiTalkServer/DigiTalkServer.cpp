#include "DigiTalk.h"

// Инициализация глобальных переменных
std::vector<Client> clients; // Список подключенных клиентов
std::map<std::string, std::vector<Message>> group_chats; // Групповые чаты
std::mutex clients_mutex, db_mutex; // Мьютексы для синхронизации доступа
sqlite3* db;                  // Указатель на базу данных

int main() {
    // Инициализация Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    init_db(); // Инициализация базы данных

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
    sqlite3_close(db);
    closesocket(server_socket);
    WSACleanup();
    return 0;
}


// Инициализация базы данных
void init_db() {
    sqlite3_open(DB_NAME, &db);

    // SQL-запросы для создания таблиц, если они не существуют
    const char* sql =
        "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT);"
        "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "sender TEXT, recipient TEXT, content BLOB, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, is_group INTEGER, deleted INTEGER);"
        "CREATE TABLE IF NOT EXISTS groups(id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE NOT NULL, creator TEXT NOT NULL, created_at DATETIME DEFAULT CURRENT_TIMESTAMP);"
        "CREATE TABLE IF NOT EXISTS group_members(group_id INTEGER NOT NULL, "
            "username TEXT NOT NULL, joined_at DATETIME DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY(group_id, username), "
            "FOREIGN KEY(group_id) REFERENCES groups(id), FOREIGN KEY(username) REFERENCES users(username));"
        "CREATE TABLE IF NOT EXISTS deleted_messages(message_id INTEGER NOT NULL, username TEXT NOT NULL, "
            "PRIMARY KEY(message_id, username), FOREIGN KEY(message_id) REFERENCES messages(id), "
            "FOREIGN KEY(username) REFERENCES users(username));";
    sqlite3_exec(db, sql, 0, 0, 0);
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
    authorization(client_socket, username, auth);

    // Добавление клиента в список подключенных
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({ client_socket, username });
    }

    // Обновление списка контактов для всех подключенных клиентов
    for (const auto& client : clients) {
        list_contacts(client.socket, client.username);
    }

    // Основной цикл обработки команд от клиента
    while (true) {
        len = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (len <= 0) break;

        std::string command(buffer, len);
        if (command.starts_with("MSG:")) {
            forwarding_message(username, command);
        }
        else if (command.starts_with("DEL:")) {
            delete_message(username, command);
        }
        else if (command.starts_with("GET_HISTORY:")) {
            history_messages(client_socket, username, command);
        }
        else if (command.starts_with("GET_GROUP_MEMBERS:")) {
            list_members(client_socket, command);
        }
        else if (command.starts_with("CREATE_GROUP:")) {
            create_group(client_socket, username, command);
        }
        else if (command.starts_with("INVITE_TO_GROUP:")) {
            invite_to_group(client_socket, username, command);
        }
        else if (command.starts_with("LEAVE_GROUP:")) {
            leave_group(client_socket, username, command);
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


// Обработчик команд авторизации и регистрации
std::string authorization(SOCKET client_socket, std::string& username, std::string& auth) {
    size_t sep = auth.find(':');
    if (sep == std::string::npos) {
        send(client_socket, "FAIL:Invalid format", 18, 0);
        closesocket(client_socket);
        return NULL;
    }

    // Проверка, является ли запрос регистрацией
    bool is_register = false;
    if (auth.starts_with("REGISTER:")) {
        is_register = true;
        auth = auth.substr(9); // Удаление префикса REGISTER:
    }

    sep = auth.find(':');
    std::string user = auth.substr(0, sep);
    std::string pass = auth.substr(sep + 1);

    sqlite3_stmt* stmt;
    if (is_register) {
        // Регистрация нового пользователя
        sqlite3_prepare_v2(db, "SELECT username FROM users WHERE username = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            send(client_socket, "FAIL:User already exists", 23, 0);
            sqlite3_finalize(stmt);
            closesocket(client_socket);
            return NULL;
        }
        sqlite3_finalize(stmt);

        // Хеширование пароля и сохранение пользователя
        std::string hashed_pass = hash_password(pass);
        sqlite3_prepare_v2(db, "INSERT INTO users (username, password) VALUES (?, ?)", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hashed_pass.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            username = user;
            send(client_socket, "OK:Registered successfully", 25, 0);
        }
        else {
            send(client_socket, "FAIL:Registration failed", 23, 0);
            closesocket(client_socket);
            sqlite3_finalize(stmt);
            return NULL;
        }
    }
    else {
        // Аутентификация
        sqlite3_prepare_v2(db, "SELECT password FROM users WHERE username = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW && verify_password(pass, (const char*)sqlite3_column_text(stmt, 0))) {
            username = user;
            send(client_socket, "OK: Authentification successfully", 2, 0);
        }
        else {
            send(client_socket, "FAIL:Invalid credentials", 24, 0);
            closesocket(client_socket);
            sqlite3_finalize(stmt);
            return NULL;
        }
    }
    sqlite3_finalize(stmt);
    return username;
}


// Хеш-функция для паролей
std::string hash_password(const std::string& password) {
    unsigned char salt[16];
    RAND_bytes(salt, 16);  // Генерация случайной соли
    unsigned char hash[32];

    // Используем PBKDF2 с SHA-256 для хеширования
    PKCS5_PBKDF2_HMAC(
        password.c_str(), password.length(),
        salt, 16,
        100000,  // Количество итераций для замедления брутфорса
        EVP_sha256(),
        32,  // Размер хеша
        hash
    );

    // Сохраняю соль и хеш вместе
    std::string result = std::string((char*)salt, 16) + std::string((char*)hash, 32);
    return result;
}


// Проверка пароля
bool verify_password(const std::string& password, const std::string& stored) {
    if (stored.size() != 48) return false;  // Проверка размера (16 соль + 32 хеш)

    unsigned char salt[16];
    memcpy(salt, stored.c_str(), 16);  // Извлечение соли

    unsigned char hash[32];
    // Повторное вычисление хеша с той же солью
    PKCS5_PBKDF2_HMAC(
        password.c_str(), password.length(),
        salt, 16,
        100000,
        EVP_sha256(),
        32,
        hash
    );

    // Сравнение с сохраненным хешем
    return memcmp(hash, stored.c_str() + 16, 32) == 0;
}


// Формирование и отправка клиенту списка пользователей и групп
std::string list_contacts(SOCKET client_socket, const std::string& username) {
    // Формирование списка пользователей
    std::string contacts_list = "CONTACTS:";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "SELECT username FROM users", -1, &stmt, 0);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        contacts_list += (const char*)sqlite3_column_text(stmt, 0);
        contacts_list += ",";
    }
    sqlite3_finalize(stmt);

    // Формирование списка групп
    contacts_list += "|GROUPS:";
    sqlite3_prepare_v2(db,
        "SELECT g.name FROM groups g "
        "JOIN group_members gm ON g.id = gm.group_id "
        "WHERE gm.username = ?", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        contacts_list += (const char*)sqlite3_column_text(stmt, 0);
        contacts_list += ",";
    }
    sqlite3_finalize(stmt);

    // Удаление последней запятой, если она есть
    if (!contacts_list.empty() && contacts_list.back() == ',') {
        contacts_list.pop_back();
    }
    send(client_socket, contacts_list.c_str(), contacts_list.size(), 0);

    return contacts_list;
}


// Извлечение истории сообщений пользователя с recipient из БД и ее отправка клиенту
std::string history_messages(SOCKET client_socket, const std::string& username, const std::string& command) {
    // Формат: GET_HISTORY:recipient
    std::string recipient = command.substr(12);

    sqlite3_stmt* stmt;
    std::string query;

    // Для групп: извлечение из БД всех сообщений, адресованных этой группе
    // Для личных сообщений: получение из БД переписки между двумя пользователями
    query =
        "SELECT m.id, m.sender, m.recipient, "
        "CASE WHEN m.deleted = 1 THEN '[Сообщение удалено]' ELSE m.content END as content, "
        "strftime('%s', m.timestamp), m.is_group "
        "FROM messages m "
        "LEFT JOIN deleted_messages dm ON m.id = dm.message_id AND dm.username = ?1 "
        "WHERE "
        "("
        "   (m.is_group = 0 AND ((m.sender = ?1 AND m.recipient = ?2) OR (m.sender = ?2 AND m.recipient = ?1))) "
        "   OR "
        "   (m.is_group = 1 AND m.recipient = ?2)"
        ") "
        "AND m.deleted = 0 AND dm.username IS NULL "
        "ORDER BY m.timestamp ASC";

    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, recipient.c_str(), -1, SQLITE_STATIC);

    // Формирование истории для отправки клиенту
    std::string history = "HISTORY:";
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* sender = (const char*)sqlite3_column_text(stmt, 1);
        const char* recipient = (const char*)sqlite3_column_text(stmt, 2);
        const char* content = (const char*)sqlite3_column_text(stmt, 3);
        time_t timestamp = sqlite3_column_int64(stmt, 4);
        int is_group = sqlite3_column_int(stmt, 5);

        history += std::to_string(id) + ":" + sender + ":" + recipient + ":" +\
                   std::to_string(timestamp) + ":" + std::to_string(is_group) + ":" + content + "|";
    }
    sqlite3_finalize(stmt);

    // Удаление последнего разделителя
    if (!history.empty() && history.back() == '|') {
        history.pop_back();
    }
    send(client_socket, history.c_str(), history.size(), 0);

    return history;
}


// Извлечение из БД списка участников группового чата и его отправка клиенту
std::string list_members(SOCKET client_socket, const std::string& command) {
    // Формат: GET_GROUP_MEMBERS:groupname
    std::string groupname = command.substr(18);
    sqlite3_stmt* stmt;
    std::string query =
        "SELECT username FROM group_members gm "
        "JOIN groups g ON gm.group_id = g.id "
        "WHERE g.name = ?";

    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, groupname.c_str(), -1, SQLITE_STATIC);

    std::string list_members = "GROUP_MEMBERS:" + groupname + ":";
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* member = (const char*)sqlite3_column_text(stmt, 0);
        list_members += std::string(member) + ",";
    }
    sqlite3_finalize(stmt);

    // Удаление последней запятой, если есть участники
    if (list_members.back() == ',') {
        list_members.pop_back();
    }
    send(client_socket, list_members.c_str(), list_members.size(), 0);
    return list_members;
}


// Пересылка сообщения клиенту(ам)
void forwarding_message(const std::string& username, const std::string& command) {
    // Обработка сообщения
    std::string msg = command.substr(4);
    size_t sep1 = msg.find(':');
    size_t sep2 = msg.find(':', sep1 + 1);
    std::string recipient = msg.substr(0, sep1);
    time_t timestamp = std::stol(msg.substr(sep1 + 1, sep2 - sep1 - 1));
    std::string content = msg.substr(sep2 + 1);

    // Удаление решетки у recipient, если она есть, для записи в БД
    bool is_group = (recipient[0] == '#');
    if (is_group) {
        recipient = recipient.substr(1);
    }

    // Сохранение сообщения в БД
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "INSERT INTO messages (sender, recipient, content, is_group, deleted, timestamp) VALUES (?, ?, ?, ?, 0, datetime(?, 'unixepoch'))", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, recipient.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content.data(), content.size(), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, is_group ? 1 : 0); // Проверка на групповой чат
    sqlite3_bind_int64(stmt, 5, timestamp);
    sqlite3_step(stmt);
    int message_id = sqlite3_last_insert_rowid(db); // Получение ID вставленного сообщения
    sqlite3_finalize(stmt);

    // Пересылка сообщения получателю(ям)
    msg = "MSG:" + std::to_string(message_id) + ":" + username + ":" + msg;

    std::lock_guard<std::mutex> lock(clients_mutex);
    if (is_group) {
        //// Групповое сообщение
        // Получение списка участников группы
        sqlite3_stmt* members_stmt;
        sqlite3_prepare_v2(db,
            "SELECT username FROM group_members gm "
            "JOIN groups g ON gm.group_id = g.id "
            "WHERE g.name = ?", -1, &members_stmt, 0);
        sqlite3_bind_text(members_stmt, 1, recipient.c_str(), -1, SQLITE_STATIC);

        // Отправка сообщения всем участникам группы, кроме отправителя
        while (sqlite3_step(members_stmt) == SQLITE_ROW) {
            std::string member = (const char*)sqlite3_column_text(members_stmt, 0);
            if (member != username) {
                for (const auto& client : clients) {
                    if (client.username == member) {
                        send(client.socket, msg.c_str(), msg.size(), 0);
                        break;
                    }
                }
            }
        }
        sqlite3_finalize(members_stmt);
    }
    else {
        // Личное сообщение
        for (const auto& client : clients) {
            if (client.username == recipient && client.username != username) {
                send(client.socket, msg.c_str(), msg.size(), 0);
                break;
            }
        }
    }
}


// Обработчик удаления сообщения (изменения в БД и ответ клиентам, если удалено для всех)
void delete_message(const std::string& username, const std::string& command) {
    // Формат: DEL:message_id:for_everyone (for_everyone == 1 - для всех, 0 - только для себя)
    size_t sep = command.find(':', 4);
    int msg_id = std::stoi(command.substr(4, sep - 4));
    bool for_everyone = (command.substr(sep + 1) == "1");

    // Получение информации о сообщении перед удалением
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "SELECT sender, recipient, is_group FROM messages WHERE id = ?",
        -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, msg_id);

    std::string sender, recipient;
    bool is_group = false;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sender = (const char*)sqlite3_column_text(stmt, 0);
        recipient = (const char*)sqlite3_column_text(stmt, 1);
        is_group = sqlite3_column_int(stmt, 2) == 1;
    }
    sqlite3_finalize(stmt);

    // Обновление сообщения в БД
    if (for_everyone) {
        // Удаление для всех
        sqlite3_prepare_v2(db,
            "UPDATE messages SET deleted = 1, content = '[Сообщение удалено]' WHERE id = ?",
            -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, msg_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Рассылка уведомлений об удалении
        std::string notify_msg = "MSG_DELETED:" + std::to_string(msg_id);

        if (is_group) {
            // Для группового чата - получение всех участников группы
            sqlite3_prepare_v2(db,
                "SELECT username FROM group_members gm "
                "JOIN groups g ON gm.group_id = g.id "
                "WHERE g.name = ?", -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, recipient.c_str(), -1, SQLITE_STATIC);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string member = (const char*)sqlite3_column_text(stmt, 0);
                if (member != username) { // Не отправляю себе
                    for (const auto& client : clients) {
                        if (client.username == member) {
                            send(client.socket, notify_msg.c_str(), notify_msg.size(), 0);
                            break;
                        }
                    }
                }
            }
            sqlite3_finalize(stmt);
        }
        else {
            // Для личного чата - отправление собеседнику
            for (const auto& client : clients) {
                if (client.username == recipient) {
                    send(client.socket, notify_msg.c_str(), notify_msg.size(), 0);
                    break;
                }
            }
        }
    }
    else {
        // Удаление только для отправителя
        // Сохранение в отдельной таблице БД идентификатора удаленного сообщения и того, кто удалил у себя
        sqlite3_prepare_v2(db,
            "INSERT INTO deleted_messages (message_id, username) VALUES (?, ?)",
            -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, msg_id);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}


// Обработчик команды создания группового чата
void create_group(SOCKET client_socket, const std::string& username, const std::string& command) {
    if (command.starts_with("CREATE_GROUP:")) {
        size_t name_end = command.find(':', 13);
        if (name_end == std::string::npos) return;

        std::string group_name = command.substr(13, name_end - 13);
        std::string members_str = command.substr(name_end + 1);

        // Создание группы в базе данных
        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO groups (name, creator) VALUES (?, ?)";
        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, group_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            int group_id = sqlite3_last_insert_rowid(db);

            // Добавление создателя в группу
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db, "INSERT INTO group_members (group_id, username) VALUES (?, ?)", -1, &stmt, 0);
            sqlite3_bind_int(stmt, 1, sqlite3_last_insert_rowid(db));
            sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);

            // Добавление участников в группу
            std::vector<std::string> members;
            size_t pos = 0;
            while ((pos = members_str.find(',')) != std::string::npos) {
                members.push_back(members_str.substr(0, pos));
                members_str.erase(0, pos + 1);
            }
            if (!members_str.empty()) {
                members.push_back(members_str);
            }

            for (const auto& member : members) {
                sqlite3_prepare_v2(db, "INSERT INTO group_members (group_id, username) VALUES (?, ?)", -1, &stmt, 0);
                sqlite3_bind_int(stmt, 1, group_id);
                sqlite3_bind_text(stmt, 2, member.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                // Уведомление участника
                notify_user(member, "GROUP_MEMBER_ADDED:" + group_name + ":" + username);
            }

            // Уведомление создателя
            std::string responce = "GROUP_CREATED:" + group_name;
            send(client_socket, responce.c_str(), responce.size(), 0);

            // Обновление списка контактов у подключенных участников группы
            for (const auto& client : clients) {
                // Проверка, содержится ли имя пользователя в векторе members
                if (std::find(members.begin(), members.end(), client.username) != members.end()) {
                    list_contacts(client.socket, client.username);
                }
            }
        }
    }
}


// Обработчик добавления пользователя в групповой чат
void invite_to_group(SOCKET client_socket, const std::string& username, const std::string& command) {
    // Формат: INVITE_TO_GROUP:groupname:username
    size_t group_end = command.find(':', 16);
    std::string group_name = command.substr(16, group_end - 16);
    std::string invite_username = command.substr(group_end + 1);

    // Проверка, существует ли группа и является ли приглашающий её участником
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "SELECT 1 FROM group_members gm "
        "JOIN groups g ON gm.group_id = g.id "
        "WHERE g.name = ? AND gm.username = ?", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, group_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Проверка, не является ли пользователь уже участником
        sqlite3_finalize(stmt);
        sqlite3_prepare_v2(db,
            "SELECT 1 FROM group_members gm "
            "JOIN groups g ON gm.group_id = g.id "
            "WHERE g.name = ? AND gm.username = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, group_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, invite_username.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            // Добавление пользователя в группу
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db,
                "INSERT INTO group_members (group_id, username) "
                "SELECT g.id, ? FROM groups g WHERE g.name = ?", -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, invite_username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, group_name.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                notify_user(invite_username, "GROUP_MEMBER_ADDED:" + group_name + ":" + invite_username);

                // Обновление списка контактов у приглашенного пользователя
                for (const auto& client : clients) {
                    if (client.username == invite_username) {
                        std::string contacts_list = list_contacts(client.socket, client.username);
                        send(client.socket, contacts_list.c_str(), contacts_list.size(), 0);
                    }
                }
            }
            else {
                send(client_socket, "INVITE_RESULT:FAIL:Database error", 32, 0);
            }
        }
        else {
            send(client_socket, "INVITE_RESULT:FAIL:User already in group", 37, 0);
        }
    }
    else {
        send(client_socket, "INVITE_RESULT:FAIL:Not a group member or group doesn't exist", 55, 0);
    }
    sqlite3_finalize(stmt);
}


// Обработчик удаления пользователя из списка участников группового чата
void leave_group(SOCKET client_socket, const std::string& username, const std::string& command) {
    // Формат: LEAVE_GROUP:groupname
    std::string group_name = command.substr(12);

    // Удаление пользователя из группы
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "DELETE FROM group_members WHERE username = ? AND group_id IN "
        "(SELECT id FROM groups WHERE name = ?)", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, group_name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0) {
        // Уведомление пользователю о выходе из группового чата
        std::string notify_msg = "GROUP_MEMBER_LEFT:" + group_name;
        send(client_socket, notify_msg.c_str(), notify_msg.size(), 0);
    }
    else {
        send(client_socket, "LEAVE_FAILED:Not a group member or group doesn't exist", 52, 0);
    }
    sqlite3_finalize(stmt);
}

// Уведомление пользователя о его присоединении к групповому чату
void notify_user(const std::string& username, const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.username == username) {
            send(client.socket, message.c_str(), message.size(), 0);
            break;
        }
    }
}