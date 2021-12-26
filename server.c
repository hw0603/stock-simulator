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

#define MAX_CLNT 32
#define BUF_SIZE 128
#define COMPANY_COUNT 5


int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

struct company {
    char name[20];
    int value;
    double stdev;
} company;
struct company clist[COMPANY_COUNT];

struct User {
    char username[20];
    int balance;
    int wallet[COMPANY_COUNT];
} User;
struct User user;

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

int findidx(char* companyname) {
    for (int i = 0; i < COMPANY_COUNT; i++) {
        if (!(strcmp(clist[i].name, companyname))) {
            return i;
        }
    }
    return -1;
}


void send_msg(int clnt_sock, void* msg, int len) {
    pthread_mutex_lock(&mutx);
    write(clnt_sock, msg, len);
    pthread_mutex_unlock(&mutx);
}

void error_handling(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void* handle_clnt(void* arg) {
    int clnt_sock = *((int*)arg);
    int str_len = 0, i;
    char msg[BUF_SIZE];
    char response[BUF_SIZE];
    char optype[10] = {'\0', };
    char argument[50] = { '\0', };
    
    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        strcpy(optype, "");
        strcpy(argument, "");
        optype[0] = '\0';
        argument[0] = '\0';

        sscanf(msg, "%s %s", optype, argument);
        printf("[%s] [%s]\n", optype, argument);
        if (!(strcmp(optype, "GETLIST"))) {
            
        }
        else if (!(strcmp(optype, "BALANCE"))) {
            char temp[20];
            sprintf(temp, "%d\n", user.balance);
            send_msg(clnt_sock, temp, strlen(temp));
        }
        else if (!(strcmp(optype, "GETVAL"))) {
            int index = findidx(argument);
            if (index >= 0 && index < COMPANY_COUNT) {
                next_value(&clist[index].value, clist[index].stdev);
                sprintf(response, "%s|%d\n", clist[index].name, clist[index].value);
                send_msg(clnt_sock, response, strlen(response));
            }
            else {
                send_msg(clnt_sock, "No Match Company\n", 17);
            }
        }
        else {
            send_msg(clnt_sock, "No Match Command\n", 17);
        }
    }

    pthread_mutex_lock(&mutx);
    // remove disconnected client
    for (i = 0; i < clnt_cnt; i++) {
        if (clnt_sock == clnt_socks[i]) {
            while (i < clnt_cnt)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}


// 자식 프로세스에서 sort 시키고 부모는 거기서 읽음
int sortdata(int fd) {
    int p[2];
    int pid = 0;
    int status;

    if (pipe(p) == -1) // get a pipe
        error_handling("Cannot get a pipe");

    /*------------------------------------------------------*/
    /*   now we have a pipe, now let's get two processes    */
    if ((pid = fork()) == -1)		/* get a proc	*/
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


int main(int argc, char* argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;

    // check argv
    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    
    srand(time(NULL));

    // copy sorted data to struct company
    int fd = open("stockdata.dat", O_RDONLY);
    int pipefd = sortdata(fd);
    char temp[20];
    FILE* fp = fdopen(pipefd, "rb");
    for (int i = 0; i < COMPANY_COUNT; i++) {
        fscanf(fp, "%s %d %lf", clist[i].name, &clist[i].value, &clist[i].stdev);
        printf("%s\t%d\t%lf\n", clist[i].name, clist[i].value, clist[i].stdev);
    }
    fclose(fp);
    close(pipefd);
    

    // initalize user
    strcpy(user.username, "TestUser1");
    user.balance = 1000000;
    for (int i = 0; i < COMPANY_COUNT; i++) {
        user.wallet[i] = 0;
    }
    

    // initalize socket
    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

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

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);

    return 0;
}
