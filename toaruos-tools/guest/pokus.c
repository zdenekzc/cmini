#include <stdio.h>
int main(void){
  int s = 0;
  for (int i = 1; i <= 5; i++) s += i;
  printf("s=%d\n", s);
  return 0;
}
