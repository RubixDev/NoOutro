/*---------------------------------------------------------------------------------

        default ARM7 core

                Copyright (C) 2005 - 2010
                Michael Noland (joat)
                Jason Rogers (dovoto)
                Dave Murphy (WinterMute)

        This software is provided 'as-is', without any express or implied
        warranty.  In no event will the authors be held liable for any
        damages arising from the use of this software.

        Permission is granted to anyone to use this software for any
        purpose, including commercial applications, and to alter it and
        redistribute it freely, subject to the following restrictions:

        1.	The origin of this software must not be misrepresented; you
                must not claim that you wrote the original software. If you use
                this software in a product, an acknowledgment in the product
                documentation would be appreciated but is not required.

        2.	Altered source versions must be plainly marked as such, and
                must not be misrepresented as being the original software.

        3.	This notice may not be removed or altered from any source
                distribution.

---------------------------------------------------------------------------------*/

/*******************************************
 * Modified to have fifo checking for I2C. *
 * -Pk11, 2020                             *
 *                                         *
 * Switched to dsiwifi. -Pk11, 2022        *
 ******************************************/

#include "i2c_handler.h"

#include <dsiwifi7.h>
#include <nds.h>

extern void wifi_card_deinit(void);

void VblankHandler(void) {}

void VcountHandler() { inputGetAndSend(); }

volatile bool exitflag = false;

void powerButtonCB() { exitflag = true; }

int main() {
    readUserSettings();
    ledBlink(0);

    irqInit();
    // Start the RTC tracking IRQ
    initClockIRQ();
    fifoInit();
    touchInit();

    SetYtrigger(80);

    installWifiFIFO();

    installSystemFIFO();

    irqSet(IRQ_VCOUNT, VcountHandler);
    irqSet(IRQ_VBLANK, VblankHandler);

    irqEnable(IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

    setPowerButtonCB(powerButtonCB);

    fifoSetValue32Handler(FIFO_CAMERA, i2cFifoHandler, NULL);

    // Keep the ARM7 mostly idle
    while (!exitflag) {
        if (0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
            exitflag = true;
        }
        swiWaitForVBlank();
    }

    wifi_card_deinit();

    return 0;
}
