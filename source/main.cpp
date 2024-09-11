/*
//Heavily based on Wii Network Config Editor by Bazmoc (2023) https://github.com/Bazmoc/Wii-Network-Config-Editor/


*/

#include <fat.h>
#include <gccore.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ogc/machine/processor.h>
#include <ogcsys.h>
#include <locale.h>
#include <wiiuse/wpad.h>
#include <stdlib.h>
#include <ogc/isfs.h>
#include <fat.h>
#include <network.h>
#include <time.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>


#include <iostream>
#include <dirent.h>
#include <list>


#include "length2hex.h"
#include "isfs_readwrite.h"

//network configuration file path and length :
#define ISFS_CONFIGDAT_PATH "/shared2/sys/net/02/config.dat"
#define CONFIGDAT_FILELENGTH 7004


static void *xfb = NULL;
static GXRModeObj *rmode = NULL;


int SSID1_pos=1996, PASSWORD1_pos=2040; //location of the first ssid and first password in the file config.dat 

//those characters determine which connection is selected. 
int connection1_selectpos = 8, connection2_selectpos = 8+2332, connection3_selectpos = 8+2332+2332;//like I said before, 2332 is the number of byte in 1 connection

int connection1_securitypos = 2033; //security type position (connection 1 only)

char connection_wireless_selectedchar = 0xA6, connection_wireless_unselectedchar = 0x26; //when a wifi connection is selected the character is 0xA6, if it's not selected but is wifi, it's 0x26
char connection_wired_selectedchar = 0xA7, connection_wired_unselectedchar = 0x27; //same thing but for a wired connection


u8 *confBuffer = NULL;
u32 confSize = 0;

void exitprogram(int e){
	ISFS_Deinitialize();
	exit(e);
}



int main(int argc, char** argv) {
    // Initialise the video system
    VIDEO_Init();

    // This function initialises the attached controllers
    WPAD_Init();
    PAD_Init();

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Initialise the console, required for printf
    console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

    // Set up the video registers with the chosen mode
    VIDEO_Configure(rmode);

    // Tell the video hardware where our display memory is
    VIDEO_SetNextFramebuffer(xfb);

    // Make the display visible
    VIDEO_SetBlack(FALSE);

    // Flush the video register changes to the hardware
    VIDEO_Flush();

    // Wait for Video setup to complete
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();


    //Initialize ISFS for NAND Access
    ISFS_Initialize();

    char filepath[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);

    sprintf(filepath, ISFS_CONFIGDAT_PATH);

    //read current networksettings from nand into confBuffer
    confSize=0;
    confBuffer = ISFS_GetFile((u8*)&filepath, &confSize, -1);
    if(confSize == 0 || confBuffer == NULL) exitprogram(0); //error when reading ISFS ---> we exit immediately


    if(confBuffer[8] == 0x27 || confBuffer[8] == 0x26)
        printf("Info: connection 1 is currently not selected,just saving will cause it to become the selected connection\n");
    if (confBuffer[8]==0x27)
        confBuffer[8]=0xA7; //==if network one is wired but not selected, make it wired AND selected
    if (confBuffer[8]==0x26)
        confBuffer[8]=0xA6; //if network one is wireless but not selected, make it wireless AND selected

    //print current settings for the first connection through console
    printf("Current connection 1 settings:\n");

    if(confBuffer[8]==0xA7) {
        printf("\tType: Wired\n");
    }
    else {
        char connection1_securitytype = confBuffer[connection1_securitypos];
        char* sec = NULL;

        switch(connection1_securitytype) {
            case(0x01):
                sec = "WEP64";
                break;
            case(0x02):
                sec = "WEP128";
                break;
            case(0x04):
                sec = "WPA-PSK (TKIP)";
                break;
            case(0x05):
                sec = "WPA2-PSK (AES)";
                break;
            case(0x06):
                sec = "WPA-PSK (AES)";
                break;
            case(0x03): //apparently unused wtf?
                sec = "Error";
                break;
            default:
                sec = "Open";
                break;
        }

        char* ssidString = (char*) malloc(32);
        char* pwString = (char*) malloc(62);
        sprintf(ssidString, "%s", confBuffer+SSID1_pos);
        sprintf(pwString, "%s", confBuffer+PASSWORD1_pos);

        printf("\tType: Wireless\n\tSSID: %s\n\tSecurity: %s\n\tPassword: %s\n", ssidString,sec, pwString);
    }

    //Prompt user for action
    printf("Press\nA to read wifi settings from USB\nB to read wifi settings from SD\nX to set Network to wired\nStart to save & exit\nZ to exit without saving\n\n");
    //Input Loop
    int menu = 0;
    FILE* ptr = NULL;
    std::list<char*> content;
    std::_List_iterator<char *> iter;
    char* path = (char*) malloc(260);

    while(1) {
        // Call PAD_ScanPads each frame, this reads the latest controller states
        PAD_ScanPads();
        u32 pressed_gc = PAD_ButtonsDown(0);

        switch (menu) {
            case 0://main selection
                //Set 1st connection as wired & active
                if (pressed_gc & PAD_BUTTON_X) {
                    confBuffer[4]=0x02;
                    confBuffer[8]=0xA7;
                    printf("Connection 1 set as wired, save pending.\n Press Start to save or Z to cancel\n\n");
                    menu = 3;
                }
                //Initialize Filesystem and switch to selection
                if((pressed_gc & PAD_BUTTON_A) || (pressed_gc & PAD_BUTTON_B)) {
                    if (!fatInitDefault()) {
                        printf("Unable to initialise FAT subsystem, exiting.\n");
                        exitprogram(0);
                    }

                    if (pressed_gc & PAD_BUTTON_A) {
                        if (!fatMountSimple("usb", &__io_usbstorage)) {
                            printf("Unable to mount USB Drive, exiting.\n");
                            exitprogram(0);
                        }
                    } else {
                        if (!fatMountSimple("usb", &__io_wiisd)) {
                            printf("Unable to mount SD Drive, exiting.\n");
                            exitprogram(0);
                        }
                    }

                    //Populating file list
                    if (auto dir = opendir("usb:/wifi/")) {
                        while (auto f = readdir(dir)) {
                            if (!f->d_name || f->d_name[0] == '.')
                                continue; // Skip everything that starts with a dot
                            char* temp = (char*) malloc(260);
                            strcpy(temp,f->d_name);
                            content.push_back(temp);
                        }
                        closedir(dir);
                    }
                    iter = content.begin();
                    //If the given dir is empty
                    if(iter == content.end()) {
                        printf("No files found, please check your drive or select a different option.\33[10;0H");
                    } else {
                        printf("Select a file with the dpad\n");
                        std::cout << '<' << *iter << '>';
                        menu = 1;
                    }
                }
                break;
            case 1: //File selection
                if((pressed_gc & PAD_BUTTON_LEFT) ) {
                    printf("\n\33[2K\33[1A\r");
                    if(iter != content.begin())
                        iter.operator--();
                    std::cout << '<' << *iter << '>';
                }
                if((pressed_gc & PAD_BUTTON_RIGHT) ) {
                    printf("\n\33[2K\33[1A\r");
                    iter.operator++();
                    if(iter == content.end())
                        iter.operator--();
                    std::cout << '<' << *iter << '>';
                }

                //confirm file selection, print out file information
                if((pressed_gc & PAD_BUTTON_A)) {
                    sprintf(path,"usb:/wifi/%s",*iter);

                    ptr = fopen(path, "r");

                    if (ptr != NULL) {
                        bool err = 0;

                        char ssid[33];
                        char pw[63];

                        char *line1;
                        char *line2;

                        line1 = fgets(ssid, 33, ptr);
                        if(line1 != NULL) {
                            line2 = fgets(pw, 63, ptr);
                            line1[strcspn(line1, "\r\n")] = 0;
                            line2[strcspn(line2, "\r\n")] = 0;

                        }

                        fclose(ptr);
                        if (line1 == NULL || line2 == NULL) {
                            printf("\nERROR: network information couldn't be parsed from selected file\33[10;0H");

                        } else {
                            printf("\n\n\33[2K\33[1A\rRead from file, press A to confirm, or B to abort\n\tSSID:%s\n\tPW:%s\n", line1, line2);
                            menu = 2;
                        }
                    } else {
                        printf("file couldn't be opened\n");
                    }

                }
                if((pressed_gc & PAD_BUTTON_B)) {
                    printf("\33[10;0H\33[0J");
                    fatUnmount("usb:");
                    content.clear();
                    menu = 0;
                }
                break;
            case 2: //Await final confirmation
                if((pressed_gc & PAD_BUTTON_A) ) {
                    ptr = fopen(path,"r");
                    if( ptr != NULL) {

                        char ssid[33];
                        char pw[63];

                        char* line1;
                        char* line2;
                        line1 = fgets(ssid, 33,ptr);
                        line2 = fgets(pw,63,ptr);

                        if(line1 == NULL || line2 == NULL) {
                            printf("Error: network information couldn't be parsed from file, returning to loader\n");
                            exitprogram(1);
                        }

                        line1[strcspn(line1, "\r\n")] = 0;
                        line2[strcspn(line2, "\r\n")] = 0;

                        //Setting the connection 1 as wireless and selected
                        confBuffer[4]=0x01;
                        confBuffer[8]=0xA6;

                        for (int i=0;i<=32;i++){  //replaces the ssid in confBuffer
                            if(i >= strlen(line1))
                                confBuffer[SSID1_pos+i] = '\0';
                            else {
                                confBuffer[SSID1_pos + i] = line1[i];
                            }
                        }
                        //change the ssid length in confBuffer
                        confBuffer[SSID1_pos+33] = lengthtoHex(strlen(line1));

                        for (int i=0;i<=62;i++){  //replaces the pw in confBuffer
                            if(i >= strlen(line2))
                                confBuffer[PASSWORD1_pos+i] = '\0';
                            else {
                                confBuffer[PASSWORD1_pos + i] = line2[i];
                            }
                        }
                        //changes the pw length in confbuffer
                        confBuffer[PASSWORD1_pos-3] = lengthtoHex(strlen(line2));

                        //Set security to WPA2 AES
                        confBuffer[connection1_securitypos] = 0x05;

                        char* ssidString = (char*) malloc(32);
                        char* pwString = (char*) malloc(62);
                        sprintf(ssidString, "%s", confBuffer+SSID1_pos);
                        sprintf(pwString, "%s", confBuffer+PASSWORD1_pos);

                        printf("New connection 1 settings:\n");
                        printf("\tType: Wireless\n\tSSID: \"%s\"\n\tSecurity: %s\n\tPassword: \"%s\"\n\tPasswordlength: %d\nPress Start to save or Z to cancel\n", ssidString,"WPA2-PSK AES", pwString,confBuffer[PASSWORD1_pos-3]);
                        fclose(ptr);
                        menu = 3;
                    } else {
                        printf("Fatal Error: File couldn't be read. Returning to loader");
                        exitprogram(0);
                    }
                }
                if((pressed_gc & PAD_BUTTON_B)) {
                    //abort & return to file selection
                    printf("\33[10;0H\33[0J");
                    fclose(ptr);
                    ptr = NULL;
                    iter = content.begin();
                    printf("Select a file with the dpad\n");
                    std::cout << '<' << *iter << '>';
                    menu = 1;
                }
                break;
            case 3: //Save Pending, other options are locked
                //Return to loader and flush confBuffer
                if ( pressed_gc & PAD_BUTTON_START ) {
                    printf("Saving settings to NAND...\n");
                    ISFS_WRITE_CONFIGDAT(confBuffer);
                    printf("Done, now returning to loader.");
                    exitprogram(0);
                }
                break;
            default:
                break;

        }

        //We can leave at any time
        //Return to loader without flushing confBuffer
        if ( pressed_gc & PAD_TRIGGER_Z ) {
            printf("\n\nCanceling, now returning to loader.");
            exitprogram(0);
        }

        // Wait for the next frame
        VIDEO_WaitVSync();
    }
}





