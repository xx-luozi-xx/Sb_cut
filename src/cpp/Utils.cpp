/**
 * @file Utils.cpp
 * @author xx-luozi-xx
 * @brief This file contains some utility functions.
 * @version 0.1
 * @date 2024-10-04
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "Utils.h"

#include <chrono>   // for timestamp
#include <ctime>    // for localtime
#include <iomanip>  // for put_time

using namespace std;

// Get current timestamp in string format.
string get_timestamp(){
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&in_time_t), "%Y-%m-%d %X");
    return ss.str();
}