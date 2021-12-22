#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

char username[20] = "TestUser1";
int balance = 1000000;


struct company {
    char name[20];
    int value;
    double stdev;
} company;


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
int* convert(int* valueptr, double stdev) {
    int value = *valueptr;
    double random = gaussianRandom(0.0, stdev);

    value += value * random;
    value = value > 1000 ? value : 1000;

    *valueptr = value;

    return valueptr;
}

int main() {
    srand(time(NULL));

    struct company clist[5];
    FILE* fp = fopen("stockdata.dat", "r");
    for (int i = 0; i < 5; i++) {
        fscanf(fp, "%s %d %lf", clist[i].name, &clist[i].value, &clist[i].stdev);
    }
    for (int i = 0; i < 5; i++) {
        printf("%s\t%d\t%lf\n", clist[i].name, clist[i].value, clist[i].stdev);
    }


    int idx = 1;
    printf("\n\nCalculating %s...\n", clist[idx].name);
    for (int i = 0; i < 100; i++) {
        printf("%d\n", *(convert(&clist[idx].value, clist[idx].stdev)));
    }

    

    return 0;
}