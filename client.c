#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>

#define maxW 160
#define maxH 40


void color_setup();


int main()
{
    initscr();
    move(10,10);
    color_setup();
    for( int i=0 ; i < 8 ; i++ )
    {
        for( int j=0 ; j < 10 ; j++ )
        {
            
            move(i,j);
            // attron(COLOR_PAIR(i));
            printw("       ");
            refresh();
    
            
        }
    }
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



    // init_pair(2, COLOR_RED, COLOR_RED);  
    // attron(COLOR_PAIR(2));
	// addstr("  ");
    // refresh();
    // sleep(5);