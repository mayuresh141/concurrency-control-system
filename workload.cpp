#include <cstdlib>
#include <string>

std::string choose_key(double p, int hotset_size, int keyspace_size) {

    double r = (double)rand() / RAND_MAX;

    if (r < p) {
        int key = rand() % hotset_size;
        return "key" + std::to_string(key);
    } 
    else {
        int key = rand() % keyspace_size;
        return "key" + std::to_string(key);
    }
}