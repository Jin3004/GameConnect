#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "Header.h"

int main() {

  Connection c(5000);
  int val = 20;
  std::cin >> val;
  Data data;
  *(int*)data = val;
  try {
	c.Send(data, sizeof data, getIP(), 3000);
  }
  catch (const std::exception & e) {
	std::cout << e.what() << std::endl;
  }
  return 0;
}