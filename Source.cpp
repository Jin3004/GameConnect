#include <iostream>
#include "Header.h"
int main() {
  Jin::GameConnect g(3000);
  const char* data = g.Get();
  int x = *(int*)data;
  std::cout << 2 * x << std::endl;
  return 0;
}