#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>

#define MAXW 180
#define MAXH 40
#define WORDSIZE 5

#define LOGOSIZE 5
#define LOGOROW 1
#define LOGOCOL (MAXW/2 - 12)  

#define MENUROW 20
#define MENUCOL (MAXW/2 - 5)  

char **empty;


void color_setup();
void empty_setup();
void start();

void set_crmode(void);
void set_nodelay_mode(void);
void tty_mode(int);

int main()
{
    tty_mode(0);
    set_nodelay_mode();
	set_crmode();
    
    
    initscr();
    
    color_setup();
    empty_setup();

    start();
    
    sleep(10);
    
    endwin();


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


void start()
{
    clear();
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
    refresh();

    //game start, game end
	move(MENUROW,MENUCOL);
	standout();
	addstr("Stock List");
	int cnt = MENUROW;

	move(MENUROW+1, MENUCOL);
	standend();
	addstr("My Account");

	move(MENUROW+2, MENUCOL);
	addstr("How to play");

	

	move(30, 100);
	refresh();

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

    // init_pair(2, COLOR_RED, COLOR_RED);  
    // attron(COLOR_PAIR(2));
	// addstr("  ");
    // refresh();
    // sleep(5);