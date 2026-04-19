#include <stdint.h>
#include <stdio.h>
#include "PLL.h"
#include "SysTick.h"
#include "uart.h"
#include "onboardLEDs.h"
#include "tm4c1294ncpdt.h"
#include "VL53L1X_api.h"

#define I2C_MCS_ACK 0x00000008
#define I2C_MCS_DATACK 0x00000008
#define I2C_MCS_ADRACK 0x00000004
#define I2C_MCS_STOP 0x00000004
#define I2C_MCS_START 0x00000002
#define I2C_MCS_ERROR 0x00000002
#define I2C_MCS_RUN 0x00000001
#define I2C_MCS_BUSY 0x00000001
#define I2C_MCR_MFE 0x00000010

#define NUM_ANGLES 128
#define NUM_SCANS 3

int angle[NUM_ANGLES] = {0, 2, 5, 8, 11, 14, 16, 19, 22, 25, 28, 30, 33, 36, 39, 42, 45, 47, 50, 53, 56, 59, 61, 64, 67, 70, 73, 75, 78, 81, 84, 87, 90, 92, 95, 98, 101, 104, 106, 109, 112, 115, 118, 120, 123, 126, 129, 132, 135, 137, 140, 143, 146, 149, 151, 154, 157, 160, 163, 165, 168, 171, 174, 177, 180, 182, 185, 188, 191, 194, 196, 199, 202, 205, 208, 210, 213, 216, 219, 222, 225, 227, 230, 233, 236, 239, 241, 244, 247, 250, 253, 255, 258, 261, 264, 267, 270, 272, 275, 278, 281, 284, 286, 289, 292, 295, 298, 300, 303, 306, 309, 312, 315, 317, 320, 323, 326, 329, 331, 334, 337, 340, 343, 345, 348, 351, 354, 357};
int distance[NUM_SCANS][NUM_ANGLES];
static const uint8_t STEPPER_SEQ[8] = {
    0x09, 0x08, 0x0C, 0x04,
    0x06, 0x02, 0x03, 0x01};

volatile int pj0Pressed = 0;
volatile int running = 0;
volatile int stopReq = 0;

int currentScan = 0;
int stepperIndex = 0;

void I2C_Init(void)
{
    SYSCTL_RCGCI2C_R |= SYSCTL_RCGCI2C_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
    while ((SYSCTL_PRGPIO_R & 0x0002) == 0)
    {
    }

    GPIO_PORTB_AFSEL_R |= 0x0C;
    GPIO_PORTB_ODR_R |= 0x08;
    GPIO_PORTB_DEN_R |= 0x0C;
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xFFFF00FF) + 0x00002200;

    I2C0_MCR_R = I2C_MCR_MFE;
    I2C0_MTPR_R = 0b0000000000000101000000000111011;
}

void PortG_Init(void)
{
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R6;
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R6) == 0)
    {
    }

    GPIO_PORTG_DIR_R &= ~0x01;
    GPIO_PORTG_AFSEL_R &= ~0x01;
    GPIO_PORTG_DEN_R |= 0x01;
    GPIO_PORTG_AMSEL_R &= ~0x01;
}

void VL53L1X_XSHUT(void)
{
    GPIO_PORTG_DIR_R |= 0x01;
    GPIO_PORTG_DATA_R &= ~0x01;
    SysTick_Wait10ms(10);
    GPIO_PORTG_DIR_R &= ~0x01;
}

void Stepper_Init(void)
{
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R7;
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R7) == 0)
    {
    }

    GPIO_PORTH_DIR_R |= 0x0F;
    GPIO_PORTH_DEN_R |= 0x0F;
    GPIO_PORTH_AFSEL_R &= ~0x0F;
    GPIO_PORTH_AMSEL_R &= ~0x0F;
}

void Stepper_Output(uint8_t data)
{
    GPIO_PORTH_DATA_R = (GPIO_PORTH_DATA_R & ~0x0F) | (data & 0x0F);
}

void Stepper_HalfStep_Forward(void)
{
    Stepper_Output(STEPPER_SEQ[stepperIndex]);
    stepperIndex = (stepperIndex + 1) % 8;

    SysTick_Wait10us(100); // 10 ms per half-step
}

void Stepper_HalfStep_Backward(void)
{
    stepperIndex = (stepperIndex - 1 + 8) % 8;
    Stepper_Output(STEPPER_SEQ[stepperIndex]);

    SysTick_Wait10us(100);
}
void Stepper_Rotate45(void)
{
    for (int i = 0; i < 32; i++)
    {
        if (stopReq)
            break;
        Stepper_HalfStep_Forward();
    }
    Stepper_Output(0x00);

    SysTick_Wait10ms(1); // let it settle
}
void Stepper_Rotate45_Backward(void)
{
    for (int i = 0; i < 32; i++)
    {
        if (stopReq)
            break;
        Stepper_HalfStep_Backward();
    }
    Stepper_Output(0x00);
    SysTick_Wait10ms(1);
}
void Stepper_Rotate45_Backward_Force(void)
{
    for (int i = 0; i < 32; i++)
    {
        Stepper_HalfStep_Backward();
    }
    Stepper_Output(0x00);
    SysTick_Wait10ms(1);
}
void EnableInt(void)
{
    __asm("    cpsie   i\n");
}

// Disable interrupts
void DisableInt(void)
{
    __asm("    cpsid   i\n");
}

// Low power wait
void WaitForInt(void)
{
    __asm("    wfi\n");
}

void PortJ_Init(void)
{
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R8; // activate clock for Port J
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R8) == 0)
    {
    }

    GPIO_PORTJ_DIR_R &= ~0x01;   // PJ0 input
    GPIO_PORTJ_DEN_R |= 0x01;    // digital enable PJ0
    GPIO_PORTJ_AFSEL_R &= ~0x01; // GPIO function
    GPIO_PORTJ_AMSEL_R &= ~0x01; // no analog

    GPIO_PORTJ_PCTL_R &= ~0x0000000F; // clear PCTL for PJ0
}

void PortJ_Interrupt_Init(void)
{

    GPIO_PORTJ_IS_R &= ~0x01;  // PJ0 edge-sensitive
    GPIO_PORTJ_IBE_R &= ~0x01; // not both edges
    GPIO_PORTJ_IEV_R &= ~0x01; // falling edge
    GPIO_PORTJ_ICR_R = 0x01;   // clear flag for PJ0
    GPIO_PORTJ_IM_R |= 0x01;   // arm interrupt on PJ0
    GPIO_PORTJ_PUR_R |= 0x01;  // weak pull-up on PJ0
    NVIC_EN1_R = 0x00080000;   // (Step 2) Enable interrupt 51 in NVIC (which is in Register EN1)
    NVIC_PRI12_R = 0xA0000000; // (Step 4) Set interrupt priority to 5
    EnableInt();
}
void GPIOJ_IRQHandler(void)
{
    GPIO_PORTJ_ICR_R = 0x01;
    if (running)
    {
        stopReq = 1; // if already scanning, stop immediately
    }
    else
    {
        pj0Pressed = 1; // if idle, use press to start
    }
}
uint16_t dev = 0x29;
int status = 0;

int main(void)
{
    uint8_t byteData, sensorState = 0, myByteArray[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, i = 0;
    uint16_t wordData;
    uint16_t Distance;
    uint16_t SignalRate;
    uint16_t AmbientRate;
    uint16_t SpadNum;
    uint8_t RangeStatus;
    uint8_t dataReady;

    // initialize
    PLL_Init();
    SysTick_Init();
    onboardLEDs_Init();
    I2C_Init();
    UART_Init();
    PortG_Init();
    Stepper_Init();
    PortJ_Init();
    PortJ_Interrupt_Init();
    VL53L1X_XSHUT();
    UART_printf("Deliverable 2\r\n");

    // 1 Wait for device booted
    while (sensorState == 0)
    {
        status = VL53L1X_BootState(dev, &sensorState);
        SysTick_Wait10ms(10);
    }

    FlashAllLEDs();
    UART_printf("ToF Chip Booted!\r\n Please Wait...\r\n");

    status = VL53L1X_ClearInterrupt(dev); /* clear interrupt has to be called to enable next interrupt*/

    /* 2 Initialize the sensor with the default setting  */
    status = VL53L1X_SensorInit(dev);
    Status_Check("SensorInit", status);

    /* 3 Optional functions to be used to change the main ranging parameters according the application requirements to get the best ranging performances */
    status = VL53L1X_SetDistanceMode(dev, 2);           /* 1=short, 2=long */
    status = VL53L1X_SetTimingBudgetInMs(dev, 100);     /* in ms possible values [20, 50, 100, 200, 500] */
    status = VL53L1X_SetInterMeasurementInMs(dev, 120); /* in ms, IM must be > = TB */

    while (1)
    {

        if (pj0Pressed)
        {
            pj0Pressed = 0;

            SysTick_Wait10ms(1); // tiny debounce
            while ((GPIO_PORTJ_DATA_R & 0x01) == 0)
            {
            } // wait for release

            if (running)
            {
                stopReq = 1; // press during scan -> stop
            }
            else
            {
                running = 1; // press while idle -> start
                stopReq = 0;
            }
        }

        if (running)
        {

            if (currentScan >= NUM_SCANS)
            {
                UART_printf("END\r\n");
                VL53L1X_StopRanging(dev);
                Stepper_Output(0x00);
                running = 0;
                stopReq = 0;
            }
            else
            {
                int sweepStepsTaken = 0;
                int completedAngles = 0;

                status = VL53L1X_StartRanging(dev);

                for (int i = 0; i < NUM_ANGLES; i++)
                {

                    if (i > 0)
                    {
                        Stepper_Rotate45();
                        if (!stopReq)
                        {
                            sweepStepsTaken++;
                        }
                    }

                    dataReady = 0;
                    while (dataReady == 0)
                    {
                        status = VL53L1X_CheckForDataReady(dev, &dataReady);
                        VL53L1_WaitMs(dev, 5);

                        if (stopReq)
                        {
                            break;
                        }
                    }

                    if (stopReq)
                    {
                        break;
                    }

                    status = VL53L1X_GetRangeStatus(dev, &RangeStatus);
                    status = VL53L1X_GetDistance(dev, &Distance);
                    status = VL53L1X_ClearInterrupt(dev);

                    int valid = 1;

                    if (RangeStatus == 4 || RangeStatus == 7)
                    {
                        valid = 0;
                    }

                    if (Distance == 0 || Distance > 4000)
                    {
                        valid = 0;
                    }

                    if (!valid)
                    {
                        Distance = 65535; // invalid marker
                    }
                    distance[currentScan][i] = Distance;
                    completedAngles++;

                    SysTick_Wait10ms(1);
                }

                for (int j = 0; j < sweepStepsTaken; j++)
                {
                    Stepper_Rotate45_Backward_Force();
                }

                if (stopReq)
                {
                    UART_printf("STOPPED\r\n");
                    sprintf(printf_buffer, "RESCAN %d\r\n", currentScan);
                    UART_printf(printf_buffer);
                }
                else
                {
                    for (int i = 0; i < completedAngles; i++)
                    {
                        sprintf(printf_buffer, "%d,%d,%u\r\n", currentScan, angle[i], distance[currentScan][i]);
                        UART_printf(printf_buffer);
                    }

                    currentScan++;

                    if (currentScan >= NUM_SCANS)
                    {
                        UART_printf("END\r\n");
                    }
                    else
                    {
                        UART_printf("END_SCAN\r\n");
                    }
                }

                VL53L1X_StopRanging(dev);
                Stepper_Output(0x00);

                running = 0;
                stopReq = 0;
            }
        }
    }
}
