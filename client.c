#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define USER_NAME_MAX_LENGTH 10
#define RESPONSE_LENGTH 50
#define MAX_SERVERS_NUMBER 15
#define MAX_USERS_NUMBER 20 //na jednym serwerze
#define ROOM_NAME_MAX_LENGTH 10
#define MAX_MSG_LENGTH 256
#define SERVER_IDS_SM_ID 15

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

enum MSG_TYPE {LOGIN=1, RESPONSE, LOGOUT, REQUEST, MESSAGE, ROOM, SERVER2SERVER, M_USERS_LIST, M_ROOMS_LIST, M_ROOM_USERS_LIST};
typedef struct {
  long type;
  char username[USER_NAME_MAX_LENGTH];
  key_t ipc_num; //nr kolejki na której będzie nasłuchiwał klient
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

void Quit();
void Menu();
void Get();
void GetRegister();
void PrintMenu();
void CreateGetQueue();
void Register();

// lista id serwerów w postaci tablicy id kolejek, na których nasłuchują serwery
int* server_ids;
int MenuPID;
int GetQueueID;

// ------------------------------------------------------------------------

int main() {
  CreateGetQueue();
  if (MenuPID = fork()) { while(1) Menu(); }
  else { while(1) Get(); }
  return 0;
}

void Menu() {
  int Navigate;
  PrintMenu();
  scanf("%d", &Navigate);
  switch (Navigate) {
    case 1:
      Register();
      break;
    case 0:
      Quit();
  }
}

void Get() {
  // printf(".");
  GetRegister();
  // sleep(5);
}

void GetRegister() {
  MSG_RESPONSE msg_response;
  int i;
  int SthReceived;
  SthReceived = msgrcv(GetQueueID, &msg_response, sizeof(MSG_RESPONSE) - sizeof(long), RESPONSE, IPC_NOWAIT);
  if (SthReceived > 0) { printf("Odbieram: %s\n", msg_response.content); }
}

void PrintMenu() {
  printf("1 - Rejestracja uzytkownika\n");
  printf("0 - Wyjscie\n");
}

void Quit() {
  msgctl(GetQueueID, IPC_RMID, NULL);
  kill(MenuPID, 9);
  // wywal pamiec wspoldzielona
  exit(0);
}

void CreateGetQueue() {
  GetQueueID = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
}

int PrintServers() {
  int i, ServersThere = 0;
  if (PrepareServerIDSM()) {
    printf("Dostępne serwery:\n");
    for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != -1) printf("Serwer #%d: %d\n", i, server_ids[i]);
    ServersThere = 1;
  } else printf("Brak serwerow. Nie mozesz sie zalogowac.\n");
  return ServersThere;
}

int ServersOnline() {
  int i;
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != -1) return 1;
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
    MSG_LOGIN msg_login;
      msg_login.type = LOGIN;
      strcpy(msg_login.username, Username);
      msg_login.ipc_num = GetQueueID;
    msgsnd(server_ids[ServerID], &msg_login, sizeof(MSG_LOGIN) - sizeof(long), 0);
    printf("twoje queue id ktore podales to %d\n", GetQueueID);
    printf("Your request [%s, %d] was sent to the server. Wait for response...\n", Username, ServerID);
  }
}

int PrepareServerIDSM() {
  int i;
  int ShMID = shmget(SERVER_IDS_SM_ID, 0, 0);
  if (ShMID < 0) { // tablica nie istnieje
    return 0;
  } else { // tablica jest
    server_ids = (int*) shmat(ShMID, NULL, 0);
    return 1;
  }
}
