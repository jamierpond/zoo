#include "./cfs_utility.h"

#include <vector>

std::vector<int> linear_vector(int n) {
    std::vector<int> rv;
    rv.reserve(n);
    for(auto i = 0; i < n; ++i) {
        rv.push_back(i);
    }
    return rv;
}


#include <iterator>
#include <random>
#include <algorithm>

std::random_device rdevice;
std::mt19937 gen(rdevice());
std::uniform_int_distribution<int> dist(0, (1 << 30) - 1); // 16 M

int randomTwo30() {
    return dist(gen);
}

std::vector<int> makeRandomVector(int size) {
    std::vector<int> v;
    v.reserve(size);
    for(auto i = size; i--; ) {
        v.push_back(dist(gen));
    }
    return v;
}
