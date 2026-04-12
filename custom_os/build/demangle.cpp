#include <cmath>
#include <cstdlib>
#include <array>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cxxabi.h>
#include <iostream>

int main() {
    const size_t BUFSIZE = 4096;
	char* buffer = static_cast<char*>(std::malloc(BUFSIZE));

    std::string s;
    while (std::getline(std::cin, s)) {
        int status = 0;
        size_t sz = BUFSIZE - 1;
        const char* name = abi::__cxa_demangle(s.c_str(), buffer, &sz, &status);
        std::cout << -status << "|";
        if (status >= 0) {
            std::cout << buffer;
        } else {
            std::cout << s.c_str();
        }
        std::cout << std::endl;
    }

	std::free(buffer);

	return 0;
}
