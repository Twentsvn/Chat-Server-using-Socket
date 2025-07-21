# Chat Server with Groups and Private Messages
CS425: Computer Networks - Assignment 1

##Team Members 
- Ekansh Bajpai (220390)
- Pulkit Dhayal (220834)

## Features Implementation Status
### Implemented Features
- Basic Server Functionality
  * TCP-based server listening on port 12345
  * Multiple concurrent client connections using multithreading approach 
  * Connected clients list management (socket to username)
  
- User Authentication
  * Username/password validation from users.txt upon login
  * Authentication failure handling (Disconnects client who fail authentication with an error message)
- Messaging Features
  * Broadcast messages (/broadcast <message>) should send messages to all connected clients
  * Broadcast messages (/broadcast <message>) should send messages to all connected clients
  * Group messages (/group_msg <group> <message>) should allow sending messages to all group members
- Group Management
  * Group creation (/create_group <group>) should allow users to create new groups
  * Group joining (/join_group <group>) should let clients join existing groups
  * Group leaving (/leave_group <group>) should allow clients to exit a group they previously joined


## Design Decisions
### Connection Management
- I used multithreaded approach for my chat server as it is memory efficient as threads share same memory space while for every process, needs separate memory allocation. Moreover, threading enables us to process network requests without waiting for other tasks to complete.
    Threading also provides scalability to the code.
## File Description
- server_grp.cpp - Server side code
- client_grp.cpp - client side code
- users.txt - Contains the user-password list
- test.sh - Bash script to test the server
- Makefile - to build the processes
- test_users.txt - user list for stress testing
- Readme.md - Documenting the code
  
### Synchronization
Since the server is multithreaded, multiple clients interact concurrently, modifying shared resources. 
To prevent race conditions and data corruption, the following resources require synchronization:

Resource                | Data Structure                                                   | Need
----------------------- | ---------------------------------------------------------------- | -----------------------------------------
Connected Clients List  | std::unordered_map<int, std::string> clients;                    | Clients dynamically connect/disconnect.
User Authentication     | std::unordered_map<std::string, std::string> users;              | Read by multiple threads during authentication.
Group Memberships       | std::unordered_map<std::string, std::unordered_set<int>> groups; | Clients join, leave, and send messages.

## Mechanisms Used

Mechanism                  | Where It Is Used?        | Purpose
-------------------------- | ----------------------- | ----------------------------
std::mutex clients_mutex;  | Protects clients map    | Ensures safe addition/removal of clients.
std::mutex groups_mutex;   | Protects groups map     | Ensures safe group creation, joining, and messaging.
std::lock_guard<std::mutex>| Used in modifying maps  | Provides automatic locking and unlocking of shared resources.

## 3. Justification for Synchronization in Each Resource

 Connected Clients (`clients` map)
  - Clients dynamically connect, disconnect, and send messages, modifying the clients map.
  - Without synchronization: Two clients modifying clients at the same time could lead to inconsistent data.
  - Implementation:
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[client_socket] = username; // Safely add client

 Group Membership (`groups` map)
  - Clients create, join, leave, and send messages in groups, modifying groups.
  - Without synchronization: Two clients modifying a group simultaneously could cause data corruption or loss.
  - Implementation:
    std::lock_guard<std::mutex> lock(groups_mutex);
    groups[group_name].insert(client_socket); // Safely modify group

If mutexes were not used, the following problems could occur:

Issue                 | Cause                                             | Example Scenario
--------------------- | ----------------------------------------------- | -----------------------------------
Race Conditions      | Multiple threads modify the same resource.       | Two clients joining a group simultaneously could cause one to be ignored.
Data Corruption      | Partial updates due to unsynchronized access.    | A client’s username could be partially written if another thread modifies clients.
Unexpected Crashes   | Iterating over a structure while another modifies it. | Broadcasting a message while a client disconnects could cause an invalid memory access.


## Implementation Details
### Key Functions
1. loadUsers() - Reads each line form the users.txt, extracts the usernsme and password and stores them in the users map.
2. authenticateUser(const std::string& username, const std::string& password) - Checks if the username exists in the users map, compares the provided password with the stored password. Returns a boolean value.
3. broadcastMessage(int sender_socket, const std::string& message) - Iterates over all clients and sends message (Uses send() to send messages over TCP) to everone connected except the sender.
4. privateMessage(int sender_socket, const std::string& recipient, const std::string& message) - Searches for the recipient’s socket in clients. If the recipient is found, sends the message using send() else sends an error message back to the sender.
5. groupMessage(int sender_socket, const std::string& group_name, const std::string& message) - Sends a message to all members of a group. Gets the sender’s username, locks groups_mutex to safely access the group’s member list. If the group exists, sends a formatted message [Group group_name] username: message to all group members except the sender else sends an error message.
6. createGroup(int sender_socket, const std::string& group_name) - Creates a new chat group and adds the creator as the first member. If the group does not exist, creates a new group and adds sender_socket to it and sends a success message. If the group already exists, sends an error message.
7. joinGroup(int sender_socket, const std::string& group_name) - Adds a client to an existing group. Locks groups_mutex to safely modify groups. If the group exists, adds sender_socket to the group else sends an error message.
8. leaveGroup(int sender_socket, const std::string& group_name) - Removes a client from a group. Locks groups_mutex to safely modify groups. If the group exists, removes sender_socket from the group, sends confirmation to the sender and notifies other group members. If the group doesn’t exist, sends an error message.  

### Code Flow
          +---------------------------+
          |   Start Chat Server (main)|
          +---------------------------+
                     |
                     v
          +---------------------------+
          |  Load users from users.txt|
          |      (loadUsers())        |
          +---------------------------+
                     |
                     v
          +---------------------------+
          |  Create TCP Server Socket |
          |     Bind to Port 12345    |
          |  Listen for New Clients   |
          +---------------------------+
                     |
                     v
          +---------------------+
          |  Accept New Client  |
          |     Connection      |
          |  (Loop in start())  |
          +---------------------+
                     |
                     v
          +------------------------+
          |  Spawn New Thread for  |
          |         Client         |
          |   (handleClient())     |
          +------------------------+
                     |
                     v
    +----------------------------------------+
    |    Client Authentication (username/pwd)|
    |   authenticateUser(username, password) |
    +----------------------------------------+
        |                  |
        |                  v
        |       +----------------------------+
        |       | Authentication Failed      |
        |       | Send Error & Close Socket  |
        |       +----------------------------+
        |                  |
        v                  v
    +-----------------+    +--------------------------+
    | Authentication  |    | Add Client to Connected  |
    | Successful      |    | Clients Map (clients)    |
    +-----------------+    +--------------------------+
                              |
                              v
                  +----------------------------+
                  | Send "Welcome" Message to  |
                  | Client & Notify Others     |
                  +----------------------------+
                              |
                              v
                  +----------------------------+
                  | Wait for Client Commands   |
                  | (Loop in handleClient())   |
                  +----------------------------+
                              |
                              v
    +-------------------------------------------------------------+
    |   Message Handling Based on Command                         |
    +-------------------------------------------------------------+
    | 1. `/broadcast <message>` -> broadcastMessage()             |
    | 2. `/msg <user> <message>` -> privateMessage()              |
    | 3. `/group_msg <group> <message>` -> groupMessage()         |
    | 4. `/create_group <group>` -> createGroup()                 |
    | 5. `/join_group <group>` -> joinGroup()                     |
    | 6. `/leave_group <group>` -> leaveGroup()                   |
    +-------------------------------------------------------------+
                              |
                              v
                  +----------------------------+
                  | Client Disconnects or Quits|
                  +----------------------------+
                              |
                              v
                  +----------------------------+
                  | Remove Client from Clients |
                  | Remove from Groups         |
                  | Close Client Socket        |
                  +----------------------------+
                              |
                              v
                  +----------------------------+
                  | Server Continues Running   |
                  | Accepts More Clients       |
                  +----------------------------+

## Testing
### Correctness Testing
# Edge Cases and Expected Behavior

Edge Case                        | Expected Behavior                                              | Test
-------------------------------- | -------------------------------------------------------------- | -----------------------------------------------
Duplicate User Login             | Prevent same username from logging in twice.                   | Try logging in twice with the same user.
Long Messages                    | Messages should be truncated.                                  | Send a long message (more than N bytes).
Empty Messages                   | Empty messages should be ignored.                              | Send an empty /msg B "".
Group Message Without Joining    | User should not send messages to a group they are not part of. | Try /group_msg CS425 Hi without joining the group.
Client Disconnects Unexpectedly  | Server should disconnect the client                            | Forcefully close the client socket.


### Stress Testing
- We wrote a bash script to stress test the server.
- In order to rigorously assess the performance and reliability of the server under heavy load, this script facilitates an automated stress test by simulating numerous concurrent client interactions. Each client is represented by a unique set of user credentials, extracted from the test_users.txt file. For every user, the script launches a new Terminal window and executes the client binary, automatically providing the appropriate username and password. This process mimics actual user logins and interactions with the server, generating a high volume of simultaneous connections. By running these client instances in parallel, the script creates a traffic pattern that challenges the server’s ability to authenticate, process, and respond to requests efficiently. This stress test serves to uncover potential bottlenecks, memory leaks, or failures that could arise when handling large numbers of clients, such as server crashes, slowdowns, or data inconsistencies. The insights gained from this testing are crucial for identifying performance thresholds and optimizing the server to ensure it can effectively scale and handle real-world usage scenarios with minimal disruptions.
- The server was able to manage around 250 connections under default server settings, however on increasing the system resources limit for the process, we were able to achieve around 600 connections.
- "Connecting Reset by Peer" and "Broken Pipe" errors came up when we increases the density of connections.
   * If the server is overwhelmed, experiences a crash, or terminates a connection prematurely (due to server-side resource constraints, authentication failures, or an intentional server shutdown), you might encounter the “Connection reset by peer” error.
   * The “Broken pipe” error can occur if a client (started by the script) attempts to write data to the server after the server has closed or terminated the connection. This could happen if the server closes its end of the connection before the client finishes sending data, or if the server crashes or shuts down while the client is still active.
- The maximum message size that a client can send is determined by the buffer size allocated for receiving messages from clients. This buffer has a maximum capacity of 1024 bytes, which means the maximum message length a client can send (without fragmentation or chunking) is 1024 bytes.
- The system’s total memory (RAM) will be the limiting factor for the maximum number of groups as each group name is stored as a string, and the number of characters in the group name (maximum allowed by the system) directly influences memory usage.


## Challenges Faced
1. How does the server differentiate between text and command from the user?
   - User have to communicate with the server first in order to communicate with others he wishes to but how does the server know what user wants.   
   - Syntax was designed (actually given) to understand what the user want like          ```/broadcast Hey CS425!!``` 
     this string will be tokenize that is it will be divided into tokens of word and will be parsed accordingly like here /broadcast matches with our syntax to broadcast whaterver is after it, here "HEY CS425!!". Similialry other commands work.    
2. How do we know where our server is crashing or behaving wrongly ?
    - It is well known that bad code will be presenent and we need to debug it with time to time but how to figure out where is our program crashing or running badly.
    - Error handling or Exceptional handling come handy while doing the debugging since we can now easily pin point where the leakage is and what is causing the bad behaviour but rest is onto us. 

## Team Contribution
### Member 1 - Ekansh Bajpai
- Helped with the basic code structure and documentation.

### Member 2 - Pulkit Dhayal 
- Helped with the debugging and stress testing of the server side code.

## Declaration
We declare that this assignment is our own work and that we have not indulged in plagiarism of any kind. All external sources have been properly cited.
