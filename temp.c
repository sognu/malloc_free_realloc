#define SIZE 4
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int get_s();




int main(){

  get_s();


}



int get_s(){

  void *p = malloc(4*sizeof(int));
  int i = 1;
  int j = 2;
  int k = 3;
  int l = 4;
  memcpy(p, &i, sizeof(int));
  memcpy(p, &j, sizeof(int));
  memcpy(p, &k, sizeof(int));
  memcpy(p, &l, sizeof(int));
  p++;
  printf("%d\n", *p);
}
