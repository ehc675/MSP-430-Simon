#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include "rgb_interface.h"
#include "simon_random.h"

/**
 * main.c
 */

void init_wdt(void){
    BCSCTL3 |= LFXT1S_2;     // ACLK = VLO
    WDTCTL = WDT_ADLY_1000;    // WDT 16ms (~43.3ms since clk 12khz), ACLK, interval timer
    IE1 |= WDTIE;            // Enable WDT interrupt
}
void init_buttons() {
    P2DIR &= ~(BIT0 + BIT2 + BIT3 + BIT4); // set to input
    P2REN = BIT0 + BIT2 + BIT3 + BIT4; // enable pullup/down resistors
    P2OUT = BIT0 + BIT2 + BIT3 + BIT4; // set resistors to pull up

    /* Uncomment the following code if you want to use interrupts to detect button presses */
    P2IES = BIT0 + BIT2 + BIT3 + BIT4; // listen for high to low transitions
    P2IFG &=  ~(BIT0 + BIT2 + BIT3 + BIT4); // clear any pending interrupts
    P2IE = BIT0 + BIT2 + BIT3 + BIT4; // enable interrupts for these pins

}

/* ---------------------------------------------------------------- */
/* Code for generating and testing the Simon values */

#define INITIAL_BUTTON 0

unsigned int next_button() {
    return rand();
}
unsigned int check_button(unsigned int button) {

    if (button == rand())
        return 1;
    else
        return 0;
}

void reset_button_sequence(unsigned int initial_value) {
    srand(initial_value);
    return;
}

/* ---------------------------------------------------------------- */

//colors for the LEDs
uint8_t red[] = {0xF0, 0, 0, 10};
uint8_t blue[] = {0xF0, 10, 0, 0};
uint8_t green[] = {0xF0, 0, 10, 0};
uint8_t yellow[] = {0xF0, 0, 10, 10};
uint8_t pink[] = {0xF0, 3, 3, 10};
uint8_t debug[] = {0xF0, 10, 10, 10};
uint8_t off[] = {0xE0, 0, 0, 0};

//notes for buzzer
int periods[] = {1000000/261.63, 1000000/293.66, 1000000/392.63, 1000000/349.23, 0};

/*show method for playing a sound*/
void play_sound(int i)
{
    //code taken from buzzer template using timer 1
    TA1CCR0 = periods[i];
    TA1CCR2 = periods[i]>>1; // divde by 2
    TA1CCTL2 = OUTMOD_7;
    TA1CTL = TASSEL_2 + MC_1; // SMCLK, upmode
}


/*show method for showing the current pattern*/
/* delay cycles used for light and sound duration*/
void show(int arr[], int len, int tog) {
    int i;
    //special toggle for reset game
    if (tog == 5) {
            rgb_set_LEDs(debug, debug, debug, debug, debug);
            play_sound(5);
            __delay_cycles(700000);
    }
    //special toggle for win state
    if (tog == 4) {
        for (i = 0; i < 4; i++) {
            rgb_set_LEDs(green, off, green, off, green);
            play_sound(0);
            __delay_cycles(700000);
            rgb_set_LEDs(off, green, off, green, off);
            play_sound(3);
            __delay_cycles(700000);
        }
    }
    //special toggle for lose state
    if (tog == 3) {
        for (i = 0; i < 4; i++) {
            rgb_set_LEDs(red, off, red, off, red);
            play_sound(2);
            __delay_cycles(700000);
            rgb_set_LEDs(off, red, off, red, off);
            play_sound(1);
            __delay_cycles(700000);
        }
    }
    //special toggle for show mode
    if (tog == 1) {
        for (i = 0; i < len; i++) {
            //case statements depending on color
            switch(arr[i]) {
            //red
            case 0:
                rgb_set_LEDs(red, off, off, off, debug);
                play_sound(0);
                __delay_cycles(700000);
                break;
            //green
            case 1:
                rgb_set_LEDs(off, green, off, off, debug);
                play_sound(1);
                __delay_cycles(700000);

                break;
            //blue
            case 2:
                rgb_set_LEDs(off, off, blue, off, debug);
                play_sound(2);
                __delay_cycles(700000);
                break;
            //yellow
            case 3:
                rgb_set_LEDs(off, off, off, yellow, debug);
                play_sound(3);
                __delay_cycles(700000);
                break;
            }
        }
        rgb_set_LEDs(off, off, off, off, off);
        play_sound(5);
    }
    //special case for user input
    if (tog == 0) {
        for (i = 0; i < len; i++) {
            //case statements depending on color
            switch(arr[i]) {
            //red
            case 0:
                rgb_set_LEDs(red, off, off, off, off);
                play_sound(0);
                __delay_cycles(500000);
                break;
            //green
            case 1:
                rgb_set_LEDs(off, green, off, off, off);
                play_sound(1);
                __delay_cycles(500000);

                break;
            //blue
            case 2:
                rgb_set_LEDs(off, off, blue, off, off);
                play_sound(2);
                __delay_cycles(500000);
                break;
            //yellow
            case 3:
                rgb_set_LEDs(off, off, off, yellow, off);
                play_sound(3);
                __delay_cycles(500000);
                break;
            }
        }
        rgb_set_LEDs(off, off, off, off, off);
        play_sound(5);
    }
}

/* ---------------------------------------------------------------- */

//arrays for show method
int temp[4] = {0,1,2,3};
int red_arr[1] = {0};
int green_arr[1] = {1};
int blue_arr[1] = {2};
int yellow_arr[1] = {3};

//global variables for entire file
int count = 0; // <- used for input time out
int timer_wakeup_flag = 0;
int button_wakeup_flag = 0;
int start_flag = 1;
int curr_btn = 4; // <- used for identifying which button has been pressed

//char charmemval[] = "The Latest Color added to the LED Strip has Temperature: ";
//char charreturn[] = "\r\n";
//char mv_char[5];
//void ser_output(char *str);
//void itoa(int n, char s[]);

int seed = 5;
int next_seed;

int main(void)
{
    enum state_enum{PresentingSequence, Input, Lost, Win, Idle} state; // enum to describe state of system

    TA0CTL = TASSEL_2 + MC_1 + ID_3; //Timer used for RNG
    TA0CCR0 = 5000;

    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    DCOCTL = 0;                 // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;     // Set range
    DCOCTL = CALDCO_1MHZ;      // Set DCO step + modulation */
    BCSCTL3 |= LFXT1S_2;        // ACLK = VLO - This is also called in the init_wdt() function
    P2DIR |= BIT5; // We need P2.5 to be output
    P2SEL |= BIT5; // P2.5 is TA1.2 PWM output
//    UCA0CTL1 |= UCSWRST+UCSSEL_2;
//    UCA0BR0 = 52; //settings for 19200 baud
//    UCA0BR1 = 0;
//    UCA0MCTL = UCBRS_0;
//    UCA0CTL1 &= ~UCSWRST;
    init_wdt();
    rgb_init_spi();
    init_buttons();

    /* To make a functional Simon, you'll need to seed the random number generation system
       randomly! */
//    int initial_random_seed = 10;
//    reset_button_sequence(initial_random_seed); // TODO: Add code to make the initial seed random!

    _enable_interrupts();

    int i = 0;
    int sequence_counter = 0;
    int sequence_length = 4; // GRADING: This variable should be changed to test different sequence lengths
    int pat_arr[4]; // change this such that the size is equal to sequence_length
    int button;

    while (1) {

        /* This template is a potentially a place to start. What it provides is a
           foundation of a main loop which gets woken up either by a timer (the WDT)
           or by a button press (assuming you enable the button interrupts).
           But it needs more code:

          - SOUND!!!

          - Win state

          - WaitingForUserInput state, including processing button inputs
            (see check_button() function!)

          - If you use the button interrupts to wake up, then processing button
            input for maximum score will require changing the IES (edge-select)
            register to detect the press and release as separate events.
            + Alternatively, if you choose to use polling, you need to handle timing
            for things like sequence presentation, timeout detection, etc. This is
            probably easier than using interrupts, but if you naively use the TA1R
            or TA0R registers (the counters for TA0 or TA1), they may conflict
            with other uses of those resources for sound or serial communication.
            + A middle ground is to use the fastest WDT wakeup and poll the button
            states periodically (you can safely assume that button presses
            will last at least 40-50 ms). Of course, this would still require handling
            timeouts somehow.

          - Animations for win/lose

          - Processing if a timeout has occured to trigger the Lost state

          - Ideally, you will abstract a bunch more so that the code will be more readable!
        */

        if (timer_wakeup_flag) {
            timer_wakeup_flag = 0;
        }

        //put in idle state on start up
        if (start_flag) {
            start_flag = 0;
            state = Idle;
        }

        //enter idle state
        if (state == Idle) {
            i = 0;
            int j = 0;
            seed = seed + 1;
            next_seed = seed;
            reset_button_sequence(seed);
            //clear out the pattern array while in idle for new game
            for (j = 0; j < sequence_length; j++) {
                pat_arr[j] = 4;
            }
            //turn off any LEDs and sound / set timer back down to zero
            rgb_set_LEDs(off, off, off, off, off);
            play_sound(5);
            count = 0;
            //detect button press to enter new game
            if (curr_btn == 0 | curr_btn == 1 | curr_btn == 2 | curr_btn == 3) {
                curr_btn = 4;
                show(temp, 0, 5);
                sequence_counter = 0;
                //enter Presenting state
                state = PresentingSequence;
            }
        }

        //enter win state
        else if (state == Win) {
            //turn all LEDs off since display was triggered in Presenting state
            rgb_set_LEDs(off, off, off, off, off);
            play_sound(5);
            count = 0;
            //enter Idle state
            state = Idle;
        }

        //enter lost state
        else if (state == Lost) {
            //reset count and show lost animation
            count = 0;
            show(temp, 0, 3);
            //enter Idle state
            state = Idle;
        }

        //enter presenting state
        else if (state == PresentingSequence) {
            count = 0;
            //if new game reset button sequence
            if (sequence_counter == 0) {
//                reset_button_sequence(initial_random_seed);
            }
            //seed the LFSR for RNG
            srand(next_seed);
            next_seed = return_lfsr();
            //grab next button
            button = next_button();
            //increment the sequence counter pattern length that needs to be met
            sequence_counter = sequence_counter + 1;
            //if counter is greater then length, then game has been won
            if (sequence_counter > sequence_length) { // TODO: This is actually the condition for winning!
                count = 0;
                //show win animation
                show(temp, 0, 4);
                //enter win state
                state = Win; // TODO: Change to "Win" (need to add an enum), and add processing code for winning
            }
            //if counter length has not been met, continue with game
            else {
                //add new button into the pattern array
                pat_arr[sequence_counter - 1] = button;
                //show the pattern
                show(pat_arr, sequence_counter, 1);
                //enter input state
                state = Input;
                //reset the WDT for user input timing
                init_wdt();
            }
        }

        //enter input state
        else if (state == Input) {
            //while there is still time for user to input buttons
            while (count != 3) {
                //swtich statements based on what buttons have been pressed
                //only for visual
                switch(curr_btn) {
                //red
                case 0:
                    show(red_arr, 1, 0);
                    break;
                //green
                case 1:
                    show(green_arr, 1, 0);
                    break;
                //blue
                case 2:
                    show(blue_arr, 1, 0);
                    break;
                //yellow
                case 3:
                    show(yellow_arr, 1, 0);
                    break;
                }
                //check to see if the curr_btn (button that has been pressed) is equal to ith number in pattern
                if (curr_btn == pat_arr[i]) {
                    //increment for i + 1 button for next pattern check
                    i = i + 1;
                    //if sequence length has been met, get n + 1 button and show pattern in presenting
                    if (i == sequence_counter) {
                        //enter presenting state
                        state = PresentingSequence;
                        //reset i for next input state
                        i = 0;
                        break;
                    }
                    //if sequence length has not been met, check next button
                    else {
                        //enter input state
                        state = Input;
                        break;
                    }
                }
                //if curr_btn is not equal to ith number in pattern
                else {
                    //enter lose state
                    state = Lost;
                    break;
                }
            }
            //if time has ran out for user input
            if (count == 2) {
                state = Lost;
            }
            //set curr_btn equal to 4 (since user cannot set button to 4)
            curr_btn = 4;
        }
        __bis_SR_register(LPM3_bits + GIE);
    }
    return 0;
}

// Watchdog Timer interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(WDT_VECTOR))) watchdog_timer (void)
#else
#error Compiler not supported!
#endif
{
    //increment count on each WDT interrupt
    count = count + 1;
//    timer_wakeup_flag = 1;
    __bic_SR_register_on_exit(LPM3_bits); // exit LPM3 when returning to program (clear LPM3 bits)
}

// Watchdog Timer interrupt service routine
// Port 2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void button(void) {
    //button interrupt logic
    //red
    if (P2IFG & BIT0) {
        P2IFG &= ~BIT0; // clear the interrupt flag associated with P2.5
//        itoa(1, mv_char);
//        ser_output(mv_char);
        count = 0;
        //set curr_btn global variable
        curr_btn = 0;
//        seed = TA0R;
    }
    //green
     if (P2IFG & BIT2) {
        P2IFG &= ~BIT2; // clear the interrupt flag associated with P2.5
//        itoa(2, mv_char);
//        ser_output(mv_char);
        count = 0;
        //set curr_btn global variable
        curr_btn = 1;
//        seed = TA0R;
    }
    //yellow
    else if (P2IFG & BIT3) {
        P2IFG &= ~BIT3;
//        itoa(3, mv_char);
//        ser_output(mv_char);
        count = 0;
        //set curr_btn global variable
        curr_btn = 3;
//        seed = TA0R;

    }
    //blue
    else if (P2IFG & BIT4) {
        P2IFG &= ~BIT4;
//        itoa(4, mv_char);
//        ser_output(mv_char);
        count = 0;
        //set curr_btn global variable
        curr_btn = 2;
//        seed = TA0R;
}
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT2_VECTOR))) button (void)
#else
#error Compiler not supported!
#endif
    {
    seed = TA0R;
    button_wakeup_flag = 1;
    P2IFG &= ~(BIT0 + BIT2 + BIT3 + BIT4);
    }
    __bic_SR_register_on_exit(LPM3_bits); // exit LPM3 when returning to program (clear LPM3 bits)
}

//void ser_output(char *str) {
//    while(*str != 0) {
//        while (!(IFG2&UCA0TXIFG));
//        UCA0TXBUF = *str++;
//    }
//}
//
///* itoa: convert n to characters in s */
//void itoa(int n, char s[]) {
//    int i, sign;
//    if ((sign = n) < 0) /* record sign */
//        n = -n; /* make n positive */
//        i = 0;
//        do { /* generate digits in reverse order */
//            s[i++] = n % 10 + '0'; /* get next digit */
//        } while ((n /= 10) > 0); /* delete it */
//        if (sign < 0)
//            s[i++] = '-';
//        s[i] = '\0';
//        reverse(s);
//}
//
//
//void reverse(char s[])
//{
//    int i, j;
//    char c;
//    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
//        c = s[i];
//        s[i] = s[j];
//        s[j] = c;
//    }
//}
