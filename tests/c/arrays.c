#include <stdio.h>

int main(void) {
    int arr[5];
    for (int i = 0; i < 5; i++) arr[i] = i * i;
    for (int i = 0; i < 5; i++) printf("arr[%d] = %d\n", i, arr[i]);
    return 0;
}
