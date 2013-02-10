#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define USER_NAME_MAX_LENGTH 10
#define RESPONSE_LENGTH 50
#define MAX_SERVERS_NUMBER 15
#define MAX_USERS_NUMBER 20 //na jednym serwerze
#define ROOM_NAME_MAX_LENGTH 10
#define MAX_MSG_LENGTH 256
#define SHM_SERVER_IDS 15
#define SHM_USER_SERVER 20
#define SHM_ROOM_SERVER 25
#define SEM_SERVER_IDS 35
#define SEM_USER_SERVER 36
#define SEM_ROOM_SERVER 37
#define SEM_LOGFILE 38

// powiązania loginów z id serwerów w postaci tablicy struktur
typedef struct {
  char user_name[USER_NAME_MAX_LENGTH];
  int server_id;
} USER_SERVER;

// powiązanie kanałów z id serwerów w postaci tablicy struktur
typedef struct {
  char room_name[ROOM_NAME_MAX_LENGTH];
  int server_id;
} ROOM_SERVER;

enum MSG_TYPE {LOGIN=1, RESPONSE, LOGOUT, REQUEST, MESSAGE, ROOM, SERVER2SERVER, USERS_LIST_TYPE, ROOMS_LIST_TYPE, ROOM_USERS_LIST_TYPE};
typedef struct {
  long type;
  char username[USER_NAME_MAX_LENGTH];
  int ipc_num; //nr kolejki na której będzie nasłuchiwał klient
} MSG_LOGIN;

enum RESPONSE_TYPE {LOGIN_SUCCESS, LOGIN_FAILED, LOGOUT_SUCCESS, LOGOUT_FAILED, MSG_SEND, MSG_NOT_SEND, ENTERED_ROOM_SUCCESS,
  ENTERED_ROOM_FAILED, CHANGE_ROOM_SUCCESS, CHANGE_ROOM_FAILED, LEAVE_ROOM_SUCCESS, LEAVE_ROOM_FAILED, PING};
typedef struct {
  long type;
  int response_type;
  char content[RESPONSE_LENGTH]; //można tu wpisad jakiś komunikat wyświetlany klientowi
} MSG_RESPONSE;

enum REQUEST_TYPE{R_USERS_LIST, R_ROOMS_LIST, R_ROOM_USERS_LIST, PONG};
typedef struct {
  long type;
  int request_type;
  char user_name[USER_NAME_MAX_LENGTH];
} MSG_REQUEST;

typedef struct {
  long type;
  char users[MAX_SERVERS_NUMBER * MAX_USERS_NUMBER][USER_NAME_MAX_LENGTH];
} MSG_USERS_LIST;

enum CHAT_MESSAGE_TYPE {PUBLIC, PRIVATE};
typedef struct {
  long type;
  int msg_type;
  char send_time[6]; //czas wysłania hh:mm
  char sender[USER_NAME_MAX_LENGTH]; //nazwa nadawcy
  char receiver[USER_NAME_MAX_LENGTH]; //nazwa odbiorcy
  char message[MAX_MSG_LENGTH]; //treść wiadomości
} MSG_CHAT_MESSAGE;

enum ROOM_OPERATION_TYPE {ENTER_ROOM, LEAVE_ROOM, CHANGE_ROOM};
typedef struct {
  long type;
  int operation_type;
  char user_name[USER_NAME_MAX_LENGTH];
  char room_name[ROOM_NAME_MAX_LENGTH];
} MSG_ROOM;

typedef struct {
  long type;
  int server_ipc_num; //nr kolejki serwera, który wysyła tę strukturę
} MSG_SERVER2SERVER;

typedef struct {
  char Username[USER_NAME_MAX_LENGTH];
  int GetQueueID;
  int Alive;
} TUser;

void Quit();
void Menu();
void PrintMenu();
int AmILastServer();
void Register();
void PrepareUsersArray();
void GetLogin();
void PrintServers();
void CreateGetQueue();
void PrintUsers();
void PrintAllUsers();
void Unregister();
void PrepareUSSM();
void Get();
void GetLogout();
void PrepareSemaphores();
void P(int);
void V(int);
void GetRequest();
int PrepareServerIDSM();
void GetMessage();
int UserQueueID();
int UsernameTaken(char*);
int WhereToLogin();
void RegisterUser(char*, int);
void SendLoggedIn(int);
void SendNotLoggedIn(int);
void LogoutUser(char*);
void SendLoggedOut(int);
void SendUsersList(int);
void SendMsgToUser(MSG_CHAT_MESSAGE);
void SendMsgToServer(int, MSG_CHAT_MESSAGE);
void SendMsgSent(int);
void SendMsgNotSent(int);
void SendCheckServer(int);
void UpdateAlive(char*);
void SendHeartBeat();
void RemoveDeadClients();
void ClearAlive();

int* server_ids;
int server_ids_SemID;
USER_SERVER* user_server;
int user_server_SemID;

int GetQueueID;
int MenuPID;
TUser Users[MAX_USERS_NUMBER];
int Checking = 0;

// ---------------------------- MENU i main -------------------------------

int main() {
  int Time = 0;
  CreateGetQueue();
  PrepareSemaphores();
  Register();
  MenuPID = fork();
  if (MenuPID) { while(1) Menu(); }
  else {
    signal(30, PrintAllUsers);
    while(1) {
      Get();
      if (Time % 5000 == 0) {
        RemoveDeadClients();
        ClearAlive();
        Time = 0;
      } else
        if (Time % 1000 == 0) SendHeartBeat();
      Time++;
    }
  }
  return 0;
}

void Menu() {
  int Navigate;
  PrintMenu();
  scanf("%d", &Navigate);
    switch (Navigate) {
      case 2:
        kill(MenuPID, 30); // PrintUsers
        break;
      case 1:
        PrintServers();
        break;
      case 0:
        Quit();
    }
}

void PrintMenu() {
  printf("2 - Pokaz uzytkownikow\n");
  printf("1 - Pokaz ID serwerow\n");
  printf("0 - Wyjscie\n");
}

// ------------------------------ GET ----------------------------------------------

void Get() {
  // printf(".\n");
  GetLogin();
  GetLogout();
  GetRequest();
  GetMessage();
  GetCheckServer(0);
  sleep(5);
}

void GetLogin() {
  MSG_LOGIN msg_login;
  int SthReceived;
  SthReceived = msgrcv(GetQueueID, &msg_login, sizeof(MSG_LOGIN) - sizeof(long), LOGIN, IPC_NOWAIT);
  if (SthReceived > 0) {
    if ((UsernameTaken(msg_login.username) == 0) && (WhereToLogin() > -1)) {
      RegisterUser(msg_login.username, msg_login.ipc_num);
      SendLoggedIn(msg_login.ipc_num);
    } else {
      SendNotLoggedIn(msg_login.ipc_num);
    }
  }
}

void GetLogout() {
  MSG_LOGIN msg_login;
  int ClientQueueID, SthReceived;
  SthReceived = msgrcv(GetQueueID, &msg_login, sizeof(MSG_LOGIN) - sizeof(long), LOGOUT, IPC_NOWAIT);
  if (SthReceived > 0) {
    ClientQueueID = UserQueueID(msg_login.username);
    LogoutUser(msg_login.username);
    SendLoggedOut(ClientQueueID);
  }
}

void GetRequest() {
  MSG_REQUEST msg_request;
  int SthReceived = msgrcv(GetQueueID, &msg_request, sizeof(MSG_REQUEST) - sizeof(long), REQUEST, IPC_NOWAIT);
  if (SthReceived > 0) {
    if (msg_request.request_type == USERS_LIST_TYPE) SendUsersList(UserQueueID(msg_request.user_name));
    if (msg_request.request_type == PONG) UpdateAlive(msg_request.user_name);
  }
}

void UpdateAlive(char username[]) {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) {
    if (strcmp(Users[i].Username, username) == 0) Users[i].Alive = 1;
  }
}

void GetMessage() {
  int i, Sent = 0, SthSent;
  MSG_CHAT_MESSAGE msg_chat_message;
  int SthReceived = msgrcv(GetQueueID, &msg_chat_message, sizeof(MSG_CHAT_MESSAGE) - sizeof(long), MESSAGE, IPC_NOWAIT);
  if (SthReceived > 0) {
    if (msg_chat_message.msg_type == PRIVATE) {
      P(user_server_SemID);
      for (i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++) {
        if (strcmp(user_server[i].user_name, msg_chat_message.receiver) == 0) {
          if (user_server[i].server_id == GetQueueID) {
            SendMsgToUser(msg_chat_message);
            Sent = 1;
          } else {
            Checking = 1;
            SendCheckServer(user_server[i].server_id);
            if (GetCheckServer(1)) {
              SendMsgToServer(user_server[i].server_id, msg_chat_message);
              Sent = 1;
            }
          }
        }
      }
      V(user_server_SemID);
    } else { // if public
      P(user_server_SemID);
      for (i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++) {
        if ((user_server[i].server_id != -1) && (strcmp(user_server[i].user_name, msg_chat_message.sender) != 0)) {
          strcpy(msg_chat_message.receiver, user_server[i].user_name);
          msg_chat_message.msg_type = PRIVATE;
          if (user_server[i].server_id == GetQueueID) {
            SendMsgToUser(msg_chat_message);
            Sent = 1;
          } else {
            Checking = 1;
            SendCheckServer(user_server[i].server_id);
            V(user_server_SemID);
            if (GetCheckServer(1)) {
              SendMsgToServer(user_server[i].server_id, msg_chat_message);
              Sent = 1;
            }
            P(user_server_SemID);
          }
        }
      }
      V(user_server_SemID);
    }
    if (Sent)
      if (UserQueueID(msg_chat_message.sender) != -1)
        SendMsgSent(UserQueueID(msg_chat_message.sender));
    else {
      if (UserQueueID(msg_chat_message.sender) != -1)
        SendMsgNotSent(UserQueueID(msg_chat_message.sender));
    }
  }
}

int GetCheckServer(int Force) {
  int CheckingServerID;
  int SthReceived;
  MSG_SERVER2SERVER msg_server2server;
  if (!Force) {
    SthReceived = msgrcv(GetQueueID, &msg_server2server, sizeof(MSG_SERVER2SERVER) - sizeof(long), SERVER2SERVER, IPC_NOWAIT);
  }
  else {
    SthReceived = msgrcv(GetQueueID, &msg_server2server, sizeof(MSG_SERVER2SERVER) - sizeof(long), SERVER2SERVER, 0);
  }
  if (SthReceived > 0) {
    if (!Checking) {
      CheckingServerID = msg_server2server.server_ipc_num;
      msg_server2server.server_ipc_num = GetQueueID;
      SendCheckServer(CheckingServerID);
    } else { Checking = 0; return 1; }
  } else {
    if (Checking) { Checking = 0; return 0; }
  }
}

// --------------------------------- SEND ----------------------------------------------------

void SendUsersList(int UserQueueID) {
  int i;
  MSG_USERS_LIST msg_users_list;
    msg_users_list.type = USERS_LIST_TYPE;
    for (i = 0; i < (MAX_USERS_NUMBER * MAX_SERVERS_NUMBER); i++)
      strcpy(msg_users_list.users[i], "");
    P(user_server_SemID);
    for (i = 0; i < MAX_USERS_NUMBER * MAX_SERVERS_NUMBER; i++) {
      if (user_server[i].server_id != -1) {
        strcpy(msg_users_list.users[i], user_server[i].user_name);
      }
    }
    V(user_server_SemID);
    msgsnd(UserQueueID, &msg_users_list, sizeof(MSG_USERS_LIST) - sizeof(long), IPC_NOWAIT);
}

void SendMsgToUser(MSG_CHAT_MESSAGE msg_chat_message) {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) {
    if (strcmp(Users[i].Username, msg_chat_message.receiver) == 0) {
      msgsnd(Users[i].GetQueueID, &msg_chat_message, sizeof(MSG_CHAT_MESSAGE) - sizeof(long), 0);
    }
  }
}

void SendMsgToServer(int ServerID, MSG_CHAT_MESSAGE msg_chat_message) {
  msgsnd(ServerID, &msg_chat_message, sizeof(MSG_CHAT_MESSAGE) - sizeof(long), 0);
}

void SendCheckServer(int ipc_num) {
  MSG_SERVER2SERVER msg_server2server;
    msg_server2server.type = SERVER2SERVER;
    msg_server2server.server_ipc_num = GetQueueID;
  msgsnd(ipc_num, &msg_server2server, sizeof(MSG_SERVER2SERVER) - sizeof(long), 0);
}

void SendHeartBeat() {
  int i;
  MSG_RESPONSE msg_response;
    msg_response.type = RESPONSE;
    msg_response.response_type = PING;
    strcpy(msg_response.content, "ping");
  for(i = 0; i < MAX_USERS_NUMBER; i++) {
    msgsnd(Users[i].GetQueueID, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), 0);
  }
}

// ---- responses:

void SendLoggedIn(int ipc_num) {
  MSG_RESPONSE msg_response;
    msg_response.type = RESPONSE;
    msg_response.response_type = LOGIN_SUCCESS;
    strcpy(msg_response.content, "Zalogowano.\n");
  msgsnd(ipc_num, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), 0);
}

void SendNotLoggedIn(int ipc_num) {
  MSG_RESPONSE msg_response;
    msg_response.type = RESPONSE;
    msg_response.response_type = LOGIN_FAILED;
    strcpy(msg_response.content, "Username taken or no space on server.\n");
  msgsnd(ipc_num, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), 0);
}

void SendLoggedOut(int ipc_num) {
  MSG_RESPONSE msg_response;
    msg_response.type = RESPONSE;
    msg_response.response_type = LOGOUT_SUCCESS;
    strcpy(msg_response.content, "Wylogowano.\n");
  msgsnd(ipc_num, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), 0);
}

void SendMsgSent(int UserQueueID) {
  MSG_RESPONSE msg_response;
    msg_response.type = RESPONSE;
    msg_response.response_type = MSG_SEND;
    strcpy(msg_response.content, "Message sent.\n");
  msgsnd(UserQueueID, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), 0);
}

void SendMsgNotSent(int UserQueueID) {
  MSG_RESPONSE msg_response;
    msg_response.type = RESPONSE;
    msg_response.response_type = MSG_NOT_SEND;
  msgsnd(UserQueueID, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), 0);
}

// ------------------------- BEFORE ----------------------------------

void CreateGetQueue() {
  GetQueueID = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
}

void Register() {
  int Success = PrepareServerIDSM();
  if (!Success) {
    P(server_ids_SemID);
    shmdt(server_ids);
    V(server_ids_SemID);
    printf("Nie moge zarejestrowac serwera - brak miejsca.\n");
    exit(0);
  } else {
    PrepareUSSM();
    PrepareUsersArray();
    printf("Zarejestrowano serwer @%d.\n", GetQueueID);
  }
}

int PrepareServerIDSM() {
  int i;
  P(server_ids_SemID);
  int ShMID = shmget(SHM_SERVER_IDS, 60, IPC_EXCL | IPC_CREAT | 0777);
  if (ShMID < 0) { // tablica już istnieje w pamięci
    ShMID = shmget(SHM_SERVER_IDS, 0, 0);
    server_ids = (int*) shmat(ShMID, NULL, 0);
  } else { // tablica zostanie utworzona
    server_ids = (int*) shmat(ShMID, NULL, 0);
    for (i = 0; i < MAX_SERVERS_NUMBER; i++) server_ids[i] = -1;
  }
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) {
    if (server_ids[i] == -1) {
      server_ids[i] = GetQueueID;
      V(server_ids_SemID);
      return 1;
    }
  }
  V(server_ids_SemID);
  return 0;
}

void PrepareUSSM() {
  int i;
  P(user_server_SemID);
  int ShMID = shmget(SHM_USER_SERVER, 14 * MAX_SERVERS_NUMBER * MAX_USERS_NUMBER, IPC_EXCL | IPC_CREAT | 0777);
  if (ShMID < 0) { // tablica już istnieje w pamięci
    ShMID = shmget(SHM_USER_SERVER, 0, 0);
    user_server = (USER_SERVER*) shmat(ShMID, NULL, 0);
  } else { // tablica zostanie utworzona
    user_server = (USER_SERVER*) shmat(ShMID, NULL, 0);
    for (i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++) {
      strcpy(user_server[i].user_name, "");
      user_server[i].server_id = -1;
    }
  }
  V(user_server_SemID);
}

void PrepareUsersArray() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) { strcpy(Users[i].Username, ""); Users[i].Alive = 0; }
}

void PrepareSemaphores() {
  server_ids_SemID = semget(SEM_SERVER_IDS, 1, IPC_EXCL | IPC_CREAT | 0777);
  if (server_ids_SemID < 0) server_ids_SemID = semget(SEM_SERVER_IDS, 1, 0); // sem juz istnieje
  else semctl(server_ids_SemID, 0, SETVAL, 1); // semafor zostanie utworzony
  user_server_SemID = semget(SEM_USER_SERVER, 1, IPC_EXCL | IPC_CREAT | 0777);
  if (user_server_SemID < 0) user_server_SemID = semget(SEM_USER_SERVER, 1, 0); // sem juz istnieje
  else semctl(user_server_SemID, 0, SETVAL, 1); // semafor zostanie utworzony
}

// ------------------------- AFTER -----------------------------------

void Quit() {
  Unregister();
  exit(0);
}

void Unregister() {
  int i;
  int ShMID;
  kill(MenuPID, 9);
  P(server_ids_SemID);
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] == GetQueueID) { server_ids[i] = -1; break; }
  shmdt(server_ids);
  V(server_ids_SemID);
  P(user_server_SemID);
  shmdt(user_server);
  V(user_server_SemID);
  msgctl(GetQueueID, IPC_RMID, NULL);
  if (AmILastServer) {
    P(server_ids_SemID);
    ShMID = shmget(SHM_SERVER_IDS, 0, 0);
    shmctl(ShMID, IPC_RMID, 0);
    V(server_ids_SemID);
    P(user_server_SemID);
    ShMID = shmget(SHM_USER_SERVER, 0, 0);
    shmctl(ShMID, IPC_RMID, 0);
    V(user_server_SemID);
    semctl(user_server_SemID, IPC_RMID, NULL);
    semctl(server_ids_SemID, IPC_RMID, NULL);
  }
}

// ------------------------- MAINTAIN --------------------------------

void RegisterUser(char name[], int ipc_num) {
  int i;
  P(user_server_SemID);
  for (i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++) {
    if (!strcmp(user_server[i].user_name, "")) { // 0 if equal
      user_server[i].server_id = GetQueueID;
        strcpy(user_server[i].user_name, name);
      V(user_server_SemID);
      Users[WhereToLogin()].GetQueueID = ipc_num;
        strcpy(Users[WhereToLogin()].Username, name);
        Users[WhereToLogin()].Alive = 1;
      break;
    }
  }
}

void LogoutUser(char name[]) {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++)
    if (!strcmp(Users[i].Username, name)) {
      strcpy(Users[i].Username, "");
      Users[i].GetQueueID = -1;
      Users[i].Alive = 0;
      break;
    }
  P(user_server_SemID);
  for (i = 0; i < MAX_USERS_NUMBER * MAX_SERVERS_NUMBER; i++)
    if (!strcmp(user_server[i].user_name, name)) {
      strcpy(user_server[i].user_name, "");
      user_server[i].server_id = -1;
    }
  V(user_server_SemID);
}

void PrintUsers() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) {
    if (strcmp(Users[i].Username, "") == 0) { // strings egals -> return 0
      printf("User #%d: %s listening at: %d (QueueID).\n", i, Users[i].Username, Users[i].GetQueueID);
    }
  }
}

void PrintAllUsers() {
  int i;
  P(user_server_SemID);
  for (i = 0; i < MAX_USERS_NUMBER * MAX_SERVERS_NUMBER; i++) {
    if (user_server[i].server_id != -1) {
      printf("User #%d: %s @ %d (serwer ID).\n", i, user_server[i].user_name, user_server[i].server_id);
    }
  }
  V(user_server_SemID);
}

void PrintServers() {
  int i;
  P(server_ids_SemID);
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != -1) printf("Serwer #%d: %d\n", i, server_ids[i]);
    V(server_ids_SemID);
}

void RemoveDeadClients() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) {
    if (Users[i].Alive == 0) { SendLoggedOut(Users[i].GetQueueID); LogoutUser(Users[i].Username); }
  }
}

void ClearAlive() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) Users[i].Alive = 0;
}

// ------------------------- HELPERS -------------------------------------

void P(int SemID) {
  struct sembuf sem_buf;
    sem_buf.sem_num = 0;
    sem_buf.sem_op = -1;
    sem_buf.sem_flg = 0;
  semop(SemID, &sem_buf, 1);
}

void V(int SemID) {
  struct sembuf sem_buf;
    sem_buf.sem_num = 0;
    sem_buf.sem_op = 1;
    sem_buf.sem_flg = 0;
  semop(SemID, &sem_buf, 1);
}

int AmILastServer() {
  int i;
  int NumberOfServers = 0;
  P(server_ids_SemID);
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != -1) NumberOfServers++;
  V(server_ids_SemID);
  if (NumberOfServers == 1) return 1;
  else return 0;
}

int UserQueueID(char name[]) {
  int i;
  for(i = 0; i < MAX_USERS_NUMBER; i++) if(!strcmp(name, Users[i].Username)) return Users[i].GetQueueID; // if equal
  return -1;
}

int WhereToLogin() {
  int i, WhereToLogin = -1;
  for (i = 0; i < MAX_USERS_NUMBER; i++)
    if (!strcmp(Users[i].Username, "")) { WhereToLogin = i; break; } // returns 0 if equal
  return WhereToLogin;
}

int UsernameTaken(char name[]) {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER * MAX_SERVERS_NUMBER; i++)
    if (!strcmp(user_server[i].user_name, name)) return 1;
  return 0;
}
