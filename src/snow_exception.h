#pragma once
#include <iostream>
#include <exception>

class snow_exception : public std::exception {

public:
    snow_exception(const char* msg) {
        std::cerr << "ERROR:\n" << msg << std::endl;
    }
};
