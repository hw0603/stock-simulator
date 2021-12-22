#include <stdio.h>


char *a[2];
char *ab = "hello";


void main(){
    char *d = ab;
    a[0] = ab;
    printf("%s", a[0]);
}