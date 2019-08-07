#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "Header.h"

void func() {
  return;
}

int main() {
  Connection a(func, 3000);
  return 0;
}