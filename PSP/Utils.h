#pragma once

#include <random>
#include <chrono>
#include <thread>
#include <cstdint>

// Delay tarp žinučių (ms)
constexpr int MSG_DELAY_MS = 1200;

extern std::mt19937 rng;

void pauseMsg(int ms = MSG_DELAY_MS);
int randInt(int a, int b);
double rand01();
int clampi(int v, int lo, int hi);
double clampd(double v, double lo, double hi);
std::uint64_t mix64(std::uint64_t x);
