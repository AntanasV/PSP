#include "Utils.h"

#include <algorithm>

using namespace std;

// RNG (globalus)
std::mt19937 rng(
    (uint32_t)chrono::high_resolution_clock::now().time_since_epoch().count()
);

void pauseMsg(int ms) {
    this_thread::sleep_for(chrono::milliseconds(ms));
}

int randInt(int a, int b) {
    uniform_int_distribution<int> d(a, b);
    return d(rng);
}

double rand01() {
    uniform_real_distribution<double> d(0.0, 1.0);
    return d(rng);
}

int clampi(int v, int lo, int hi) {
    return max(lo, min(hi, v));
}

double clampd(double v, double lo, double hi) {
    return max(lo, min(hi, v));
}

std::uint64_t mix64(std::uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}
