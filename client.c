#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <pthread.h>

#include <sys/time.h>


#define MAXW 180
#define MAXH 40
#define WORDSIZE 5

#define LOGOSIZE 5
#define LOGOROW 1
#define LOGOCOL (MAXW/2 - 12)  

#define MENUROW 15
#define MENUCOL (MAXW/2 - 5)  

#define HOSTLEN 256

#define STOCKS 5

#define oops(msg) {mvprintw(MAXH/2, MAXW/2, msg); refresh();}

// user가 어느 화면에 있는지
enum states { MAIN, LIST, ACCOUNT, HELP };

struct company 
{
    char name[20];
    int value;
    int stdev;
} company;
struct company company_list[BUFSIZ];
int comp_max = 0;

struct User
{
    int money;
    int stock[STOCKS];
};

struct User user;



char **empty;
int port;
char *hostname;

// 키보드 위치
int key_row = 0;
int key_col = 0;
int amount = 0;
// socket - server
int sock_id;

int fl = 0; 

// screen settings
void color_setup();
void empty_setup();


// I/O settings
void set_crmode(void);
void set_nodelay_mode(void);
void tty_mode(int);
void set_signal();

//  screens
void start();

// animate
void *startwin(void *);
void *stock_list(void *);
void account_info();
void helpme();


void req_comp_list();
void list_comp();
int set_ticker( int );

void buy(struct company *, int );
void sell(struct company *, int );


pthread_mutex_t mx= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t flag= PTHREAD_COND_INITIALIZER;
int main(int ac, char*av[])
{
    
    char c;
    hostname = av[1];
    port = atoi(av[2]);

    // reset user
    user.money = 100000;
    for( int i = 0 ; i < STOCKS ; i++ )
        user.stock[i] = 0;

    tty_mode(0);
    set_nodelay_mode();
	set_crmode();
    set_signal();


    initscr();
    
    color_setup();
    empty_setup();

    start();
    
    
    endwin();
    tty_mode(1);


}






void color_setup()
{
    start_color();
    init_pair(0, COLOR_WHITE, COLOR_BLACK);
	init_pair(1, COLOR_RED, COLOR_RED);  
	init_pair(2, COLOR_GREEN, COLOR_GREEN);
	init_pair(3, COLOR_YELLOW, COLOR_YELLOW);
	init_pair(4, COLOR_BLUE, COLOR_BLUE);
	init_pair(5, COLOR_MAGENTA, COLOR_MAGENTA);
	init_pair(6, COLOR_CYAN, COLOR_CYAN);
	init_pair(7, COLOR_WHITE, COLOR_WHITE);
}

void empty_setup()
{
    // 공백 종류 0개, 1개 ... n개
    int max_empty = 7;

    empty = (char**)malloc(sizeof(char*)*max_empty);
    for( int i = 0 ; i < max_empty ; i++ )
    {   
        empty[i] = (char*)malloc(sizeof(char)*max_empty+1);
        strcpy(empty[i], "\0");
        for( int j = 0 ; j < i ; j++ )
        {
            strcat(empty[i]," \0");
        }

    }
}





void tty_mode(int how)
{
	static struct termios original_mode;
	static int original_flags;

	if(how == 0)
	{
		tcgetattr(0, &original_mode);
		original_flags = fcntl(0,F_GETFL);
	}
	else 
	{
		tcsetattr(0, TCSANOW, &original_mode);
		original_flags = fcntl(0, F_SETFL, original_flags);
	}
}

void set_nodelay_mode(void)
{
	int termflags;
	termflags = fcntl(0, F_GETFL);
	termflags |= O_NDELAY;
	fcntl(0, F_SETFL, termflags);
}


void set_crmode(void)
{	
	struct termios state;
	tcgetattr(0, &state);
	state.c_lflag &= ~ICANON;
	state.c_cc[VMIN] = 1;

	tcsetattr(0, TCSANOW, &state);
}

void set_signal()
{
    signal(SIGQUIT,SIG_IGN);

}

void *startwin(void *arg)
{   
    pthread_mutex_lock(&mx);
    int stout;
    clear();
    key_col = 0;
    key_row = 0;
    // LOGO
    //S
    attron(COLOR_PAIR(1));
    mvprintw(LOGOROW+0,LOGOCOL+1,empty[4]);
    mvprintw(LOGOROW+1,LOGOCOL+0,empty[1]);
    mvprintw(LOGOROW+2,LOGOCOL+1,empty[3]);
    mvprintw(LOGOROW+3,LOGOCOL+4,empty[1]);
    mvprintw(LOGOROW+4,LOGOCOL+0,empty[4]);

    // T
    attron(COLOR_PAIR(2));
    mvprintw(LOGOROW+0,LOGOCOL+5,empty[5]);
    for( int i = 0 ; i < 5 ; i ++ )
    {
        mvprintw(LOGOROW+i,LOGOCOL+4+3,empty[1]);
    }

    // O
    attron(COLOR_PAIR(3));
    mvprintw(LOGOROW+0,LOGOCOL+9+2,empty[3]);
    mvprintw(LOGOROW+1,LOGOCOL+9+1,empty[2]);mvprintw(LOGOROW+1,LOGOCOL+9+4,empty[2]);
    mvprintw(LOGOROW+2,LOGOCOL+9+1,empty[1]);mvprintw(LOGOROW+2,LOGOCOL+9+5,empty[1]);
    mvprintw(LOGOROW+3,LOGOCOL+9+1,empty[2]);mvprintw(LOGOROW+3,LOGOCOL+9+4,empty[2]);
    mvprintw(LOGOROW+4,LOGOCOL+9+2,empty[3]);

    // C
    attron(COLOR_PAIR(4));
    mvprintw(LOGOROW+0,LOGOCOL+14+3,empty[3]);
    mvprintw(LOGOROW+1,LOGOCOL+14+2,empty[1]);
    mvprintw(LOGOROW+2,LOGOCOL+14+1,empty[2]);
    mvprintw(LOGOROW+3,LOGOCOL+14+2,empty[1]);
    mvprintw(LOGOROW+4,LOGOCOL+14+3,empty[3]);

    // K
    attron(COLOR_PAIR(5));
    for( int i=0 ; i<5 ; i++ )
    {
        mvprintw(LOGOROW+i,LOGOCOL+19+1,empty[1]);
    }
    mvprintw(LOGOROW+0,LOGOCOL+19+4,empty[2]);
    mvprintw(LOGOROW+1,LOGOCOL+19+3,empty[2]);
    mvprintw(LOGOROW+2,LOGOCOL+19+2,empty[2]);
    mvprintw(LOGOROW+3,LOGOCOL+19+3,empty[2]);
    mvprintw(LOGOROW+4,LOGOCOL+19+4,empty[2]);



    // MENU
    char *menu_list[3];
    menu_list[0] = "Stock List";
    menu_list[1] = "My Account";
    menu_list[2] = "How to play";
	move(MENUROW,MENUCOL);
	standout();
	addstr(menu_list[0]);

	move(MENUROW+1, MENUCOL);
	standend();
	addstr(menu_list[1]);

	move(MENUROW+2, MENUCOL);
	addstr(menu_list[2]);
    
    while(1){
        stout = key_row;
        for( int i = 0 ; i < 3 ; i++ )
        {
            if( i == stout )
            {
                standout();
            }
            mvprintw(MENUROW+i,MENUCOL,menu_list[i]);
            standend();
        }
        refresh();
        if(fl == 1)
        {   
            pthread_mutex_unlock(&mx);
            break;
        }
        pthread_mutex_unlock(&mx);
    }
    return NULL;
}


void start()
{   
    pthread_t t_list[10];

    // ncurses 는 동일한 window 에서 작성 -> 같은 자원을 공유
    // 따라서 모든 refresh, addstr 등의 작업시에 mutex exclusive 가 보장되어야함
    pthread_mutex_lock(&mx);
    pthread_create(&t_list[0], NULL, startwin, NULL);
    // pthread_create(&t_list[1], NULL, stock_list, NULL);

    // 입력 받는 부분
    char c;
    int state = MAIN;

    while( 1 )
    {

        c = getchar();
        if( c == 's' )
            {
                if( state == MAIN && key_row == 2 )
                    continue;

                if( state == LIST && key_row == comp_max-1 )
                    continue;    
                    
                key_row++;
            }
        if( c == 'w' )
            {
                if( key_row == 0 )
                    continue;
                key_row--;
            }
        if( c == 'a' && state == LIST)
            {
                if( key_col == 0 )
                    continue;
                key_col--;
            }
        if( c == 'd' && state == LIST)
            {
                if( key_col == 1 )
                    continue;
                key_col++;
            }
        if( c == 'f' && state == LIST)
            {
                if( amount == 10 )
                    continue;
                amount++;
            }
        if( c == 'g' && state == LIST)
            {
                if( amount == 0 )
                    continue;
                amount--;
            }
        if( c == 13 )
            {
                if( state == MAIN )
                {   
                    fl = 1;
                    pthread_join(t_list[0],NULL);
                    fl = 0;
                    if( key_row == 0 )
                    {
                        // startwin 끄고 stock_list animate 키고
                        state = LIST;
                        pthread_create(&t_list[1], NULL, stock_list, NULL);
                    }
                    if( key_row == 1 )
                    {
                        // startwin 끄고 account_info animate 키고
                        state = ACCOUNT;
                        account_info();
                        pthread_create(&t_list[0], NULL, startwin, NULL);
                        state = MAIN;
                    }
                    if( key_row == 2 )
                    {
                        // startwin  끄고 stock_list animate 키고
                        state = HELP;
                        helpme();
                        pthread_create(&t_list[0], NULL, startwin, NULL);
                        state = MAIN;
                        // helpme();
                    }
                }
                else if( state == LIST )
                {
                    fl = 1;
                    pthread_join(t_list[1],NULL);
                    fl = 0;
                    if( key_col == 0 )
                    {
                        // startwin  끄고 stock_list animate 키고
                        buy(&(company_list[key_row]), amount);
                    }
                    if( key_col == 1 )
                    {
                        sell(&(company_list[key_row]), amount);
                    }
                    amount = 0;
                    key_col = 0;
                    key_row = 0;
                    pthread_create(&t_list[1], NULL, stock_list, NULL);
                }
                else if( state == ACCOUNT )
                {

                }

                refresh();
            }
        if( c == 'q' )
        {
            fl=1;
            if( state == MAIN )
                {
                    pthread_cancel(t_list[0]);
                    endwin();
                    return;
                }
                else if( state == LIST )
                {
                    state = MAIN;
                    // pthread_cancel(t_list[1]);
                    pthread_join(t_list[1],NULL);
                    pthread_create(&t_list[0], NULL, startwin, NULL);
                    fl = 0;
                }
                // else if( state == ACCOUNT )
                // {

                // }
        }
        pthread_mutex_unlock(&mx);

    }   

    

}





void list_comp()
{
    // printw 류는 동일한 자원에 접근함
    // req_comp_list();
    char value[20];
    for( int i = 0 ; i < comp_max ; i++ )
    {
        mvprintw(LOGOROW+5+i,LOGOCOL-10, company_list[i].name);
        mvprintw(LOGOROW+5+i,LOGOCOL, ":");
        sprintf(value, "%dwon", company_list[i].value);
        mvprintw(LOGOROW+5+i,LOGOCOL+2, empty[6]);
        mvprintw(LOGOROW+5+i,LOGOCOL+2, value);
    }
    
    // // comp_max++;
    // // mvprintw(LOGOROW+5+comp_max,LOGOCOL-10, company_list[comp_max].name);
    // // mvprintw(LOGOROW+5+comp_max,LOGOCOL, ":");
    // // sprintf(value, "%dwon", company_list[comp_max].value);
    // // mvprintw(LOGOROW+5+comp_max,LOGOCOL+2, value);
    // // comp_max++;
    // // mvprintw(LOGOROW+5+comp_max,LOGOCOL-10, company_list[comp_max].name);
    // // mvprintw(LOGOROW+5+comp_max,LOGOCOL, ":");
    // // sprintf(value, "%dwon", company_list[comp_max].value);
    // // mvprintw(LOGOROW+5+comp_max,LOGOCOL+2, value);
    // // comp_max++;
    // // refresh();

    char m[20];
    char m_s[20];
    char amt[20];
    strcpy(m_s,"My Money : ");
    sprintf(m,"%d",user.money);
    sprintf(amt,"%d",amount);
    strcat(m_s,m);
    mvprintw(MENUROW-2,MENUCOL+10,empty[2]);
    mvprintw(MENUROW-2,MENUCOL+10,amt);
    mvprintw(MENUROW-1,MENUCOL+10,m_s);



}

void req_comp_list()
{

    comp_max = 0;


    struct sockaddr_in addr;
    struct hostent *hp;

    char *get_msg = "GETLIST\0";
    // 회사 숫자
    
    struct company ex1;
    strcpy(ex1.name,"samsung\0");
    ex1.value = (1000 + rand())%1000;
    ex1.stdev = 0;
    company_list[comp_max] = ex1;
    comp_max++;

    struct company ex2;
    strcpy(ex2.name,"hynix\0");
    ex2.value = (1000 + rand())%1000;
    ex2.stdev = 1;
    company_list[comp_max] = ex2;
    comp_max++;

    struct company ex3;
    strcpy(ex3.name,"ynix\0");
    ex3.value = (1000 + rand())%1000;
    ex3.stdev = 2;
    company_list[comp_max] = ex3;
    comp_max++;
    // ./program snow portnum
    
    sock_id = socket(PF_INET, SOCK_STREAM, 0);

    // hp = gethostbyname(hostname);

    // bcopy((void *)&hp->h_name, (void *)&addr.sin_addr, hp->h_length);
    // addr.sin_port = htons(port);
    // addr.sin_family = AF_INET;

    // if(connect(sock_id, (struct sockaddr *)&addr, sizeof(addr)) !=0)
    //     oops("ERROR : connect");

    // if( write(sock_id, (void*)get_msg, sizeof(get_msg)) == -1 )
    // {
    //     oops("Error : write");
    // }
    // else
    // {
    //     while( read(sock_id, (void*)&(company_list[comp_max]), sizeof(company_list[comp_max])) )
    //     {
    //         mvprintw(LOGOROW+10+comp_max,LOGOCOL-10, company_list->name);
    //         mvprintw(LOGOROW+10+comp_max,LOGOCOL, ":");
    //         sprintf(value, "%dwon", company_list->value);
    //         mvprintw(LOGOROW+10+comp_max,LOGOCOL+2, value);
    //         comp_max++;
    //     }
    //     refresh();
    // }
}

void update_stock(int signum)
{
    req_comp_list();
}


int set_ticker( int n_msecs )
{
    struct itimerval new_timeset;
    long n_sec, n_usecs;

    n_sec = n_msecs / 1000;
    n_usecs = ( n_msecs%1000 ) *1000L;

    new_timeset.it_interval.tv_sec = n_sec;
    new_timeset.it_interval.tv_usec = n_usecs;
    new_timeset.it_value.tv_sec = n_sec;
    new_timeset.it_value.tv_usec = n_usecs;
    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}


void *stock_list(void *arg)
{
    clear();
    key_row = 0;
    key_col = 0;
    amount = 0;

    update_stock(1);
    signal(SIGALRM,update_stock);
    set_ticker(5000);
    //처음 위치
    
    // 입력 받는 부분
    char c;
    int arr ;
    int buy_sell;
    char *action[2];
    action[0] = "BUY";
    action[1] = "SELL";
    int h = 0;
    while( 1 )
    {
        pthread_mutex_lock(&mx);
        list_comp();
        arr = key_row;
        buy_sell = key_col;
        for( int i = 0 ; i < 3 ; i++ )
        {
            if( i == arr )
            {
                mvprintw(LOGOROW+5+i,LOGOCOL-12, ">");
            }
            else
            {
                mvprintw(LOGOROW+5+i,LOGOCOL-12, empty[1]);
            }
        }
        
        for( int i = 0 ; i < 2 ; i++ )
        {
            if( i == buy_sell )
            {
                standout();
            }
            mvprintw(MENUROW,MENUCOL+10+5*i,action[i]);
            standend();
        }
        refresh();
        if(fl == 1)
        {   
            pthread_mutex_unlock(&mx);
            break;
        }
        pthread_mutex_unlock(&mx);
    }
    return NULL;
}



void buy(struct company *comp, int amount)
{
//     char *buy_msg;
//     sprintf(buy_msg, "BUY %s %d", comp->name, amount);
    
    char c;
    char response[100];

    pthread_mutex_lock(&mx);
    // client 단에서 이미 돈이 없을 때
    if( (user.money - comp->value * amount) < 0 )
    {
        clear();
        oops("Money");
        mvprintw(MENUROW, MENUCOL, "Press Any Key");
        refresh();
        while( getchar() == -1 )
        {
            
        }
        clear();
        return;
    }
    else
    {
        // 요청
        // 성공
        user.money -= comp->value * amount;
        user.stock[comp->stdev] = user.stock[comp->stdev]+amount;
        return;
    }
    // else
    // {
    //     if( write(sock_id, (void*)buy_msg, sizeof(buy_msg)) == -1 )
    //     {
    //         oops("Error : write");
    //     }
    //     else
    //     {
    //         if ( read(sock_id, (void*)&response, 20) != -1 )
    //         {
    //             response = strtok(response, " ");
    //             if( response == "o")
    //             {
    //                 user.money -= atoi(strtok(NULL," "));
    //                 strtok(NULL," "); // 비우기
    //             }
    //             else
    //                 clear(); oops("Money"); 
    //         }
    //         refresh();
    //     }
    // }
 
    
    

}

void sell(struct company *comp, int amount)
{
    //     char *buy_msg;
    //     sprintf(buy_msg, "BUY %s %d", comp->name, amount);
    
    char c;
    char response[100];

    pthread_mutex_lock(&mx);
    // client 단에서 보유 수량 없을 때
    if(user.stock[comp->stdev] < amount)
    {
        clear();
        oops("No Stock");
        mvprintw(MENUROW, MENUCOL, "Press Any Key");
        refresh();
        while( getchar() == -1 )
        {
            
        }
        return;
    }
    else
    {
        // 요청
        // 성공
        user.money += comp->value * amount;
        user.stock[comp->stdev] = user.stock[comp->stdev]-amount;
        return;
    }
    // else
    // {
    //     if( write(sock_id, (void*)buy_msg, sizeof(buy_msg)) == -1 )
    //     {
    //         oops("Error : write");
    //     }
    //     else
    //     {
    //         if ( read(sock_id, (void*)&response, 20) != -1 )
    //         {
    //             response = strtok(response, " ");
    //             if( response == "o")
    //             {
    //                 user.money -= atoi(strtok(NULL," "));
    //                 strtok(NULL," "); // 비우기
    //             }
    //             else
    //                 clear(); oops("Money"); 
    //         }
    //         refresh();
    //     }
    // }

}


void account_info()
{
    pthread_mutex_lock(&mx);
    clear();

    char value[20];

    mvprintw(LOGOROW+6,LOGOCOL-12, "username : test");
    for( int i = 0 ; i < comp_max ; i++ )
    {
        mvprintw(LOGOROW+7,LOGOCOL-12+10*i, company_list[i].name);
        sprintf(value,"%d",user.stock[i]);
        mvprintw(LOGOROW+8,LOGOCOL-12+10*i, value);
    }
    
    refresh();
    while( getchar() == -1 )
    {
        
    }
    return;
}

void helpme()
{
    pthread_mutex_lock(&mx);
    clear();

    mvprintw(LOGOROW+6,LOGOCOL-12, "This is Stock Simulator");
    mvprintw(LOGOROW+7,LOGOCOL-12, "w : up");
    mvprintw(LOGOROW+8,LOGOCOL-12, "a : left");
    mvprintw(LOGOROW+9,LOGOCOL-12, "s : down");
    mvprintw(LOGOROW+10,LOGOCOL-12, "d : right");
    mvprintw(LOGOROW+11,LOGOCOL-12, "q : go back");
    mvprintw(LOGOROW+12,LOGOCOL-12, "f : amount up");
    mvprintw(LOGOROW+13,LOGOCOL-12, "g : amount down");
    mvprintw(LOGOROW+14,LOGOCOL-12, "q : go back");
    mvprintw(LOGOROW+15,LOGOCOL-12, "Enter : enter");
    refresh();
    while( getchar() == -1 )
    {
        
    }
    return;
}