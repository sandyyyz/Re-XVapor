#include "types.h"
#include "user.h"

void handler(int sig);

int main() {
    sigset_t set, oldset;
    int ret;

    sig_empty_set(&set);
    sig_add_set(set, SIGUSR1);

    // Set up the signal handler
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    act.sa_mask = set;

    ret = sigaction(SIGUSR1, &act, NULL);
    if (ret < 0) {
        printf("Failed to set signal handler: %d\n", ret);
        return -1;
    }
    printf("Signal handler set for SIGUSR1\n");
    // Block the signal
    ret = rt_sigprocmask(SIG_BLOCK, &set, &oldset, sizeof(sigset_t));
    if (ret < 0) {
        printf("Failed to block signal: %d\n", ret);
        return -1;
    }
printf("Signal SIGUSR1 blocked\n");
    // Simulate a signal being sent
    kill(getpid(), SIGUSR1);
    printf("Signal SIGUSR1 sent\n");
    // Unblock the signal
    ret = rt_sigprocmask(SIG_UNBLOCK, &set, NULL, sizeof(sigset_t));
    if (ret < 0) {
        printf("Failed to unblock signal: %d\n", ret);
        return -1;
    }
    printf("Signal SIGUSR1 unblocked\n");
    // Wait for the signal to be handled
    printf("Waiting for signal...\n");
    sleep(3);
    printf("Exiting program\n");
    return 0;
}

void handler(int sig) {
    // Signal handler function
    printf("Received signal SIGUSR1\n");
}
