#include "driverlib.h"
#include "device.h"
#include "board.h"

// Prototype for Timer 0 ISR
__interrupt void cpuTimer0ISR(void);

// setting to xint4 to avoid potential conflict with xbar, might be able to just put
// to xint2
interrupt void xint2_isr(void);

// bare bones pwm signal
EPWM_SignalParams pwmSignal =
           {10000, 0.5f, 0.5f, true, DEVICE_SYSCLK_FREQ, SYSCTL_EPWMCLK_DIV_2,
           EPWM_COUNTER_MODE_UP_DOWN, EPWM_CLOCK_DIVIDER_1,
           EPWM_HSCLOCK_DIVIDER_1};

void main(void)
{
   // use this as a counter for the interrupt that senses the input signal
   // Initialize device clock and peripherals
   Device_init();
   Device_initGPIO();
    
   // Configure GPIO 4 as an output pin
   GPIO_setPadConfig(4, GPIO_PIN_TYPE_STD);
   GPIO_setDirectionMode(4, GPIO_DIR_MODE_OUT);
   GPIO_writePin(4, 1);

   // Configure GPIO 31 as an output pin
   GPIO_setPadConfig(31, GPIO_PIN_TYPE_STD);
   GPIO_setDirectionMode(31, GPIO_DIR_MODE_OUT);
   GPIO_writePin(31, 0);

   // Configure GPIO 34 as an output pin
   GPIO_setPadConfig(34, GPIO_PIN_TYPE_STD);
   GPIO_setDirectionMode(34, GPIO_DIR_MODE_OUT);
   GPIO_writePin(34, 1);


   // Initialize PIE and clear PIE registers. Disable CPU interrupts.
   Interrupt_initModule();
   Interrupt_initVectorTable();

   Board_init();

   // Map Timer 0 interrupt vector to our ISR function
   Interrupt_register(INT_TIMER0, &cpuTimer0ISR);

   // Interrupt to trigger xbar pins
   Interrupt_register(INT_XINT2, &xint2_isr);

   // freeze clock for the time being
   SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

   // config pwm signal for EPWM2
   EPWM_configureSignal(myEPWM2_BASE, &pwmSignal);
   EPWM_setPhaseShift(myEPWM2_BASE, 0U);
   EPWM_setSyncOutPulseMode(myEPWM2_BASE, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);

   // config pwm signal for EPWM5
   EPWM_configureSignal(myEPWM5_BASE, &pwmSignal);
   EPWM_setPhaseShift(myEPWM5_BASE, 0U);
   EPWM_setSyncOutPulseMode(myEPWM5_BASE, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);

    // config GPIO0 as input to xbar1 for pwm
   GPIO_setDirectionMode(0, GPIO_DIR_MODE_IN);          // input
   GPIO_setPadConfig(0, GPIO_PIN_TYPE_STD);
   GPIO_setQualificationMode(0, GPIO_QUAL_ASYNC);

   // config GPIO1 as input to xbar2 for pwm
   GPIO_setDirectionMode(1, GPIO_DIR_MODE_IN);          // input
   GPIO_setPadConfig(1, GPIO_PIN_TYPE_STD);
   GPIO_setQualificationMode(1, GPIO_QUAL_ASYNC);

   // config GPIO5 as input to sense the 60 hz signal
   GPIO_setDirectionMode(5, GPIO_DIR_MODE_IN);          // input
   GPIO_setPadConfig(5, GPIO_PIN_TYPE_STD);
   GPIO_setQualificationMode(5, GPIO_QUAL_ASYNC);
 
   // config the PWM on/off trigger
   XBAR_setInputPin(XBAR_INPUT1, 0);
   XBAR_setInputPin(XBAR_INPUT2, 1);

   EALLOW;

   // sets up trips for pwm2 and pwm5
   // most of the control done in interrupt, in feedback to input gpio1
   EPWM_setTripZoneAction(EPWM2_BASE, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_HIGH);
   EPWM_setTripZoneAction(EPWM2_BASE, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_HIGH);
   EPWM_enableTripZoneSignals(EPWM2_BASE, EPWM_TZ_SIGNAL_CBC1);
   EPWM_clearTripZoneFlag(EPWM2_BASE, EPWM_TZ_FLAG_CBC);

   EPWM_setTripZoneAction(EPWM5_BASE, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_HIGH);
   EPWM_setTripZoneAction(EPWM5_BASE, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_HIGH);
   EPWM_enableTripZoneSignals(EPWM5_BASE, EPWM_TZ_SIGNAL_CBC2);
   EPWM_clearTripZoneFlag(EPWM5_BASE, EPWM_TZ_FLAG_CBC);

   // setting up GPIO interrupt
    GPIO_setInterruptPin(5,GPIO_INT_XINT2);
    GPIO_setInterruptType(GPIO_INT_XINT2, GPIO_INT_TYPE_BOTH_EDGES);
    // GPIO_setInterruptType(GPIO_INT_XINT4, GPIO_INT_TYPE_FALLING_EDGE);
    GPIO_enableInterrupt(GPIO_INT_XINT2);

   EDIS;
 
   SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

   // Initialize CPU Timer 0.
   // Device_SYSCLK_FREQ is typically 200000000 (200MHz)
   // 60Hz period in microseconds = (1 / 60) * 1,000,000 = 16666.66... microseconds
   CPUTimer_setPeriod(CPUTIMER0_BASE, 1666600);
   CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);
 
   // Reload timer values and enable the timer interrupt
   CPUTimer_stopTimer(CPUTIMER0_BASE);
   CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
   CPUTimer_enableInterrupt(CPUTIMER0_BASE);
   CPUTimer_startTimer(CPUTIMER0_BASE);

   // enable CPU INT1
   Interrupt_enable(INT_TIMER0);

   // enable pin interrupt  
   Interrupt_enable(INT_XINT2);

   // this will send off if there is an input signal detected
   
   // pwm interrupt
   Interrupt_enable(INT_EPWM2);
   Interrupt_enable(INT_EPWM5);

   EALLOW;
   EINT;
   ERTM;
   EDIS;
 
   while(1)
   {
       // empty loop
   }
}

// CPU Timer 0 Interrupt Service Routine
__interrupt void cpuTimer0ISR(void)
{
   GPIO_togglePin(4);
   // Acknowledge this interrupt to receive more interrupts from group 1
   Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

interrupt void xint2_isr(void)
{
    uint32_t gpio_state = GPIO_readPin(5);

    if (gpio_state == 1)
    {
        GPIO_writePin(34, 0);
        GPIO_writePin(31, 1);
    }
    else 
    {
        GPIO_writePin(34, 1);
        GPIO_writePin(31, 0);
    }

   Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}


void configurePhase(uint32_t base, uint32_t masterBase, uint16_t phaseVal)
{
   uint32_t readPrdVal, phaseRegVal;

   //
   // Read Period value to calculate value for Phase Register
   //
   readPrdVal = EPWM_getTimeBasePeriod(masterBase);

   //
   // Caluclate phase register values based on Time Base counter mode
   //
   if((HWREGH(base + EPWM_O_TBCTL) & 0x3U) == EPWM_COUNTER_MODE_UP_DOWN)
   {
       phaseRegVal = (2U * readPrdVal * phaseVal) / 360U;
   }
   else if((HWREGH(base + EPWM_O_TBCTL) & 0x3U) < EPWM_COUNTER_MODE_UP_DOWN)
   {
       phaseRegVal = (readPrdVal * phaseVal) / 360U;
   }

   EPWM_selectPeriodLoadEvent(base, EPWM_SHADOW_LOAD_MODE_SYNC);
   EPWM_setPhaseShift(base, phaseRegVal);
   EPWM_setTimeBaseCounter(base, phaseRegVal);
}
