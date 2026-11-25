// The greeting says hello.

import greeting;

int main() {
    return greeting::hello("x") == "hello, x" ? 0 : 1;
}
