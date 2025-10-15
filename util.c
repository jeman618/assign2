#include <signal.h>
#include <stdio.h>
#include "util.h"

void install_handler(int sig, void (*handler)(int)) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    sigaction(sig, &sa, NULL);
}

void SIGINT_handler(int sig)  { (void)sig; }
void SIGQUIT_handler(int sig) { (void)sig; }
