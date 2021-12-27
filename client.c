#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <pthread.h>

#include <sys/time.h>

#include <aio.h>

int MAXW;
int MAXH;
#define WORDSIZE 5

#define LOGOSIZE 5

int LOGOROW;
int LOGOCOL;

int MENUROW;
int MENUCOL; 

#define HOSTLEN 256

#define STOCKS 11

#define oops(msg) {mvprintw(LOGOROW+10, LOGOCOL, msg); refresh();}


struct winsize w;

// user가 어느 화면에 있는지
enum states { MAIN, LIST, TRADE, ACCOUNT, HELP };

int comp_max = 0;
struct company
{
    char name[20];
    int value;
    int initvalue;
    double stdev;
} company;
struct company company_list[BUFSIZ];
struct company pre_company_list[BUFSIZ];
float rise_rate[STOCKS] = {0.0,};

struct User
{
    char username[20];
    int money;
    int stock[STOCKS];
};

struct User user;



char** empty;
int port;
char* hostname;

struct aiocb kbcbuf;   /* an aio control buf   */
int dir = 1;
int done = 0;

// 키보드 위치
int key_row = 0;
int key_col = 0;
int amount = 0;
// socket - server
int sock_id;

pthread_t t_list[10];

int fl = 0;
int stage = MAIN;


void err_msg(char *);


// screen settings
void color_setup();
void empty_setup();

// I/O settings
void set_crmode(void);
void tty_mode(int);
void setup_aio_buffer();
void on_input(int );
void set_signal();

//  screens
void start();

// animate
void* main_menu(void*);
void* stock_list(void*);
void account_info();
void helpme();


void list_comp();
int set_ticker(int);

void buy(int, int);
void sell(int, int);

void connect_to_server(char**);
void req_account_info();
void req_comp_list();
void req_buy_sell();

void update_stock(int);

int first_access = 0;
int first_balance = 0;


pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t flag = PTHREAD_COND_INITIALIZER;
int main(int ac, char* av[]) {

    hostname = av[1];
    port = atoi(av[2]);
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    MAXW = w.ws_col;
    MAXH = w.ws_row;
    LOGOROW = MAXH/4;
    LOGOCOL = MAXW/2 - LOGOSIZE*2.5;

    MENUROW = MAXH/2 + MAXH/4;
    MENUCOL = MAXW/2 - LOGOSIZE; 

    tty_mode(0);
    set_crmode();
    set_signal();
    signal(SIGIO, on_input);
    setup_aio_buffer();
    aio_read(&kbcbuf);

    connect_to_server(av);

    initscr();
    color_setup();
    empty_setup();

    start();


    endwin();
    tty_mode(1);

    return 0;
}

void err_msg(char *msg)
{
    clear();
    oops(msg);
    while( getchar() != -1 );
    endwin();
    printf("%s\n",msg);
    exit(3);
}


void connect_to_server(char** argv) {
    int sock;
    struct sockaddr_in serv_addr;
    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    {
        printf("Error : connect()\n");
        exit(3);
        return;
    }

    sock_id = sock;
    req_account_info();
    return;
}

void req_comp_list() {

    char msg[BUFSIZ];
    int bt;

    if(first_access != 0)
    {
        for( int i = 0 ; i<comp_max ; i++ )
        {
            pre_company_list[i] = company_list[i];
        }

    }


    strcpy(msg, "GETLIST");
    write(sock_id, msg, strlen(msg) + 1);
    if ((bt = read(sock_id, company_list, BUFSIZ)) == -1)
    {
        err_msg("Error : read()");
        return;
    }
    comp_max = bt / sizeof(struct company);
    if(first_access != 0)
    {   
        for( int i = 0 ; i<comp_max ; i++ )
        {
            rise_rate[i] = (float)(company_list[i].value - company_list[i].initvalue) / (float)pre_company_list[i].initvalue;
            
        }
        
    }
    first_access = 1;


    // 입력 ./program snow portnum
}

void req_account_info() {
    char msg[BUFSIZ];
    strcpy(msg, "USERINFO");
    write(sock_id, msg, strlen(msg) + 1);
    if ((read(sock_id, (char*)user.username, sizeof(user.username))) == -1)
    {
        err_msg("Error : read()");
        return;
    }
    if ((read(sock_id, (int*)&user.money, sizeof(user.money))) == -1) {
        err_msg("Error : read()");
        return;
    }
    if ((read(sock_id, (int*)&user.stock, sizeof(user.stock))) == -1) {
        err_msg("Error : read()");
        return;
    }
}

void tty_mode(int how) {
    static struct termios original_mode;
    static int original_flags;

    if (how == 0)    {
        tcgetattr(0, &original_mode);
        original_flags = fcntl(0, F_GETFL);
    }
    else    {
        tcsetattr(0, TCSANOW, &original_mode);
        original_flags = fcntl(0, F_SETFL, original_flags);
    }
}

void set_crmode(void) {
    struct termios state;
    tcgetattr(0, &state);
    state.c_lflag &= ~ICANON;
    state.c_cc[VMIN] = 1;

    tcsetattr(0, TCSANOW, &state);
}

void setup_aio_buffer()
{
    static char input[1];            /* 1 char of input */

   /* describe what to read */
   kbcbuf.aio_fildes     = 0;             /* standard intput */
   kbcbuf.aio_buf        = input;         /* buffer          */
   kbcbuf.aio_nbytes     = 1;             /* number to read  */
   kbcbuf.aio_offset     = 0;             /* offset in file  */

   /* describe what to do when read is ready */
   kbcbuf.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
   kbcbuf.aio_sigevent.sigev_signo  = SIGIO;  /* send SIGIO   */
}

void on_input(int snum)
{  
    int  c;
    char *cp = (char *) kbcbuf.aio_buf;   /* cast to char * */
    /* check for errors */
    if (aio_error(&kbcbuf) != 0)
        perror("reading failed");
    else 
        /* get number of chars read */
        if (aio_return(&kbcbuf) == 1)
        {
            c = *cp;

           
                if (c == 's')
                {
                    if (stage == MAIN && key_row == 2)
                    {
                        aio_read(&kbcbuf);
                        return;
                    }
                    if (stage == LIST && key_row == comp_max - 1)
                        {
                        aio_read(&kbcbuf);
                        return;
                    }
                    key_row++;
                }
                if (c == 'w')
                {
                    if (key_row == 0)
                        {
                        aio_read(&kbcbuf);
                        return;
                    }
                    key_row--;
                }
                if (c == 'a' && stage == LIST)
                {
                    if (key_col == 0)
                        {
                        aio_read(&kbcbuf);
                        return;
                    }
                    key_col--;
                }
                if (c == 'd' && stage == LIST)
                {
                    if (key_col == 2)
                        {
                        aio_read(&kbcbuf);
                        return;
                    }
                    key_col++;
                }
                if ((c == '=' || c == '+')&& stage == LIST)
                {
                    if (amount == 10)
                        {
                        aio_read(&kbcbuf);
                        return;
                    }
                    amount++;
                }
                if ((c == '-' || c == '_') && stage == LIST)
                {
                    if (amount == 0)
                        {
                        aio_read(&kbcbuf);
                        return;
                    }
                    amount--;
                }
                if (c == 13)             {
                    if (stage == MAIN){
                        fl = 1;
                        pthread_join(t_list[0], NULL);
                        fl = 0;
                        if (key_row == 0)
                        {
                            // main_menu 끄고 stock_list animate 키고
                            stage = LIST;
                            pthread_create(&t_list[1], NULL, stock_list, NULL);
                        }
                        if (key_row == 1)
                        {
                            // main_menu 끄고 account_info
                            stage = ACCOUNT;
                            account_info();
                            stage = MAIN;
                            pthread_create(&t_list[0], NULL, main_menu, NULL);
                        }
                        if (key_row == 2)
                        {
                            // main_menu  끄고 helpme
                            stage = HELP;
                            helpme();
                            stage = MAIN;
                            pthread_create(&t_list[0], NULL, main_menu, NULL);
                        }
                    }
                    else if (stage == LIST)
                    {
                        fl = 1;
                        pthread_join(t_list[1], NULL);
                        fl = 0;
                        stage = TRADE;
                        if (key_col == 0)
                        {
                            buy(key_row, amount);
                        }
                        if (key_col == 1)
                        {
                            sell(key_row, amount);
                        }
                        if (key_col == 2)
                        {
                            sell(key_row, 11);
                        }
                        amount = 0;
                        key_col = 0;
                        key_row = 0;
                        stage = LIST;
                        pthread_create(&t_list[1], NULL, stock_list, NULL);
                    }
                    else if (stage == ACCOUNT)
                    {

                    }
                    refresh();
                }
                if (c == 'Q')
                {
                    fl = 1;
                    if (stage == MAIN)
                    {
                        pthread_join(t_list[0], NULL);
                        endwin();
                        done = 1;
                        return;
                    }
                }
                if (c == 'q')
                {
                    fl = 1;
                    if (stage == LIST)
                    {
                        stage = MAIN;
                        pthread_join(t_list[1], NULL);
                        pthread_create(&t_list[0], NULL, main_menu, NULL);
                        fl = 0;
                    }
                    else if (stage == ACCOUNT)
                    {

                    }
                }
                pthread_mutex_unlock(&mx);
            
        }
    /* place a new request */
    aio_read(&kbcbuf);
}

void set_signal() {
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
}


void color_setup() {
    start_color();
    init_pair(0, COLOR_WHITE, COLOR_BLACK);
    init_pair(1, COLOR_RED, COLOR_RED);
    init_pair(2, COLOR_GREEN, COLOR_GREEN);
    init_pair(3, COLOR_YELLOW, COLOR_YELLOW);
    init_pair(4, COLOR_BLUE, COLOR_BLUE);
    init_pair(5, COLOR_MAGENTA, COLOR_MAGENTA);
    init_pair(6, COLOR_CYAN, COLOR_CYAN);
    init_pair(7, COLOR_WHITE, COLOR_WHITE);
    init_pair(8, COLOR_GREEN, COLOR_BLACK);
    init_pair(9, COLOR_BLUE, COLOR_BLACK);


}

void empty_setup() {
    // 공백 종류 0개, 1개 ... n개
    int max_empty = 12;

    empty = (char**)malloc(sizeof(char*) * max_empty);
    for (int i = 0; i < max_empty; i++)
    {
        empty[i] = (char*)malloc(sizeof(char) * max_empty + 1);
        strcpy(empty[i], "\0");
        for (int j = 0; j < i; j++)
        {
            strcat(empty[i], " \0");
        }

    }
}

void start() {

    // ncurses 는 동일한 window 에서 작성 -> 같은 자원을 공유
    // 따라서 모든 refresh, addstr 등의 작업시에 mutex exclusive 가 보장되어야함
    pthread_create(&t_list[0], NULL, main_menu, NULL);
    while( !done ){
        pause();
    };
    // 입력 받는 부분
    
}

void* main_menu(void* arg)
{
    pthread_mutex_lock(&mx);
    int stout;
    clear();
    key_col = 0;
    key_row = 0;
    // LOGO
    //S
    attron(COLOR_PAIR(1));
    mvprintw(LOGOROW + 0, LOGOCOL + 1, empty[4]);
    mvprintw(LOGOROW + 1, LOGOCOL + 0, empty[1]);
    mvprintw(LOGOROW + 2, LOGOCOL + 1, empty[3]);
    mvprintw(LOGOROW + 3, LOGOCOL + 4, empty[1]);
    mvprintw(LOGOROW + 4, LOGOCOL + 0, empty[4]);

    // T
    attron(COLOR_PAIR(2));
    mvprintw(LOGOROW + 0, LOGOCOL + 5, empty[5]);
    for (int i = 0; i < 5; i++)
    {
        mvprintw(LOGOROW + i, LOGOCOL + 4 + 3, empty[1]);
    }

    // O
    attron(COLOR_PAIR(3));
    mvprintw(LOGOROW + 0, LOGOCOL + 9 + 2, empty[3]);
    mvprintw(LOGOROW + 1, LOGOCOL + 9 + 1, empty[2]);mvprintw(LOGOROW + 1, LOGOCOL + 9 + 4, empty[2]);
    mvprintw(LOGOROW + 2, LOGOCOL + 9 + 1, empty[1]);mvprintw(LOGOROW + 2, LOGOCOL + 9 + 5, empty[1]);
    mvprintw(LOGOROW + 3, LOGOCOL + 9 + 1, empty[2]);mvprintw(LOGOROW + 3, LOGOCOL + 9 + 4, empty[2]);
    mvprintw(LOGOROW + 4, LOGOCOL + 9 + 2, empty[3]);

    // C
    attron(COLOR_PAIR(4));
    mvprintw(LOGOROW + 0, LOGOCOL + 14 + 3, empty[3]);
    mvprintw(LOGOROW + 1, LOGOCOL + 14 + 2, empty[1]);
    mvprintw(LOGOROW + 2, LOGOCOL + 14 + 1, empty[2]);
    mvprintw(LOGOROW + 3, LOGOCOL + 14 + 2, empty[1]);
    mvprintw(LOGOROW + 4, LOGOCOL + 14 + 3, empty[3]);

    // K
    attron(COLOR_PAIR(5));
    for (int i = 0; i < 5; i++)
    {
        mvprintw(LOGOROW + i, LOGOCOL + 19 + 1, empty[1]);
    }
    mvprintw(LOGOROW + 0, LOGOCOL + 19 + 4, empty[2]);
    mvprintw(LOGOROW + 1, LOGOCOL + 19 + 3, empty[2]);
    mvprintw(LOGOROW + 2, LOGOCOL + 19 + 2, empty[2]);
    mvprintw(LOGOROW + 3, LOGOCOL + 19 + 3, empty[2]);
    mvprintw(LOGOROW + 4, LOGOCOL + 19 + 4, empty[2]);



    // MENU
    char* menu_list[3];
    menu_list[0] = "Stock List";
    menu_list[1] = "My Account";
    menu_list[2] = "How to play";
    move(MENUROW, MENUCOL);
    standout();
    addstr(menu_list[0]);

    move(MENUROW + 1, MENUCOL);
    standend();
    addstr(menu_list[1]);

    move(MENUROW + 2, MENUCOL);
    addstr(menu_list[2]);

    while (1)
    {
        stout = key_row;
        for (int i = 0; i < 3; i++)
        {
            if (i == stout)
            {
                standout();
            }
            mvprintw(MENUROW + i, MENUCOL, menu_list[i]);
            standend();
        }
        refresh();
        if (fl == 1)
        {
            pthread_mutex_unlock(&mx);
            break;
        }
        pthread_mutex_unlock(&mx);
    }
    return NULL;
}

void* stock_list(void* arg)
{
    clear();
    key_row = 0;
    key_col = 0;
    amount = 0;

    update_stock(1);
    signal(SIGALRM, update_stock);
    set_ticker(1000);
    //처음 위치

    // 입력 받는 부분
    char c;
    int arr;
    int buy_sell;
    char* action[3];
    action[0] = "  BUY  ";
    action[1] = " SELL ";
    action[2] = "SELLALL";

    int h = 0;

    while (1)     {
        pthread_mutex_lock(&mx);

        //draw company list
        list_comp();

        arr = key_row;
        buy_sell = key_col;

        for (int i = 0; i < comp_max; i++)
        {
            if (i == arr)
            {
                mvprintw(LOGOROW+1 + i, MAXW/2 - 30 , ">");
            }
            else
            {
                mvprintw(LOGOROW+1 + i, MAXW/2 - 30 , empty[1]);
            }
        }

        for (int i = 0; i < 3; i++)
        {
            if (i == buy_sell)
            {
                standout();
            }
            mvprintw(MENUROW +2, MENUCOL +4 + 9 * i, action[i]);
            standend();
        }
        refresh();

        if (fl == 1)
        {
            pthread_mutex_unlock(&mx);
            break;
        }

        pthread_mutex_unlock(&mx);
    }
    return NULL;
}

void list_comp() {
    // printw 류는 동일한 자원에 접근함
    char comp_info[100];
    char table[100];
    
    attron(COLOR_PAIR(8)); 
    sprintf(table, "%10s   %10s %12s %12s","COMPANY", "PRICE", "MY STOCKS", "FLUCTUATION");
    mvprintw(LOGOROW-1 , MAXW/2 - 25, table);
    standend();

    char *per = "%%";
    for (int i = 0; i < comp_max; i++)
    {   
        if( i == key_row )
            standout();
        sprintf(comp_info, "%10s  %10d KRW %9d %10.1f %s",company_list[i].name, company_list[i].value, user.stock[i], rise_rate[i]*100,per);
        mvprintw(LOGOROW+1 + i, MAXW/2 - 25, comp_info);
        standend();
    }

    char m_s[30];
    char amt[20];
    char c_m[30];
    int c_money = user.money;

    sprintf(m_s, "%s %s %d ", "My Money : ", "KRW", user.money );
    for( int i = 0 ; i < comp_max ; i++ )
    {
        c_money += user.stock[i]*company_list[i].value;
    }
    sprintf(c_m, "Current asset :  KRW %d ", c_money );
    sprintf(amt, "%2d", amount);
    strcat(amt, " stocks");

    mvprintw(MENUROW + 2, MENUCOL - 7, empty[10]);
    attron(COLOR_PAIR(9));
    mvprintw(MENUROW + 2, MENUCOL - 7, amt);
    standend();
    mvprintw(MENUROW +3, MENUCOL + 6, m_s);
    mvprintw(MENUROW+ + 4, MENUCOL + 1, c_m);

}

void update_stock(int signum) {
    req_comp_list();
}

int set_ticker(int n_msecs) {
    struct itimerval new_timeset;
    long n_sec, n_usecs;

    n_sec = n_msecs / 1000;
    n_usecs = (n_msecs % 1000) * 1000L;

    new_timeset.it_interval.tv_sec = n_sec;
    new_timeset.it_interval.tv_usec = n_usecs;
    new_timeset.it_value.tv_sec = n_sec;
    new_timeset.it_value.tv_usec = n_usecs;

    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}

void buy(int num, int amount) {

    char c;
    char response[100];
    char *msg;
    char amt[20];
    int is_ok;
    struct company* comp = &company_list[num];

    msg = (char *)malloc(sizeof(char)*BUFSIZ);
    sprintf(msg,"%s %s %d","BUY", comp->name, amount);
    write(sock_id, msg, strlen(msg) + 1);
    free(msg);
    if ((read(sock_id, &is_ok, sizeof(is_ok))) == -1)     {
        err_msg("Error : read()");
        return;
    }

    // server 와 동기화 한번 하고
    pthread_mutex_lock(&mx);
    req_account_info(); //USER

    // client가 돈이 없을 때
    if (!is_ok)     {
        clear();
        oops("No Money");
        mvprintw(LOGOROW+11, LOGOCOL, "Press Any Key");
        refresh();
        while (getchar() == -1)         {

        }
        clear();
        return;
    }
    
}

void sell(int num, int amount) {

    char c;
    char response[100];
    char msg[BUFSIZ];
    char amt[20];
    int is_ok;
    struct company* comp = &company_list[num];
    if( amount == 11 )
    {
        amount = user.stock[num];
    }

    sprintf(msg,"%s %s %d","SELL", comp->name, amount);
    // SELL NAME AMT
    strcat(msg, amt);
    write(sock_id, msg, strlen(msg) + 1);
    if ((read(sock_id, &is_ok, sizeof(is_ok))) == -1)     {
        err_msg("Error : read()");
        return;
    }

    // server 와 동기화 한번 하고
    pthread_mutex_lock(&mx);
    req_account_info(); //USER

    // client의 주식 보유 수량 없을 때
    if (!is_ok)     {
        clear();
        oops("No Stock");
        mvprintw(LOGOROW + 11, LOGOCOL, "Press Any Key");
        refresh();
        while (getchar() == -1)
        {

        }
        return;
    }
}

void account_info() {
    pthread_mutex_lock(&mx);
    req_account_info();
    req_comp_list();

    clear();

    char value[20];
    char u_n[50];

    sprintf(u_n,"Username : %15s",user.username);
    mvprintw(MAXH/2, 5, u_n);
    for (int i = 0; i < comp_max; i++)     {
        mvprintw(MAXH/2+1, 5+i*10, company_list[i].name);
        sprintf(value, "%d", user.stock[i]);
        mvprintw(MAXH/2+2, 5+i*10, value);
    }
    sprintf(u_n, "%s %s %d ", "My Money : ", "KRW", user.money );
    mvprintw(MAXH/2+3, 5, u_n);
    refresh();
    while (getchar() == -1)
    {

    }
    return;
}

void helpme() {
    pthread_mutex_lock(&mx);
    clear();

    mvprintw(LOGOROW + 6, LOGOCOL - 12, "This is Stock Simulator");
    mvprintw(LOGOROW + 7, LOGOCOL - 12, "w : up");
    mvprintw(LOGOROW + 8, LOGOCOL - 12, "a : left");
    mvprintw(LOGOROW + 9, LOGOCOL - 12, "s : down");
    mvprintw(LOGOROW + 10, LOGOCOL - 12, "d : right");
    mvprintw(LOGOROW + 11, LOGOCOL - 12, "q : go back");
    mvprintw(LOGOROW + 12, LOGOCOL - 12, "+ : amount up");
    mvprintw(LOGOROW + 13, LOGOCOL - 12, "- : amount down");
    mvprintw(LOGOROW + 14, LOGOCOL - 12, "Q : exit(main menu)");
    mvprintw(LOGOROW + 15, LOGOCOL - 12, "Enter : enter");
    refresh();
    while (getchar() == -1)
    {

    }
    return;
}