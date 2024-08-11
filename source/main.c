/* This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or
as a compiled binary, for any purpose, commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this software dedicate any and all copyright
interest in the software to the public domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an overt act of relinquishment in perpetuity
of all present and future rights to this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "ntp.h"

bool setsysInternetTimeSyncIsOn() {
    Result rs = setsysInitialize();
    if (R_FAILED(rs)) {
        printf("setsysInitialize failed, %x\n", rs);
        return false;
    }

    bool internetTimeSyncIsOn;
    rs = setsysIsUserSystemClockAutomaticCorrectionEnabled(&internetTimeSyncIsOn);
    setsysExit();
    if (R_FAILED(rs)) {
        printf("Unable to detect if Internet time sync is enabled, %x\n", rs);
        return false;
    }

    return internetTimeSyncIsOn;
}

Result enableSetsysInternetTimeSync() {
    Result rs = setsysInitialize();
    if (R_FAILED(rs)) {
        printf("setsysInitialize failed, %x\n", rs);
        return rs;
    }

    rs = setsysSetUserSystemClockAutomaticCorrectionEnabled(true);
    setsysExit();
    if (R_FAILED(rs)) {
        printf("Unable to enable Internet time sync: %x\n", rs);
    }

    return rs;
}

/*

Type   | SYNC | User | Local | Network
=======|======|======|=======|========
User   |      |      |       |
-------+------+------+-------+--------
Menu   |      |  *   |   X   |
-------+------+------+-------+--------
System |      |      |       |   X
-------+------+------+-------+--------
User   |  ON  |      |       |
-------+------+------+-------+--------
Menu   |  ON  |      |       |
-------+------+------+-------+--------
System |  ON  |  *   |   *   |   X

*/
TimeServiceType __nx_time_service_type = TimeServiceType_System;
bool setNetworkSystemClock(time_t time) {
    Result rs = timeSetCurrentTime(TimeType_NetworkSystemClock, (uint64_t)time);
    if (R_FAILED(rs)) {
        printf("timeSetCurrentTime failed with %x\n", rs);
        return false;
    }
    printf("Successfully set NetworkSystemClock.\n");
    return true;
}

int consoleExitWithMsg(char* msg, PadState* pad) {
    printf("%s\n\nPress + to quit...", msg);

    while (appletMainLoop()) {
        padUpdate(pad);
        u64 kDown = padGetButtonsDown(pad);

        if (kDown & HidNpadButton_Plus) {
            consoleExit(NULL);
            return 0;  // return to hbmenu
        }

        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}

bool toggleHBMenuPath(char* curPath, PadState* pad) {
    const char* HB_MENU_NRO_PATH = "sdmc:/hbmenu.nro";
    const char* HB_MENU_BAK_PATH = "sdmc:/hbmenu.nro.bak";
    const char* DEFAULT_RESTORE_PATH = "sdmc:/switch/switch-time.nro";

    printf("\n\n");

    Result rs;
    if (strcmp(curPath, HB_MENU_NRO_PATH) == 0) {
        // restore hbmenu
        rs = access(HB_MENU_BAK_PATH, F_OK);
        if (R_FAILED(rs)) {
            printf("could not find %s to restore. failed: 0x%x", HB_MENU_BAK_PATH, rs);
            consoleExitWithMsg("", pad);
            return false;
        }

        rs = rename(curPath, DEFAULT_RESTORE_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", curPath, DEFAULT_RESTORE_PATH, rs);
            consoleExitWithMsg("", pad);
            return false;
        }
        rs = rename(HB_MENU_BAK_PATH, HB_MENU_NRO_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", HB_MENU_BAK_PATH, HB_MENU_NRO_PATH, rs);
            consoleExitWithMsg("", pad);
            return false;
        }
    } else {
        // replace hbmenu
        rs = rename(HB_MENU_NRO_PATH, HB_MENU_BAK_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", HB_MENU_NRO_PATH, HB_MENU_BAK_PATH, rs);
            consoleExitWithMsg("", pad);
            return false;
        }
        rs = rename(curPath, HB_MENU_NRO_PATH);
        if (R_FAILED(rs)) {
            printf("fsFsRenameFile(%s, %s) failed: 0x%x", curPath, HB_MENU_NRO_PATH, rs);
            rename(HB_MENU_BAK_PATH, HB_MENU_NRO_PATH);  // hbmenu already moved, try to move it back
            consoleExitWithMsg("", pad);
            return false;
        }
    }

    printf("Quick launch toggled\n\n");

    return true;
}

int main(int argc, char* argv[]) {
    consoleInit(NULL);
    printf("SwitchTime v0.1.3\n\n");

    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeAny(&pad);

    if (!setsysInternetTimeSyncIsOn()) {
        // printf("Trying setsysSetUserSystemClockAutomaticCorrectionEnabled...\n");
        // if (R_FAILED(enableSetsysInternetTimeSync())) {
        //     return consoleExitWithMsg("Internet time sync is not enabled. Please enable it in System Settings.");
        // }
        // doesn't work without rebooting? not worth it
        return consoleExitWithMsg("Internet time sync is not enabled. Please enable it in System Settings.", &pad);
    }

    // Main loop
    while (appletMainLoop()) {
        printf(
            "\n\n"
            "Press:\n\n"
            "UP/DOWN to change hour | LEFT/RIGHT to change day | X/B to 10 minutes\n"
            "L/ZL to change month   | R/ZR to change year\n"
            "A to confirm time      | Y to reset to current time (Cloudflare time server)\n"
            "                       | + to quit\n\n\n");

        int yearsChange = 0, monthChange = 0, dayChange = 0, hourChange = 0, tenMinChange = 0;
        while (appletMainLoop()) {
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);

            if (kDown & HidNpadButton_Plus) {
                consoleExit(NULL);
                return 0;  // return to hbmenu
            }
            if (kDown & HidNpadButton_Minus) {
                if (!toggleHBMenuPath(argv[0], &pad)) {
                    return 0;
                }
            }

            time_t currentTime;
            Result rs = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
            if (R_FAILED(rs)) {
                printf("timeGetCurrentTime failed with %x", rs);
                return consoleExitWithMsg("", &pad);
            }

            struct tm* p_tm_timeToSet = localtime(&currentTime);
            p_tm_timeToSet->tm_year += yearsChange;
            p_tm_timeToSet->tm_mon += monthChange;
            p_tm_timeToSet->tm_mday += dayChange;
            p_tm_timeToSet->tm_hour += hourChange;
            p_tm_timeToSet->tm_min += tenMinChange * 10;
            time_t timeToSet = mktime(p_tm_timeToSet);

            if (kDown & HidNpadButton_A) {
                printf("\n\n\n");
                setNetworkSystemClock(timeToSet);
                break;
            }

            if (kDown & HidNpadButton_Y) {
                printf("\n\n\n");
                rs = ntpGetTime(&timeToSet);
                if (R_SUCCEEDED(rs)) {
                    setNetworkSystemClock(timeToSet);
                }
                break;
            }

            if (kDown & HidNpadButton_Left) {
                dayChange--;
            } else if (kDown & HidNpadButton_Right) {
                dayChange++;
            } else if (kDown & HidNpadButton_Down) {
                hourChange--;
            } else if (kDown & HidNpadButton_Up) {
                hourChange++;
            } else if (kDown & HidNpadButton_R) {
                yearsChange--;
            } else if (kDown & HidNpadButton_ZR) {
                yearsChange++;
            } else if (kDown & HidNpadButton_L) {
                monthChange--;
            } else if (kDown & HidNpadButton_ZL) {
                monthChange++;
            } else if (kDown & HidNpadButton_B) {
                tenMinChange--;
            } else if (kDown & HidNpadButton_X) {
                tenMinChange++;
            }

            char timeToSetStr[25];
            strftime(timeToSetStr, sizeof timeToSetStr, "%c", p_tm_timeToSet);
            printf("\rTime to set: %s", timeToSetStr);
            consoleUpdate(NULL);
        }
    }
}
