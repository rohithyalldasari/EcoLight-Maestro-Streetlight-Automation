#include <lpc214x.h>
#include "drivers/lcd.h"
#include "drivers/delay.h"
#include "drivers/keypad.h"

// Hardware Pin Configurations
#define LED_GRID_MASK      (0xF << 16) // P0.16 to P0.19 represents street light banks
#define ALERT_BUZZER_PIN   (1 << 25)  // P0.25 drives the warning buzzer

// Operation Mode State Tracking
volatile unsigned char street_mode = 0; // 0 = Auto Day/Night Scheduler, 1 = Admin RTC Edit

void Streetlight_Init(void) {
    // Configure LEDs and Buzzer pins as GPIO Outputs
    IO0DIR |= LED_GRID_MASK | ALERT_BUZZER_PIN;
    IO0CLR = LED_GRID_MASK | ALERT_BUZZER_PIN; // Initialize all off
    
    // Start underlying physical user interface blocks
    Lcd_Init();
    Keypad_Init();
    
    // Initialize Internal Hardware Real-Time Clock (RTC)
    CCR = 0x02;     // Reset clock configuration lines
    ILR = 0x03;     // Clear internal ticks and interrupts
    
    // Calibrate startup reference time point: 17:59:00 (5:59 PM) for quick validation checking
    HOUR = 17; 
    MIN = 59; 
    SEC = 0;
    DOM = 28;
    MONTH = 5;
    YEAR = 2026;
    
    CCR = 0x01;     // Release reset and start the RTC counting engine
    
    // Configure External Interrupt 0 (EINT0 on P0.1) for Admin Calibration Menu
    PINSEL0 |= 0x0000000C;         // Set P0.1 function to EINT0 line
    EXTMODE |= 0x01;               // Configure edge-triggered tracking
    EXTPOLAR &= ~0x01;             // Set trigger to falling edge
    
    // Register EINT0 inside the Vector Interrupt Controller (Slot 2)
    VICVectAddr2 = (unsigned int)Eint0_Isr;
    VICVectCntl2 = 0x20 | 14;      // Channel 14 is EINT0, activate slot
    VICIntEnable |= (1 << 14);     // Enable global EINT0 bit lines
}

unsigned int Read_LDR_Sensor(void) {
    // Select AD0.1 analog function on pin P0.28
    PINSEL1 &= ~0x03000000;
    PINSEL1 |=  0x01000000; 
    
    // AD0CR: Channel 1, Clock Divisor = 14 (Generates standard 4MHz ADC clock from 60MHz PCLK)
    AD0CR = (1 << 1) | (14 << 8) | (1 << 21);
    
    AD0CR |= (1 << 24); // Start conversion cycle
    while (!(AD0GDR & 0x80000000)); // Poll DONE bit until conversion finishes
    
    return (AD0GDR >> 6) & 0x3FF; // Extract and return the 10-bit digital intensity scale
}

void Trigger_Buzzer(void) {
    IO0SET = ALERT_BUZZER_PIN;
    Delay_ms(200);
    IO0CLR = ALERT_BUZZER_PIN;
}

void Handle_Admin_Menu(void) {
    char key_selection = '\0';
    Lcd_Clear();
    Lcd_SetCursor(1, 1);
    Lcd_DisplayString("1) Edit RTC Info");
    Lcd_SetCursor(2, 1);
    Lcd_DisplayString("2) Exit Menu    ");
    
    while (key_selection != '1' && key_selection != '2') {
        key_selection = Keypad_GetKey();
    }
    
    if (key_selection == '1') {
        unsigned int input_hour = 0;
        Lcd_Clear();
        Lcd_SetCursor(1, 1);
        Lcd_DisplayString("Set Hour (0-23):");
        
        char digit1 = '\0', digit2 = '\0';
        while (digit1 == '\0') digit1 = Keypad_GetKey();
        Lcd_SetCursor(2, 1); Lcd_Data(digit1);
        while (digit2 == '\0') digit2 = Keypad_GetKey();
        Lcd_SetCursor(2, 2); Lcd_Data(digit2);
        
        // Parse raw ASCII characters into decimals
        input_hour = ((digit1 - '0') * 10) + (digit2 - '0');
        
        if (input_hour <= 23) {
            CCR = 0x02;        // Safely halt counter clock lines
            HOUR = input_hour; // Update hardware register values
            CCR = 0x01;        // Re-enable clock line tracking
            Lcd_Clear();
            Lcd_SetCursor(1, 1);
            Lcd_DisplayString("Time Sync Saved!");
        } else {
            Lcd_Clear();
            Lcd_SetCursor(1, 1);
            Lcd_DisplayString("Range Error!    ");
            Trigger_Buzzer();
        }
        Delay_ms(2000);
    }
    street_mode = 0; // Hand processing flow back to main automated execution loops
}

int main(void) {
    char display_buffer[16];
    unsigned int ambient_lux = 0;
    
    Streetlight_Init();
    
    while (1) {
        if (street_mode == 1) {
            Handle_Admin_Menu();
        }
        
        // Read real-time parameters out of hardware counters
        unsigned int snapshot_hr  = HOUR;
        unsigned int snapshot_min = MIN;
        unsigned int snapshot_sec = SEC;
        
        // Assemble display string layout safely
        Lcd_SetCursor(1, 1);
        display_buffer[0] = 'T'; display_buffer[1] = 'i'; display_buffer[2] = 'm'; display_buffer[3] = 'e'; display_buffer[4] = ':';
        display_buffer[5] = ' ';
        display_buffer[6] = (snapshot_hr / 10) + '0';   display_buffer[7] = (snapshot_hr % 10) + '0';   display_buffer[8] = ':';
        display_buffer[9] = (snapshot_min / 10) + '0';  display_buffer[10] = (snapshot_min % 10) + '0'; display_buffer[11] = ':';
        display_buffer[12] = (snapshot_sec / 10) + '0'; display_buffer[13] = (snapshot_sec % 10) + '0'; display_buffer[14] = '\0';
        Lcd_DisplayString(display_buffer);
        
        // Check Chronographic Boundary Rules (Active Auto Window: 18:00 to 06:00)
        if (snapshot_hr >= 18 || snapshot_hr < 6) {
            ambient_lux = Read_LDR_Sensor();
            
            Lcd_SetCursor(2, 1);
            Lcd_DisplayString("LUX: ");
            Lcd_Data((ambient_lux / 100) + '0');
            Lcd_Data(((ambient_lux % 100) / 10) + '0');
            Lcd_Data((ambient_lux % 10) + '0');
            Lcd_DisplayString(" [AUTO]");
            
            // Dynamic intensity check: trip grid lights on relative dark conditions
            if (ambient_lux < 400) { 
                IO0SET = LED_GRID_MASK; // Drive lights ON
            } else {
                IO0CLR = LED_GRID_MASK; // Sunlight detected, keep lights OFF
            }
        } else {
            // Daytime rules enforced: clear power line grid entirely to eliminate power bleed
            IO0CLR = LED_GRID_MASK;
            Lcd_SetCursor(2, 1);
            Lcd_DisplayString("GRID: OFF [DAY] ");
        }
        Delay_ms(200);
    }
}

void __irq Eint0_Isr(void) {
    street_mode = 1;     // Trip mode tracker context flag
    EXTINT |= 0x01;      // Clear active interrupt register flags for EINT0
    VICVectAddr = 0;     // Release interrupt execution engine context lines
}
