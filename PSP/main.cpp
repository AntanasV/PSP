#include <iostream>
#include "Game.h"
#include "Utils.h"

using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Game g;
    pauseMsg(800);
    g.run();
    return 0;
}
