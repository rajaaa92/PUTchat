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
#include <sys/time.h>

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
void SendLogin();
void SendLogout();
void SendPrintUsers();
void GetUsersList();
void SendMsg(int);
void SetLoggedIn();
void GetMessage();
void SendHeartBeat();
void SendEnterRoom();
void GetRoomsList();
void SendPrintRooms();
void SendQuitRoom();

int MenuPID;
int GetQueueID;
char MyUsername[USER_NAME_MAX_LENGTH];
int MyServerID;
char Room[ROOM_NAME_MAX_LENGTH];
int LoggedIn = 0;

// ------------ MENU i main -----------------------------------------------

int main() {
  CreateGetQueue();
  // printf("%d\n", GetQueueID);
  MenuPID = fork();
  if (MenuPID) {
    signal(31, SetLoggedIn);
    while(1) Menu();
  }
  else { while(1) Get(); }
  return 0;
}

void Menu() {
  int Navigate;
  PrintMenu();
  scanf("%d", &Navigate);
  switch (Navigate) {
    case 7:
      SendQuitRoom();
      break;
    case 6:
      SendPrintRooms();
      break;
    case 5:
      SendEnterRoom();
      break;
    case 4:
      SendMsg(PUBLIC);
      break;
    case 3:
      SendMsg(PRIVATE);
      break;
    case 2:
      SendPrintUsers();
      break;
    case 1:
      SendLogin();
      break;
    case 0:
      if (LoggedIn) SendLogout(); else Quit();
      break;
  }
}

void PrintMenu() {
  printf("7 - Wyjscie z pokoju\n");
  printf("6 - Wyswietl pokoje\n");
  printf("5 - Wejscie do pokoju / zmiana pokoju\n");
  printf("4 - Wyslij wiadomosc do wszystkich\n");
  printf("3 - Wyslij wiadomosc do uzytkownika / na pokoj\n");
  printf("2 - Wyswietl uzytkownikow\n");
  printf("1 - Rejestracja uzytkownika\n");
  printf("0 - Wyjscie\n");
}

// ------------------- GET --------------------------------------------------

void Get() {
  // printf(".\n");
  GetResponse();
  GetUsersList();
  GetRoomsList();
  GetMessage();
  sleep(5);
}

void GetResponse() {
  MSG_RESPONSE msg_response;
  int SthReceived;
  SthReceived = msgrcv(GetQueueID, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), RESPONSE, IPC_NOWAIT);
  if (SthReceived > 0) {
    if (strcmp(msg_response.content, "ping") != 0)
      printf("SERVER: %s\n", msg_response.content);
    if (msg_response.response_type == LOGOUT_SUCCESS) Quit();
    if (msg_response.response_type == LOGIN_SUCCESS) { LoggedIn = 1; kill(getppid(), 31); }
    if (msg_response.response_type == PING) SendHeartBeat();
    PrintMenu();
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
  }
}

void GetRoomsList() {
  MSG_USERS_LIST msg_rooms_list;
  int i;
  int SthReceived = msgrcv(GetQueueID, &msg_rooms_list, sizeof(MSG_USERS_LIST) - sizeof(long), ROOMS_LIST_TYPE, IPC_NOWAIT);
  if (SthReceived > 0) {
    printf("All rooms:\n");
    for(i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++)
      if (strcmp(msg_rooms_list.users[i], "") != 0) printf("%s\n", msg_rooms_list.users[i]);
  }
}

void GetMessage() {
  MSG_CHAT_MESSAGE msg_chat_message;
  int SthReceived = msgrcv(GetQueueID, &msg_chat_message, sizeof(MSG_CHAT_MESSAGE) - sizeof(long), MESSAGE, IPC_NOWAIT);
  if (SthReceived > 0) {
    if (msg_chat_message.msg_type == PRIVATE) printf("--------- a new private message ------------\n");
    else printf("--------------- a new public message ---------------\n");
    printf("At %s %s writes:\n", msg_chat_message.send_time, msg_chat_message.sender);
    printf("%s\n", msg_chat_message.message);
  }
}

// ------------------------------ SEND -----------------------------------------

void SendLogin() {
  int i;
  for(i = 0; i < USER_NAME_MAX_LENGTH; i++) { MyUsername[i] = '\0'; }
  printf("Wpisz nazwe uzytkownika.\n");
  scanf("%s", MyUsername);
  printf("Wpisz ID serwera, do ktorego chcesz sie zalogowac:\n");
  scanf("%d", &MyServerID);
  MSG_LOGIN msg_login;
    msg_login.type = LOGIN;
    strcpy(msg_login.username, MyUsername);
    msg_login.ipc_num = GetQueueID;
  msgsnd(MyServerID, &msg_login, sizeof(MSG_LOGIN) - sizeof(long), 0);
}

void SendLogout() {
  MSG_LOGIN msg_login;
    msg_login.type = LOGOUT;
    strcpy(msg_login.username, MyUsername);
  msgsnd(MyServerID, &msg_login, sizeof(MSG_LOGIN) - sizeof(long), 0);
}

void SendPrintUsers() {
  MSG_REQUEST msg_request;
    msg_request.type = REQUEST;
    msg_request.request_type = USERS_LIST_TYPE;
    strcpy(msg_request.user_name, MyUsername);
  msgsnd(MyServerID, &msg_request, sizeof(MSG_REQUEST) - sizeof(long), 0);
}

void SendPrintRooms() {
  MSG_REQUEST msg_request;
    msg_request.type = REQUEST;
    msg_request.request_type = ROOMS_LIST_TYPE;
    strcpy(msg_request.user_name, MyUsername);
  msgsnd(MyServerID, &msg_request, sizeof(MSG_REQUEST) - sizeof(long), 0);
  printf("Wyslalem requesta o print rooms\n");
}

void SendMsg(int type) {
  char c;
  time_t time_now = time(NULL);
  struct tm time_now_local = *localtime(&time_now);
  MSG_CHAT_MESSAGE msg_chat_message;
    msg_chat_message.type = MESSAGE;
    msg_chat_message.msg_type = type;
    strcpy(msg_chat_message.sender, MyUsername);
    msg_chat_message.send_time[0] = (char)(((int)'0') + (time_now_local.tm_hour / 10));
    msg_chat_message.send_time[1] = (char)(((int)'0') + (time_now_local.tm_hour % 10));
    msg_chat_message.send_time[2] = ':';
    msg_chat_message.send_time[3] = (char)(((int)'0') + (time_now_local.tm_min / 10));
    msg_chat_message.send_time[4] = (char)(((int)'0') + (time_now_local.tm_min % 10));
    msg_chat_message.send_time[5] = '\0';
  if (type == PRIVATE) {
    printf("Receiver:\n");
    scanf("%s", msg_chat_message.receiver);
  }
  printf("Message:\n");
  scanf("%c%[^\n]", &c, msg_chat_message.message);
  msgsnd(MyServerID, &msg_chat_message, sizeof(MSG_CHAT_MESSAGE) - sizeof(long), 0);
}

void SendHeartBeat() {
  MSG_REQUEST msg_request;
    msg_request.type = REQUEST;
    msg_request.request_type = PONG;
    strcpy(msg_request.user_name, MyUsername);
  msgsnd(MyServerID, &msg_request, sizeof(MSG_REQUEST) - sizeof(long), 0);
}

void SendEnterRoom() {
  int SthSent;
  MSG_ROOM msg_room;
    msg_room.type = ROOM;
    msg_room.operation_type = ENTER_ROOM;
    strcpy(msg_room.user_name, MyUsername);
    printf("Podaj nazwe pokoju:\n");
    scanf("%s", msg_room.room_name);
    strcpy(Room, msg_room.room_name);
  SthSent = msgsnd(MyServerID, &msg_room, sizeof(MSG_ROOM) - sizeof(long), 0);
  printf("SthSent = %d, Wyslalem requesta o room\n", SthSent);
}

void SendQuitRoom() {
  int SthSent;
  MSG_ROOM msg_room;
    msg_room.type = ROOM;
    msg_room.operation_type = LEAVE_ROOM;
    strcpy(msg_room.user_name, MyUsername);
    strcpy(msg_room.room_name, Room);
    strcpy(Room, "");
  SthSent = msgsnd(MyServerID, &msg_room, sizeof(MSG_ROOM) - sizeof(long), 0);
}

// ----------------- OTHER -----------------------------------------------------------

void CreateGetQueue() {
  GetQueueID = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
}

void SetLoggedIn() {
  if (LoggedIn) LoggedIn = 0; else LoggedIn = 1;
}

void Quit() {
  msgctl(GetQueueID, IPC_RMID, NULL);
  kill(MenuPID, 9);
  exit(0);
}


