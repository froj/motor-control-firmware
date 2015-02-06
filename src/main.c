#include "ch.h"
#include "hal.h"
#include "uavcan_node.h"

int main(void) {
    halInit();
    chSysInit();

    can_transceiver_activate();

    uavcan_node_start(NULL);

    while (1) {
        palSetPad(GPIOA, GPIOA_LED);
        chThdSleepMilliseconds(500);
        palClearPad(GPIOA, GPIOA_LED);
        chThdSleepMilliseconds(500);
    }
}
