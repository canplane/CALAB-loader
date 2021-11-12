#include "branch.c"



#include <unistd.h>

int main() {
  int *zero = NULL;
  return_to_loader(*zero);   // return *zero;
  // int y[500];
  // int x[500];
  // x[501] += 1;
}
