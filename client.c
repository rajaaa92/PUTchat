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

#define USER_NAME_MAX_LENGTH 10
#define RESPONSE_LENGTH 50
#define MAX_SERVERS_NUMBER 15
#define MAX_USERS_NUMBER 20
#define ROOM_NAME_MAX_LENGTH 10 // ??
#define MAX_MSG_LENGTH 256
#define SHM_SERVER_IDS 15
#define SEM_SERVER_IDS 35
#define SEM_LOGFILE 38

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
  int ipc_num;
} MSG_LOGIN;

enum RESPONSE_TYPE {LOGIN_SUCCESS, LOGIN_FAILED, LOGOUT_SUCCESS, LOGOUT_FAILED, MSG_SEND, MSG_NOT_SEND, ENTERED_ROOM_SUCCESS,
  ENTERED_ROOM_FAILED, CHANGE_ROOM_SUCCESS, CHANGE_ROOM_FAILED, LEAVE_ROOM_SUCCESS, LEAVE_ROOM_FAILED, PING};
typedef struct {
  long type;
  int response_type;
  char content[RESPONSE_LENGTH];
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
  int server_ipc_num;
} MSG_SERVER2SERVER;

void Quit();
void Menu();
void Get();
void GetResponse();
void PrintMenu();
void CreateGetQueue();
void Register();
void Logout();
void PrepareSemaphores();
void P(int);
void V(int);
void SendPrintUsers();
void GetUsersList();

int* server_ids;
int MenuPID;
int GetQueueID;
int server_ids_SemID;
char MyUsername[USER_NAME_MAX_LENGTH];
int MyServerNr;
int Registered = 0;

// ------------------------------------------------------------------------

int main() {
  CreateGetQueue();
  printf("moje getqueue id: %d\n", GetQueueID);
  printf("msg type to: %d", USERS_LIST_TYPE);
  if (MenuPID = fork()) { while(1) Menu(); }
  else { while(1) Get(); }
  return 0;
}

void PrepareSemaphores() {
  server_ids_SemID = semget(SEM_SERVER_IDS, 1, IPC_EXCL | IPC_CREAT | 0777);
  if (server_ids_SemID < 0) server_ids_SemID = semget(SEM_SERVER_IDS, 1, 0); // sem juz istnieje
  else semctl(server_ids_SemID, 0, SETVAL, 1); // semafor zostanie utworzony
}

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

void Menu() {
  int Navigate;
  PrintMenu();
  scanf("%d", &Navigate);
  switch (Navigate) {
    case 2:
      SendPrintUsers();
      break;
    case 1:
      Register();
      break;
    case 0:
      if (Registered) Logout(); else Quit();
  }
}

void PrintMenu() {
  printf("2 - Wyswietl uzytkownikow\n");
  printf("1 - Rejestracja uzytkownika\n");
  printf("0 - Wyjscie\n");
}

void SendPrintUsers() {
  MSG_REQUEST msg_request;
    msg_request.type = REQUEST;
    msg_request.request_type = USERS_LIST_TYPE;
    printf("kopiuje msg_request.user_name << MyUserame --- %s << %s\n", msg_request.user_name, MyUsername);
    strcpy(msg_request.user_name, MyUsername);
    printf("teraz msgreq ma username: %s\n", msg_request.user_name);
  P(server_ids_SemID);
  printf("wysylam zaraz zapytanie o liste ludzi\n");
  msgsnd(server_ids[MyServerNr], &msg_request, sizeof(MSG_REQUEST) - sizeof(long), 0);
  printf("wyslalem zapytanie o liste ludzi\n");
  V(server_ids_SemID);
}

void Get() {
  // printf(".");
  GetResponse();
  GetUsersList();
  sleep(5);
}

void GetResponse() {
  MSG_RESPONSE msg_response;
  int i;
  int SthReceived;
  SthReceived = msgrcv(GetQueueID, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), RESPONSE, IPC_NOWAIT);
  if (SthReceived > 0) {
    printf("Odbieram: %s\n", msg_response.content);
    if (msg_response.response_type == LOGOUT_SUCCESS) Quit();
    if (msg_response.response_type == LOGIN_SUCCESS) Registered = 1;
  }
}

void GetUsersList() {
  MSG_USERS_LIST msg_users_list;
  int i;
  int SthReceived;
  SthReceived = msgrcv(GetQueueID, &msg_users_list, sizeof(MSG_USERS_LIST) - sizeof(long), USERS_LIST_TYPE, IPC_NOWAIT);
  if (SthReceived > 0) {
    printf("All users:\n");
    for(i = 0; i < MAX_USERS_NUMBER * MAX_SERVERS_NUMBER; i++)
      if (strcmp(msg_users_list.users[i], "") != 0) printf("%s\n", msg_users_list.users[i]);
  } else { printf(".\n"); }
}

void Logout() {
  MSG_LOGIN msg_login;
    msg_login.type = LOGOUT;
    strcpy(msg_login.username, MyUsername);
  P(server_ids_SemID);
  msgsnd(server_ids[MyServerNr], &msg_login, sizeof(MSG_LOGIN) - sizeof(long), 0);
  V(server_ids_SemID);
}

void Quit() {
  msgctl(GetQueueID, IPC_RMID, NULL);
  kill(MenuPID, 9);
  P(server_ids_SemID);
  shmdt(server_ids);
  V(server_ids_SemID);
  exit(0);
}

void CreateGetQueue() {
  GetQueueID = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
}

int PrintServers() {
  int i, ServersThere = 0;
  if (PrepareServerIDSM()) {
    printf("Dostępne serwery:\n");
    P(server_ids_SemID);
    for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != -1) printf("Serwer #%d: %d\n", i, server_ids[i]);
    V(server_ids_SemID);
    ServersThere = 1;
  } else printf("Brak serwerow. Nie mozesz sie zalogowac.\n");
  return ServersThere;
}

int ServersOnline() {
  int i;
  P(server_ids_SemID);
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != -1) { V(server_ids_SemID); return 1; }
  V(server_ids_SemID);
  return 0;
}

// Rejestracja użytkownika na serwerze
void Register() {
  char Username[USER_NAME_MAX_LENGTH];
  int ServerID, i;
  if (PrintServers()) { // sa serwery
    for(i = 0; i < USER_NAME_MAX_LENGTH; i++) Username[i] = '\0';
    printf("Wpisz nazwe uzytkownika.\n");
    scanf("%s", Username);
    printf("Wpisz numer serwera, do ktorego chcesz sie zalogowac:\n");
    scanf("%d", &ServerID);
    strcpy(MyUsername, Username);
    MyServerNr = ServerID;
    MSG_LOGIN msg_login;
      msg_login.type = LOGIN;
      strcpy(msg_login.username, Username);
      msg_login.ipc_num = GetQueueID;
    P(server_ids_SemID);
    msgsnd(server_ids[ServerID], &msg_login, sizeof(MSG_LOGIN) - sizeof(long), 0);
    V(server_ids_SemID);
  }
}

int PrepareServerIDSM() {
  int i;
  P(server_ids_SemID);
  int ShMID = shmget(SHM_SERVER_IDS, 0, 0);
  if (ShMID < 0) { // tablica nie istnieje
    V(server_ids_SemID);
    return 0;
  } else { // tablica jest
    server_ids = (int*) shmat(ShMID, NULL, 0);
    V(server_ids_SemID);
    return ShMID;
  }
}
