#pragma once
#include <chrono>
#include <iostream>
#include <string_view>

struct Timer
{
    std::string_view name;
    std::chrono::high_resolution_clock::time_point start;

    Timer(std::string_view name) : name(name), start(std::chrono::high_resolution_clock::now()) {}
    ~Timer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[Timer] " << name << ": " << duration << " ms" << std::endl; 
    }
};