#include "Functions.h"

#include <stdexcept>
int Add(int x, int y) {
    return x + y;
}

bool IsLeap(int year) {
  if (year <= 0) {
    throw std::invalid_argument("Year must be greater than 0");
  }

  if (year % 4 == 0) {
    if (year % 100 == 0) {
      if (year % 400 == 0) {
        return true;
      } else {
        return false;
      }
    } else {
      return true;
    }
    return true;
  }

  return false;
}


int GetMonthDays(int year, int month) {
  if (month <= 0 || month > 12) {
    throw std::out_of_range("Month should be in range [1;12]");
  }
  
  if (year == 1930) {
    return 30;
  }

  if (month == 2) {
    if (IsLeap(year)) {
        return 29;
    } else {
        return 28;
    }
  }


  if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  }
  return 31;
}

