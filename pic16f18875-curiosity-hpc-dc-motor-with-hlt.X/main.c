/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.76
        Device            :  PIC16F18875
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"

/*
                         Main application
 */
#define HOLES_PER_REV 24 // holes per revolution
#define SECONDS_PER_MIN 60
#define MICROSECONDS_PER_SECOND 1000000
#define ERROR_AMPLIFIER_K 20
#define PER_STEPS_TO_MICROS_SHIFT_VAL 5

#define PWM_DUTY_MIN 0
#define PWM_DUTY_MAX 1023

typedef enum {
    MOTOR_OFF,
    MOTOR_ON
} motorState_t;

volatile int32_t period, freq, rpm;
volatile bool rpmBusy = false;
volatile motorState_t motorState;

void SMT1_PR_ACQ_ISR(void)
{
    // prevent reading the rpm value in the while loop:
    rpmBusy = true;
    LED_D4_LAT = !LED_D4_LAT; // for debug
 
    // read period from SMT:
    period = SMT1_GetCapturedPeriod();
    
    // SMT time step is set to 32 microseconds
    // convert period to microseconds:
    period = period << PER_STEPS_TO_MICROS_SHIFT_VAL; // x32

    // compute RPM (rotations per minute)
    // n = 24 (holes per circumference) in the spinning wheel
    freq = MICROSECONDS_PER_SECOND/period; // frequency in Hz
    rpm = (freq*SECONDS_PER_MIN)/HOLES_PER_REV;
    rpmBusy = false;
    // now it is safe to read the rpm value in the while loop
    
    // Disabling SMT1 period acquisition interrupt flag bit.
    PIR8bits.SMT1PRAIF = 0;  
}


void stopMotor()
{
    motorState = MOTOR_OFF;
}


void main(void)
{   
    volatile int32_t wantedRpm, rpmErr;
    
    // initialize the device
    SYSTEM_Initialize();

    // When using interrupts, you need to set the Global and Peripheral Interrupt Enable bits
    // Use the following macros to:

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();

    // Disable the Peripheral Interrupts
    //INTERRUPT_PeripheralInterruptDisable();
    
    // TMR4 with HLT is configured to interrupt
    // if there are no pulses for 50ms
    // stop motor on interrupt:
    TMR4_SetInterruptHandler(stopMotor);
    
    while (1)
    {
        // desired RPM value is set using the onboard potentiometer:
        ADCC_GetSingleConversion(channel_ANA0);    
        wantedRpm = ADCC_GetFilterValue();
        
        // wait while the ISR is updating the measured RPM value:
        while(rpmBusy)
        {
            ;
        }
        
        // compute difference:
        rpmErr = wantedRpm - rpm;
        
        // amplify x20:
        rpmErr = rpmErr * ERROR_AMPLIFIER_K;
        
        // limit to [0, 1023]:
        if(rpmErr<PWM_DUTY_MIN)
        {
            rpmErr = PWM_DUTY_MIN;
        }
        
        if(rpmErr > PWM_DUTY_MAX)
        {
            rpmErr = PWM_DUTY_MAX;
        }
        
        // On / Off switches handling:
        if(!S1_PORT)
        {
            motorState = MOTOR_ON;
            TMR4_Start();
        }
        if(!S2_PORT)
        {
            motorState = MOTOR_OFF;
            TMR4_Stop();
            TMR4_WriteTimer(0);
        }
        
        // update PWM duty cycle:
        // if off, then write 0 to stop motor:
        PWM6_LoadDutyValue( (motorState == MOTOR_ON) ? rpmErr : 0);
    }
}
/**
 End of File
*/