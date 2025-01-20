
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

class ChatServer {
private:
    int server_socket;
    std::mutex clients_mutex;
    std::mutex groups_mutex;
    std::unordered_map<int, std::string> clients; // socket -> username
    std::unordered_map<std::string, std::string> users; // username -> password
    std::unordered_map<std::string, std::unordered_set<int> > groups; // group -> client sockets

    void loadUsers() {

        std::ifstream file("/Users/ekanshbajpai/Desktop/cs425-2025/Homeworks/A1/users.txt");
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file");
        } 
        std::string line;
        std::string username;
        std::string password;
        while (std::getline(file, line)) {
            for (int i=0 ; i<(int)line.length() ; ++i){
                  if(line[i]==':'){
                    username = line.substr(0, i);
                    password = line.substr(i+1);
                    std::cout << "Username: " << username << std::endl;
                    users[username] = password;
                  }  
            }   
        }
    }

    bool authenticateUser(const std::string& username, const std::string& password) {
        std::cout << "Authenticating user: " << username << std::endl;
        std::cout << "Password: " << password << std::endl;
        if(users[username]==password){
            return true;
        }
        return false;
        
        //auto it = users.find(username);
        //return (it != users.end() && it->second == password);
    }

    void broadcastMessage(int sender_socket, const std::string& message) {
        std::string sender_username;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            sender_username = clients[sender_socket];
        }
        
        std::string formatted_message = "[" + sender_username + "]: " + message + "\n";
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& client : clients) {
            if (client.first != sender_socket) {
                send(client.first, formatted_message.c_str(), formatted_message.length(), 0);
            }
        }
    }

    void privateMessage(int sender_socket, const std::string& recipient, const std::string& message) {
        std::string sender_username;
        int recipient_socket = -1;
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            sender_username = clients[sender_socket];
            for (const auto& client : clients) {
                if (client.second == recipient) {
                    recipient_socket = client.first;
                    break;
                }
            }
        }

        if (recipient_socket != -1) {
            std::string formatted_message = "[" + sender_username + "]: " + message + "\n";
            send(recipient_socket, formatted_message.c_str(), formatted_message.length(), 0);
        } else {
            std::string error_message = "Error: User " + recipient + " not found.\n";
            send(sender_socket, error_message.c_str(), error_message.length(), 0);
        }
    }

    void createGroup(int sender_socket, const std::string& group_name) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) == groups.end()) {
            groups[group_name].insert(sender_socket);
            std::string success_message = "Group " + group_name + " created.\n";
            send(sender_socket, success_message.c_str(), success_message.length(), 0);
        } else {
            std::string error_message = "Error: Group " + group_name + " already exists.\n";
            send(sender_socket, error_message.c_str(), error_message.length(), 0);
        }
    }

    void joinGroup(int sender_socket, const std::string& group_name) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) != groups.end()) {
            groups[group_name].insert(sender_socket);
            std::string success_message = "You joined the group " + group_name + ".\n";
            send(sender_socket, success_message.c_str(), success_message.length(), 0);
        } else {
            std::string error_message = "Error: Group " + group_name + " doesn't exist.\n";
            send(sender_socket, error_message.c_str(), error_message.length(), 0);
        }
    }

    void leaveGroup(int sender_socket, const std::string& group_name) {
        std::lock_guard<std::mutex> lock(groups_mutex);
        if (groups.find(group_name) != groups.end()) {
            groups[group_name].erase(sender_socket);
            std::string success_message = "You left the group " + group_name + ".\n";
            send(sender_socket, success_message.c_str(), success_message.length(), 0);
        } else {
            std::string error_message = "Error: Group " + group_name + " doesn't exist.\n";
            send(sender_socket, error_message.c_str(), error_message.length(), 0);
        }
    }

    void groupMessage(int sender_socket, const std::string& group_name, const std::string& message) {
        std::string sender_username;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            sender_username = clients[sender_socket];
        }

        std::unordered_set<int> group_members;
        {
            std::lock_guard<std::mutex> lock(groups_mutex);
            if (groups.find(group_name) != groups.end()) {
                group_members = groups[group_name];
            }
        }

        if (!group_members.empty()) {
            std::string formatted_message = "[Group " + group_name + "] " + sender_username + ": " + message + "\n";
            for (int member_socket : group_members) {
                if (member_socket != sender_socket) {
                    send(member_socket, formatted_message.c_str(), formatted_message.length(), 0);
                }
            }
        } else {
            std::string error_message = "Error: Group " + group_name + " doesn't exist or is empty.\n";
            send(sender_socket, error_message.c_str(), error_message.length(), 0);
        }
    }

    void handleClient(int client_socket) {
        char buffer[1024];
        
        // Authentication
        send(client_socket, "Enter username: ", 15, 0);
        ssize_t username_len = recv(client_socket, buffer, sizeof(buffer), 0);
        std::string username(buffer, username_len);  // Remove newline
        
        send(client_socket, "Enter password: ", 15, 0);
        ssize_t password_len = recv(client_socket, buffer, sizeof(buffer), 0);
        std::string password(buffer, password_len);  // Remove newline

        bool a = authenticateUser(username, password);
        if (!a) {
            send(client_socket, "Authentication failed.\n", 21, 0);
            close(client_socket);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[client_socket] = username;
        }

        send(client_socket, "Welcome to the chat server!\n", 27, 0);

        // Broadcast join message
        broadcastMessage(client_socket, "has joined the chat");

        while (true) {
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;

            std::string message(buffer, bytes_received);  // Remove newline
            
            if (message.rfind("/msg ", 0) == 0) {
                size_t first_space = message.find(' ');
                size_t second_space = message.find(' ', first_space + 1);
                if (second_space != std::string::npos) {
                    std::string recipient = message.substr(first_space + 1, second_space - first_space-1);
                    std::string msg = message.substr(second_space + 1);
                    privateMessage(client_socket, recipient, msg);
                }
            }
            else if (message.rfind("/broadcast ", 0) == 0) {
                broadcastMessage(client_socket, message.substr(11));
            }
            else if (message.rfind("/create_group ", 0) == 0) {
                createGroup(client_socket, message.substr(14));
            }
            else if (message.rfind("/join_group ", 0) == 0) {
                joinGroup(client_socket, message.substr(12));
            }
            else if (message.rfind("/leave_group ", 0) == 0) {
                leaveGroup(client_socket, message.substr(13));
            }
            else if (message.rfind("/group_msg ", 0) == 0) {
                size_t first_space = message.find(' ');
                size_t second_space = message.find(' ', first_space + 1);
                if (second_space != std::string::npos) {
                    std::string group_name = message.substr(first_space + 1, second_space - first_space - 1);
                    std::string msg = message.substr(second_space + 1);
                    groupMessage(client_socket, group_name, msg);
                }
            }
        }

        // Clean up
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(client_socket);
        }
        {
            std::lock_guard<std::mutex> lock(groups_mutex);
            for (auto& group : groups) {
                group.second.erase(client_socket);
            }
        }
        close(client_socket);
    }

public:
    ChatServer(int port) {
        loadUsers();
        
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            throw std::runtime_error("Failed to create socket");
        }

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(server_socket, 10) < 0) {
            throw std::runtime_error("Failed to listen on socket");
        }
    }

    void start() {
        std::cout << "Server started. Listening for connections...\n";
        
        while (true) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket < 0) {
                std::cerr << "Failed to accept connection\n";
                continue;
            }

            std::thread client_thread([this, client_socket]() {
                this->handleClient(client_socket);
            });
            client_thread.detach();
        }
    }

    ~ChatServer() {
        close(server_socket);
    }
};

int main() {
    try {
        ChatServer server(12345);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}