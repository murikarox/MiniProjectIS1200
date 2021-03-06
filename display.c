//
//  main.c
//
//
//  Created by Murat Ekşi on 2018-02-04.
//

#include "main.h"
#include <pic32mx.h>
#include <stdint.h>
#include "Project.h"
#include "display.h"

#define DISPLAY_CHANGE_TO_COMMAND_MODE (PORTFCLR = 0x10)
#define DISPLAY_CHANGE_TO_DATA_MODE (PORTFSET = 0x10)

#define DISPLAY_ACTIVATE_RESET (PORTGCLR = 0x200)
#define DISPLAY_DO_NOT_RESET (PORTGSET = 0x200)

#define DISPLAY_ACTIVATE_VDD (PORTFCLR = 0x40)
#define DISPLAY_ACTIVATE_VBAT (PORTFCLR = 0x20)

#define DISPLAY_TURN_OFF_VDD (PORTFSET = 0x40)
#define DISPLAY_TURN_OFF_VBAT (PORTFSET = 0x20)

//
void spi_init(void){
    SYSKEY = 0xAA996655;  /* Unlock OSCCON, step 1 */
    SYSKEY = 0x556699AA;  /* Unlock OSCCON, step 2 */
    while(OSCCON & (1 << 21)); /* Wait until PBDIV ready */
    OSCCONCLR = 0x180000; /* clear PBDIV bit <0,1> */
    while(OSCCON & (1 << 21));  /* Wait until PBDIV ready */
    SYSKEY = 0x0;  /* Lock OSCCON */
    
    /* Set up output pins */
    AD1PCFG = 0xFFFF;
    ODCE = 0x0;
    TRISECLR = 0xFF;
    PORTE = 0x0;
    
    /* Output pins for display signals */
    PORTF = 0xFFFF;
    PORTG = (1 << 9);
    ODCF = 0x0;
    ODCG = 0x0;
    TRISFCLR = 0x70;
    TRISGCLR = 0x200;
    
    /* Set up input pins */
    TRISDSET = (1 << 8);
    TRISFSET = (1 << 1);
    
    /* Set up SPI as master */
    SPI2CON = 0;
    SPI2BRG = 4;
    /* SPI2STAT bit SPIROV = 0; */
    SPI2STATCLR = 0x40;
    /* SPI2CON bit CKP = 1; */
    SPI2CONSET = 0x40;
    /* SPI2CON bit MSTEN = 1; */
    SPI2CONSET = 0x20;
    /* SPI2CON bit ON = 1; */
    SPI2CONSET = 0x8000;
}

// quick delay for screen initiliazing for timing it right to not deplete the life of the screen
void quicksleep(int cyc) {
    int i;
    for(i = cyc; i > 0; i--);
}

// Spi data sending function
uint8_t spi_send_recv(uint8_t data) {
    while(!(SPI2STAT & 0x08));
    SPI2BUF = data;
    while(!(SPI2STAT & 1));
    return SPI2BUF;
}

// Default Lab code
void display_init(void) {
    DISPLAY_CHANGE_TO_COMMAND_MODE; //clear and set to be able to send command
    quicksleep(10);
    DISPLAY_ACTIVATE_VDD; //turn on power
    quicksleep(1000000);
    //reset 0 then reset 1
    spi_send_recv(0xAE);
    DISPLAY_ACTIVATE_RESET;
    quicksleep(10);
    DISPLAY_DO_NOT_RESET;
    quicksleep(10);
    
    spi_send_recv(0x8D);
    spi_send_recv(0x14);
    
    spi_send_recv(0xD9);
    spi_send_recv(0xF1);
    //turn on vcc
    DISPLAY_ACTIVATE_VBAT;
    quicksleep(10000000);
    
    //make columns as x axis long side, make rows as y axis short side
    spi_send_recv(0xA1);
    spi_send_recv(0xC8);
    
    spi_send_recv(0xDA);
    spi_send_recv(0x20);
    
    spi_send_recv(0xAF); //Display on command
}

// Call whenever you go into game or don't change the line that has been printed before
void clear_textbuffer(void){
    int i, j;
    for(i = 0; i < 4; i++){
        for(j = 0; j < 16; j++){
            textbuffer[i][j] = 0;
        }
    }
}

// Default Lab code
//put letters in textbuffer and spaces if blank
void display_string(int line, char *s) {
    int i;
    if(line < 0 || line >= 4)
        return;
    if(!s)
        return;
    
    for(i = 0; i < 16; i++)
        if(*s) {
            textbuffer[line][i] = *s;
            s++;
        } else
            textbuffer[line][i] = ' ';
}


void display_game(int x, const uint8_t *data) {
    int i, j;
    
    for(i = 0; i < 4; i++) { //iterates through row
        DISPLAY_CHANGE_TO_COMMAND_MODE; //set to recieve command
        
        spi_send_recv(0x22); //page command
        spi_send_recv(i); //page number
        
        spi_send_recv(x & 0xF); //column low nibble
        spi_send_recv(0x10 | ((x >> 4) & 0xF)); //column high nibble
        
        DISPLAY_CHANGE_TO_DATA_MODE;
        
        if (hit == 1){ //if ship hit by enemy
            for(j = 0; j < 128; j++) //iterates through columns
                spi_send_recv(~data[i*128 + j]); //inverts the color
        } else {
            for(j = 0; j < 128; j++) //iterates through columns
                spi_send_recv(data[i*128 + j]); //put the buffer into screen
        }
        
        if (game_status == 0){ //if in menu
            for(j = 0; j < 16; j++) { //iterates through the column of letters
                c = textbuffer[i][j];
                if(c & 0x80)
                    continue;
                int k;
                for(k = 0; k < 8; k++)
                    spi_send_recv(font[c*8 + k]);
            }
        }
        
    }
}

print_ship(int x,int y,const uint8_t *data){
    
    int j = 0;
    short offset = 0;
    short junction = 0;
    int k = 0;
    if (y > 0) { offset = y / 8; } //y value
    junction =  y % 8; //how many pixels in the next page
    if (junction > 0){
        for (j = 0; j < 16; j++ ){ //iterate through ship bits width
            GAME [(offset*128 + x) + j ] |= (~data[j]) << (y - offset * 8); //if between pages show top part
            GAME [((offset + 1)*128 + x) + j ] |=  (((uint8_t)(~data[j]) >> (8 - junction)) & (power(2,(junction)))); //if between pages show bottom part
        }
    } else {
        for(j = 0; j < 16; j++)
            GAME [(offset*128 + x) + j ]|= (~data[j] << (y - offset * 8)) ; //if not between pages print it normally
    }
}

void display_bullet(int x,int y,const uint8_t *data){
    int j = 0;
    short offset = 0;
    int k = 0;
    if (y > 0) { offset = y / 8; }
    for(j = 0; j < 10; j++)
        GAME [(offset*128 + x) + j ]|= (~data[j] << (y - offset * 8)) ;
}

void game_over_print(int xPos, int yPos, const uint8_t *data, int tag){
    
    int max = 0;
    if(tag == 0)//if letter
    {
        max = 32;
    }
    if(tag == 1)//if whale
    {
        max = 58;
    }
    
    int x = 0;
    for(x = 0; x < max; x++)
    {
        if(x < (max / 2))
        {
            if(-1 < (x + xPos))//if not below array
            {
                GAME[(x + xPos) + (128 * yPos)] = data[x];
            }
        }
        else
        {
            if(-1 < (x + xPos))//if not below array
            {
                GAME[((x - (max / 2)) + xPos) + (128) + (128 * yPos)] = data[x];
            }
        }
    }
}

void mask_print(int xPos, int yPos, const uint8_t *data)
{
    GAME[xPos + (128 * yPos)] = data[0];
}

clear_screen(void){
    int i = 0;
    for(i = 0; i < 128*4; i++)
        GAME[i] = 0;
}
