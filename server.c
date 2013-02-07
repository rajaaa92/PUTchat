#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define INTSIZE 8

#define USER_NAME_MAX_LENGTH 10
#define RESPONSE_LENGTH 50
#define MAX_SERVERS_NUMBER 15
#define MAX_USERS_NUMBER 20 //na jednym serwerze
#define ROOM_NAME_MAX_LENGTH 10
#define MAX_MSG_LENGTH 256
#define USER_SERVER_SM_ID 20
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

typedef struct {
  char Username[USER_NAME_MAX_LENGTH];
  int GetQueueID;
} TUser;

void Quit();
void Menu();
void PrintMenu();
int AmILastServer();
void Register();
void PrepareUsersArray();
void GetMessages();
void GetLogin();
void PrintServers();
void CreateGetQueue();
void PrintUsers();
void PrintAllUsers();
void Unregister();
void PrepareUSSM();
void Get();

// pamięć współdzielona serwerów
int* server_ids;
USER_SERVER* user_server;

int GetQueueID;
int MenuPID;
TUser Users[MAX_USERS_NUMBER];

// ------------------------------------------------------------------------

int main() {
  CreateGetQueue();
  Register();
  if (MenuPID = fork()) { while(1) Menu(); }
  else {
    signal(30, PrintUsers);
    while(1) Get();
  }
  return 0;
}

void Menu() {
  int Navigate;
  PrintMenu();
  scanf("%d", &Navigate);
  switcher:
    switch (Navigate) {
      case 3:
        PrintAllUsers();
        break;
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

void Quit() {
  Unregister();
  exit(0);
}

void PrintUsers() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) {
    if (strcmp(Users[i].Username, "")) { // strings egals -> return 0
      printf("User #%d: %s listening at: %d.\n", i, Users[i].Username, Users[i].GetQueueID);
    }
  }
}

void PrintAllUsers() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER * MAX_SERVERS_NUMBER; i++) {
    if (strcmp(user_server[i].user_name, "")) { // strings egals -> return 0
      printf("User #%d: %s @ %d.\n", i, user_server[i].user_name, user_server[i].server_id);
    }
  }
}

void PrintMenu() {
  printf("3 - Pokaz wszystkich uzytkownikow\n");
  printf("2 - Pokaz uzytkownikow na tym serwerze\n");
  printf("1 - Pokaz ID serwerow\n");
  printf("0 - Wyjscie\n");
}

void Unregister() {
  int i;
  int ShMID;
  kill(MenuPID, 9);
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] == GetQueueID) { server_ids[i] = 0; break; }
  shmdt(server_ids);
  shmdt(user_server);
  msgctl(GetQueueID, IPC_RMID, NULL);
  if (AmILastServer) {
    ShMID = shmget(SERVER_IDS_SM_ID, 0, 0);
    shmctl(ShMID, IPC_RMID, 0);
    ShMID = shmget(USER_SERVER_SM_ID, 0, 0);
    shmctl(ShMID, IPC_RMID, 0);
  }
}

int AmILastServer() {
  int i;
  int NumberOfServers = 0;
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] == 0) NumberOfServers++;
  if (NumberOfServers == 1) return 1;
  else return 0;
}

void Register() {
  int Success = PrepareServerIDSM();
  if (Success == 0) {
    shmdt(server_ids);
    printf("Nie moge zarejestrowac serwera - brak miejsca.\n");
    exit(0);
  } else {
    PrepareUSSM();
    PrepareUsersArray();
    printf("Zarejestrowano serwer.\n");
  }
}

int PrepareServerIDSM() {
  int i;
  int ShMID = shmget(SERVER_IDS_SM_ID, 60, IPC_EXCL | IPC_CREAT | 0777);
  if (ShMID < 0) { // tablica już istnieje w pamięci
    ShMID = shmget(SERVER_IDS_SM_ID, 0, 0);
    server_ids = (int*) shmat(ShMID, NULL, 0);
  } else { // tablica zostanie utworzona
    server_ids = (int*) shmat(ShMID, NULL, 0);
    for (i = 0; i < MAX_SERVERS_NUMBER; i++) server_ids[i] = 0;
  }
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) {
    if (server_ids[i] == 0) {
      server_ids[i] = GetQueueID;
      return 1;
      break;
    }
  }
  return 0;
}

void PrepareUSSM() {
  int i;
  int ShMID = shmget(USER_SERVER_SM_ID, 14 * MAX_SERVERS_NUMBER * MAX_USERS_NUMBER, IPC_EXCL | IPC_CREAT | 0777);
  if (ShMID < 0) { // tablica już istnieje w pamięci
    ShMID = shmget(USER_SERVER_SM_ID, 0, 0);
    user_server = (USER_SERVER*) shmat(ShMID, NULL, 0);
  } else { // tablica zostanie utworzona
    user_server = (USER_SERVER*) shmat(ShMID, NULL, 0);
    for (i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++) strcpy(user_server[i].user_name, "");
  }
}

void PrepareUsersArray() {
  int i;
  for (i = 0; i < MAX_USERS_NUMBER; i++) {
    strcpy(Users[i].Username, "");
  }
}

void Get() {
  printf(".");
  GetLogin();
  sleep(5);
}

void GetLogin() {
  MSG_LOGIN msg_login;
  MSG_RESPONSE msg_response;
  int i;
  int WhereToLogin = -1;
  int Taken = 0;
  int SthReceived, SthSent;
  SthReceived = msgrcv(GetQueueID, &msg_login, USER_NAME_MAX_LENGTH + INTSIZE, LOGIN, IPC_NOWAIT);
  if (SthReceived > 0) {
    printf("Odbieram: %s, id queue: %d\n", msg_login.username, msg_login.ipc_num);
    for (i = 0; i < MAX_USERS_NUMBER; i++) {
      printf("Users[%d].Username = %s.\n", i, Users[i].Username);
      if (!strcmp(Users[i].Username, "")) { WhereToLogin = i; break; } // returns 0 if equal
      if (!strcmp(Users[i].Username, msg_login.username)) { Taken = 1; break; }
    }
    printf("zebralem where to login = %d i nazwe czy jest taken = %d\n", WhereToLogin, Taken);
    if ((Taken == 0) && (WhereToLogin > -1)) {
      printf("dupa1");
      for (i = 0; i < MAX_SERVERS_NUMBER * MAX_USERS_NUMBER; i++) {
        if (!strcmp(user_server[i].user_name, "")) { // 0 if equal
          strcpy(Users[WhereToLogin].Username, msg_login.username);
          Users[WhereToLogin].GetQueueID = msg_login.ipc_num;
          strcpy(user_server[i].user_name, msg_login.username);
          user_server[i].server_id = GetQueueID;
          msg_response.type = RESPONSE;
          msg_response.response_type = LOGIN_SUCCESS;
          strcpy(msg_response.content, "Zalogowano.");
          SthSent = msgsnd(msg_login.ipc_num, &msg_response, RESPONSE_LENGTH + INTSIZE, 0);
          if (!SthSent) printf("wysylam resonse o loginie na %d\n", msg_login.ipc_num);
          else printf("%d\n", SthSent);
          printf("oto tablica moich userow a dokladniej ten jeden indeks:\n");
          printf("indeks: %d, Username: %s, GetQueueID: %d\n", WhereToLogin, Users[WhereToLogin].Username, Users[WhereToLogin].GetQueueID);
          break;
        }
      }
    } else {
      printf("dupa");
      msg_response.type = RESPONSE;
      msg_response.response_type = LOGIN_FAILED;
      strcpy(msg_response.content, "Nazwa uzyta przez innego uzytkownika lub nie ma miejsca na serwerze.");
      printf("bum");
      SthSent = msgsnd(msg_login.ipc_num, &msg_response, RESPONSE_LENGTH + INTSIZE, 0);
      if (!SthSent) printf("wysylam resonse o nieloginie na %d\n", msg_login.ipc_num);
      else printf("%d\n", SthSent);
    }
    printf("aaaaaa\n");
  }
}

void PrintServers() {
  int i;
  for (i = 0; i < MAX_SERVERS_NUMBER; i++) if (server_ids[i] != 0) printf("Serwer #%d: %d\n", i, server_ids[i]);
}

void CreateGetQueue() {
  GetQueueID = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
}
