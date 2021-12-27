//  file: server.c
//  author: 2018115385 류형욱
//  datetime: 2021-12-27 16:10
//  description: simple stock simulator server using socket

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h> 
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

#define MAX_CLNT 32
#define MSG_BUF_SIZE 128
#define DEFAULT_BALANCE 1000000 // 사용자 잔고
#define DEFAULT_UPDATE_INTERVAL 3 // 주가 업데이트 주기
int COMPANY_COUNT = 0; // data파일에서 읽어온 총 종목 개수


int clnt_cnt = 0; // 현재 연결된 총 클라이언트 수
int clnt_socks[MAX_CLNT]; // 클라이언트들의 sockid 배열
pthread_mutex_t mutx_comm; // socket 통신에 사용할 mutex
pthread_mutex_t mutx_update; // 주가 update 시 사용할 mutex


// 구조체 정의
struct company {
    char name[20];
    int value;
    double stdev;
} company;
struct company* clist;

struct User {
    char username[20];
    int balance;
    int* wallet;
} User;
struct User* userlist;


// 함수 원형 정의
double gaussianRandom(double, double);
int* next_value(int*, double);
void* update_stockval(void*);
int findidx(char*);
int finduseridx(int);
void send_msg(int, void*, int);
void error_handling(char*);
int sortdata(int);
void shutdown_server(int);


// 가우시안 랜덤 함수
double gaussianRandom(double average, double stdev) {
    double v1, v2, s, temp;

    do {
        v1 = 2 * ((double)rand() / RAND_MAX) - 1;      // -1.0 ~ 1.0 까지의 값
        v2 = 2 * ((double)rand() / RAND_MAX) - 1;      // -1.0 ~ 1.0 까지의 값
        s = v1 * v1 + v2 * v2;
    } while (s >= 1 || s == 0);

    s = sqrt((-2 * log(s)) / s);

    temp = v1 * s;
    temp = (stdev * temp) + average;

    return temp;
}

// 기준치와 표준편차를 받아서 주가 계산
int* next_value(int* valueptr, double stdev) {
    int value = *valueptr;
    double random = gaussianRandom(0.0, stdev);

    value += value * random;
    value = value > 1000 ? value : 1000;

    *valueptr = value;

    return valueptr;
}

// 주가 정보 업데이트
void* update_stockval(void* temp) {
    while (1) {
        pthread_mutex_lock(&mutx_update);
        for (int i = 0; i < COMPANY_COUNT; i++) {
            next_value(&clist[i].value, clist[i].stdev);
            // printf("%s | %d\n", clist[i].name, clist[i].value);
        }
        pthread_mutex_unlock(&mutx_update);
        sleep(DEFAULT_UPDATE_INTERVAL);
    }
    return NULL;
}

// 종목 인덱스
int findidx(char* companyname) {
    for (int i = 0; i < COMPANY_COUNT; i++) {
        if (!(strcmp(clist[i].name, companyname))) {
            return i;
        }
    }
    return -1;
}

// 사용자 인덱스
int finduseridx(int sockid) {
    for (int i = 0; i < clnt_cnt; i++) {
        if (clnt_socks[i] == sockid) {
            return i;
        }
    }
    return -1;
}

// Thread-safe msg transfer
void send_msg(int clnt_sock, void* msg, int len) {
    pthread_mutex_lock(&mutx_comm);
    write(clnt_sock, msg, len);
    pthread_mutex_unlock(&mutx_comm);
}


void error_handling(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}


void* handle_clnt(void* arg) {
    int clnt_sock = *((int*)arg);
    int str_len = 0;
    char msg[MSG_BUF_SIZE];
    
    int amount = 0;
    int flag = 0;

    int useridx = finduseridx(clnt_sock);


    char* sArr[10] = { NULL, };    // 크기가 10인 문자열 포인터 배열을 선언하고 NULL로 초기화
    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        for (int i = 0; i < 10; i++) {
            sArr[i] = NULL;
        }

        int idx = 0;                     // 문자열 포인터 배열의 인덱스로 사용할 변수

        char* ptr = strtok(msg, " ");   // 공백 문자열을 기준으로 문자열을 자름

        // 자른 문자열이 나오지 않을 때까지 반복
        while (ptr != NULL) {
            sArr[idx] = ptr;             // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
            idx++;                       // 인덱스 증가

            ptr = strtok(NULL, " ");   // 다음 문자열을 잘라서 포인터를 반환
        }

        if (!sArr[0]) {
            printf("아무 명령도 입력되지 않았습니다.\n");
            continue;
        }
        else if (!(strcmp(sArr[0], "GETLIST"))) {
            send_msg(clnt_sock, (void*)clist, sizeof(struct company)*COMPANY_COUNT);
            printf("[%s] Stock List 조회\n", userlist[useridx].username);
        }
        else if (!(strcmp(sArr[0], "USERINFO"))) {
            send_msg(clnt_sock, (void*)&userlist[useridx].username, sizeof(userlist[useridx].username));
            send_msg(clnt_sock, (void*)&userlist[useridx].balance, sizeof(int));
            send_msg(clnt_sock, (void*)userlist[useridx].wallet, sizeof(int) * COMPANY_COUNT);
            printf("[%s] UserInfo 조회\n", userlist[useridx].username);
        }
        else if (!(strcmp(sArr[0], "BUY"))) {
            if (sArr[1] && sArr[2]) {
                int companyidx = findidx(sArr[1]);
                if (companyidx == -1) {
                    // printf("%s 는 올바른 회사 이름이 아닙니다.\n", sArr[1]);
                    flag = 0;
                    send_msg(clnt_sock, &flag, sizeof(flag));
                }
                else {
                    amount = atoi(sArr[2]);

                    if (amount * clist[companyidx].value > userlist[useridx].balance) {
                        printf("[%s] 잔액이 부족합니다.\n", userlist[useridx].username);
                        flag = 0;
                        send_msg(clnt_sock, &flag, sizeof(flag));
                        continue;
                    }
                    pthread_mutex_lock(&mutx_update);
                    userlist[useridx].balance -= amount * clist[companyidx].value;
                    userlist[useridx].wallet[companyidx] += amount;
                    pthread_mutex_unlock(&mutx_update);
                    
                    printf("[%s] %s %d주를 %d에 구매 -> 잔액: %d\n", userlist[useridx].username, clist[companyidx].name, amount, clist[companyidx].value, userlist[useridx].balance);
                    // for (int i = 0; i < COMPANY_COUNT; i++) {
                    //     printf("%s:%d\t", clist[i].name, userlist[useridx].wallet[i]);
                    // }
                    puts("");
                    flag = 1;
                    send_msg(clnt_sock, &flag, sizeof(flag));
                }
            }
            else {
                // printf("회사 이름 또는 수량이 지정되지 않았습니다.\n");
                flag = 0;
                send_msg(clnt_sock, &flag, sizeof(flag));
            }
        }
        else if (!(strcmp(sArr[0], "SELL"))) {
            if (sArr[1] && sArr[2]) {
                int companyidx = findidx(sArr[1]);
                if (companyidx == -1) {
                    // printf("%s 는 올바른 회사 이름이 아닙니다.\n", sArr[1]);
                    flag = 0;
                    send_msg(clnt_sock, &flag, sizeof(flag));
                }
                else {
                    amount = atoi(sArr[2]);

                    if (amount > userlist[useridx].wallet[companyidx]) {
                        printf("[%s] 보유 수량보다 더 많은 주식을 팔 수 없습니다.\n", userlist[useridx].username);
                        flag = 0;
                        send_msg(clnt_sock, &flag, sizeof(flag));
                        continue;
                    }
                    pthread_mutex_lock(&mutx_update);
                    userlist[useridx].balance += amount * clist[companyidx].value;
                    userlist[useridx].wallet[companyidx] -= amount;
                    pthread_mutex_unlock(&mutx_update);
                    
                    printf("[%s] %s %d주를 %d에 판매 -> 잔액: %d\n", userlist[useridx].username, clist[companyidx].name, amount, clist[companyidx].value, userlist[useridx].balance);
                    // for (int i = 0; i < COMPANY_COUNT; i++) {
                    //     printf("%s:%d\t", clist[i].name, userlist[useridx].wallet[i]);
                    // }
                    puts("");
                    flag = 1;
                    send_msg(clnt_sock, &flag, sizeof(flag));
                }
            }
            else {
                // printf("회사 이름 또는 수량이 지정되지 않았습니다.\n");
                flag = 0;
                send_msg(clnt_sock, &flag, sizeof(flag));
            }
        }
        else {
            printf("[%s] %s 는 올바른 명령이 아닙니다.\n", userlist[useridx].username, sArr[0]);
        }
    }

    pthread_mutex_lock(&mutx_comm);
    // remove disconnected client
    for (int i = 0; i < clnt_cnt; i++) {
        if (clnt_sock == clnt_socks[i]) {
            while (i < clnt_cnt) {
                clnt_socks[i] = clnt_socks[i + 1];
                i++;
            }
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx_comm);
    close(clnt_sock);
    printf("[%s] 접속 종료\n", userlist[useridx].username);
    return NULL;
}

// fork() -> sort() in child -> return read_end of pipe
int sortdata(int fd) {
    int p[2];
    int pid = 0;
    int status;

    if (pipe(p) == -1)
        error_handling("Cannot get a pipe");

    if ((pid = fork()) == -1)
        error_handling("Cannot fork");


    if (pid > 0) {
        close(p[1]);
        if (waitpid(pid, &status, 0) == -1)
            error_handling("wait error");
    }
    else {
        close(p[0]);
        if (dup2(p[1], 1) == -1)
            error_handling("could not redirect stdout");
        close(p[1]);
        // stdin을 데이터 파일로 redirect
        if (dup2(fd, 0) == -1)
            error_handling("could not redirect stdin");

        execlp("sort", "sort", NULL);
        
        error_handling("error sorting");
    }
    return p[0];
}

// shutdown server and release resources
void shutdown_server(int signo) {
    pthread_mutex_lock(&mutx_comm);
    pthread_mutex_lock(&mutx_update);
    for (int i = 0; i < clnt_cnt; i++) {
        close(clnt_socks[i]);
        free(userlist[i].wallet);
    }
    free(userlist);
    free(clist);

    pthread_mutex_unlock(&mutx_comm);
    pthread_mutex_unlock(&mutx_update);

    printf("서버 종료\n");
    exit(0);
}


int main(int argc, char* argv[]) {
    pthread_t t_comm, t_update;
    struct sockaddr_in serv_adr, clnt_adr;
    int serv_sock, clnt_sock;
    int clnt_adr_sz;

    // allocate memory to userlist, companylist
    userlist = (struct User*)malloc(sizeof(struct User));
    clist = (struct company*)malloc(sizeof(struct company));

    // check argv
    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // set SIGINT Handler
    signal(SIGINT, shutdown_server);
    signal(SIGQUIT, shutdown_server);

    srand(time(NULL));

    // copy sorted data to struct company
    int fd = open("stockdata.dat", O_RDONLY);
    int pipefd = sortdata(fd);
    FILE* fp = fdopen(pipefd, "r");
    
    for (int i = 0; ; i++, COMPANY_COUNT = i) {
        clist = (struct company*)realloc(clist, sizeof(struct company) * (i + 1));
        fscanf(fp, "%s %d %lf", clist[i].name, &clist[i].value, &clist[i].stdev);
        if (feof(fp))
            break;
        printf("%s\t%d\t%lf\n", clist[i].name, clist[i].value, clist[i].stdev);
        // COMPANY_COUNT = i + 1;
    }
    printf("총 %d 개의 종목을 로드했습니다.\n", COMPANY_COUNT);
    fclose(fp);
    close(pipefd);

    // initalize stockval update thread
    pthread_mutex_init(&mutx_update, NULL);
    pthread_create(&t_update, NULL, update_stockval, (void*)NULL);

    // initalize socket
    pthread_mutex_init(&mutx_comm, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    int opt = 1;
    
    // Allow Duplicated bind()
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");


    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx_comm);
        clnt_socks[clnt_cnt++] = clnt_sock;

        // initalize user
        int userlist_size = (sizeof(userlist[0].username) + sizeof(int) + sizeof(int) * COMPANY_COUNT) * clnt_cnt;
        userlist = (struct User*)realloc(userlist, userlist_size);
        userlist[clnt_cnt-1].wallet = (int*)malloc(sizeof(int) * COMPANY_COUNT);
        sprintf(userlist[clnt_cnt-1].username, "User_%d(%d)", clnt_cnt, clnt_sock);
        userlist[clnt_cnt-1].balance = DEFAULT_BALANCE;
        for (int i = 0; i < COMPANY_COUNT; i++) {
            userlist[clnt_cnt-1].wallet[i] = 0;
        }
        
        // clnt_cnt++;
        pthread_mutex_unlock(&mutx_comm);

        pthread_create(&t_comm, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_comm);
        printf("\nClient connected! IP: %s Username: %s\n", inet_ntoa(clnt_adr.sin_addr), userlist[clnt_cnt - 1].username);
    }
    close(serv_sock);

    return 0;
}
