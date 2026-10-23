/*
Copyright 2012-2018 Jun Wako, Jack Humbert, Yiancar

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "matrix.h"
#include "debounce.h"
#include "atomic_util.h"
#undef SPLIT_KEYBOARD
#define SPLIT_KEYBOARD
#    include "split_common/split_util.h"
#    include "split_common/transactions.h"

#    define ROWS_PER_HAND (MATRIX_ROWS / 2)



#    define SPLIT_MUTABLE const


#    define SPLIT_MUTABLE_ROW

#    define SPLIT_MUTABLE_COL

#    define MATRIX_INPUT_PRESSED_STATE 0




static SPLIT_MUTABLE_ROW pin_t row_pins[ROWS_PER_HAND] = MATRIX_ROW_PINS;

static SPLIT_MUTABLE_COL pin_t col_pins[MATRIX_COLS]   = MATRIX_COL_PINS;



/* matrix state(1:on, 0:off) */
extern matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
extern matrix_row_t matrix[MATRIX_ROWS];     // debounced values

// row offsets for each hand
extern uint8_t thisHand, thatHand;

// user-defined overridable functions
__attribute__((weak)) void matrix_init_pins(void);
__attribute__((weak)) void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row);
__attribute__((weak)) void matrix_read_rows_on_col(matrix_row_t current_matrix[], uint8_t current_col, matrix_row_t row_shifter);

static inline void gpio_atomic_set_pin_output_low(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_output(pin);
        gpio_write_pin_low(pin);
    }
}

static inline void gpio_atomic_set_pin_output_high(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_output(pin);
        gpio_write_pin_high(pin);
    }
}

static inline void gpio_atomic_set_pin_input_high(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_input_high(pin);
    }
}

static inline uint8_t readMatrixPin(pin_t pin) {
    if (pin != NO_PIN) {
        return (gpio_read_pin(pin) == MATRIX_INPUT_PRESSED_STATE) ? 0 : 1;
    } else {
        return 1;
    }
}

// matrix code


static bool select_row(uint8_t row) {
    pin_t pin = row_pins[row];
    if (pin != NO_PIN) {
        gpio_atomic_set_pin_output_low(pin);
        return true;
    }
    return false;
}

static void unselect_row(uint8_t row) {
    pin_t pin = row_pins[row];
    if (pin != NO_PIN) {
#            ifdef MATRIX_UNSELECT_DRIVE_HIGH
        gpio_atomic_set_pin_output_high(pin);
#            else
        gpio_atomic_set_pin_input_high(pin);
#            endif
    }
}

static void unselect_rows(void) {
    for (uint8_t x = 0; x < ROWS_PER_HAND; x++) {
        unselect_row(x);
    }
}

__attribute__((weak)) void matrix_init_pins(void) {
    unselect_rows();
    for (uint8_t x = 0; x < MATRIX_COLS; x++) {
        if (col_pins[x] != NO_PIN) {
            gpio_atomic_set_pin_input_high(col_pins[x]);
        }
    }
}

__attribute__((weak)) void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {
    // Start with a clear matrix row
    matrix_row_t current_row_value = 0;

    if (!select_row(current_row)) { // Select row
        return;                     // skip NO_PIN row
    }
    matrix_output_select_delay();

    // For each col...
    matrix_row_t row_shifter = MATRIX_ROW_SHIFTER;
    for (uint8_t col_index = 0; col_index < MATRIX_COLS; col_index++, row_shifter <<= 1) {
        uint8_t pin_state = readMatrixPin(col_pins[col_index]);

        // Populate the matrix row with the state of the col pin
        current_row_value |= pin_state ? 0 : row_shifter;
    }

    // Unselect row
    unselect_row(current_row);
    matrix_output_unselect_delay(current_row, current_row_value != 0); // wait for all Col signals to go HIGH

    // Update the matrix
    current_matrix[current_row] = current_row_value;
}



void matrix_init(void) {

    // Set pinout for right half if pinout for that half is defined
    if (!isLeftHand) {


        const pin_t row_pins_right[ROWS_PER_HAND] = MATRIX_ROW_PINS_RIGHT;
        for (uint8_t i = 0; i < ROWS_PER_HAND; i++) {
            row_pins[i] = row_pins_right[i];
        }


        const pin_t col_pins_right[MATRIX_COLS] = MATRIX_COL_PINS_RIGHT;
        for (uint8_t i = 0; i < MATRIX_COLS; i++) {
            col_pins[i] = col_pins_right[i];
        }

    }

    thisHand = isLeftHand ? 0 : (ROWS_PER_HAND);
    thatHand = ROWS_PER_HAND - thisHand;


    // initialize key pins
    matrix_init_pins();

    // initialize matrix state: all keys off
    memset(matrix, 0, sizeof(matrix));
    memset(raw_matrix, 0, sizeof(raw_matrix));

    debounce_init(ROWS_PER_HAND);

    matrix_init_kb();
}


// Fallback implementation for keyboards not using the standard split_util.c
bool transport_master_if_connected1(matrix_row_t master_matrix[], matrix_row_t slave_matrix[]) {
    //transport_master(master_matrix, slave_matrix);
    return false; // Treat the transport as always connected
}

#include "print.h"
void print_split_matrix(matrix_row_t *matrix) {
    xprintf("\n=== Matrix State ===\n");

    // 打印左手矩阵
    xprintf("Left hand:\n");
    for (int row = 0; row < ROWS_PER_HAND; row++) {
        xprintf("L%02d: ", row);
        for (int col = 0; col < MATRIX_COLS; col++) {
            bool pressed = (matrix[row] >> col) & 1;
            xprintf("%d", pressed);
        }
        xprintf("\n");
    }

    // 打印右手矩阵
    xprintf("Right hand:\n");
    for (int row = 0; row < ROWS_PER_HAND; row++) {
        xprintf("R%02d: ", row);
        for (int col = 0; col < MATRIX_COLS; col++) {
            bool pressed = (matrix[row + ROWS_PER_HAND] >> col) & 1;
            xprintf("%d", pressed);
        }
        xprintf("\n");
    }

    xprintf("====================\n");
}
uint32_t counter1 = 1;
bool matrix_post_scan_qf(void) {
    bool changed = false;

counter1++;
    if (is_keyboard_master()) {
        //这里是主手
        static bool  last_connected              = false;
        matrix_row_t slave_matrix[ROWS_PER_HAND] = {0};
        //获取副手数据
        if (transport_master_if_connected1(matrix + thisHand, slave_matrix)) {
            //和现有的数组比较是否有变化
            changed = memcmp(matrix + thatHand, slave_matrix, sizeof(slave_matrix)) != 0;
            if(counter1%5000 ==0){
                if(changed){
                    xprintf("data from slave  changed %ld\n",counter1/1000);
                }else{
                    xprintf("data from slave no changed %ld\n",counter1/1000);
                }
            }
            last_connected = true;
        } else if (last_connected) {

             if(changed){
                xprintf("last_connected  changed %ld\n",counter1/1000);
            }else{
                xprintf("last_connected no changed %ld\n",counter1/1000);
            }

            // reset other half when disconnected
            memset(slave_matrix, 0, sizeof(slave_matrix));
            changed = true;
            last_connected = false;
        }else{

        }
        //数据copy 到从机数组位置
        if (changed) memcpy(matrix + thatHand, slave_matrix, sizeof(slave_matrix));
        if (changed) {
            xprintf("Data from slave changed!\n");

            // 更新右手矩阵内容
            memcpy(matrix + thatHand, slave_matrix, sizeof(slave_matrix));

            // 打印当前整个矩阵状态
            print_split_matrix(matrix);
        }

        matrix_scan_kb();
    } else {
        if(counter1 % 15000 ==0){
            extern  bool usb_bus_detected(void);
            if(usb_bus_detected()){

                xprintf("222 usb_bus_detected! this is master!!! %ld\n",counter1/1000);

                __set_FAULTMASK(1);
                NVIC_SystemReset();
            }
           // xprintf("xxxxxxxxxx! %ld\n",counter1/1000);

             //print_split_matrix(matrix);
        }

        transport_slave(matrix + thatHand, matrix + thisHand);

        matrix_slave_scan_kb();
    }


// if(counter % 1000 ==0){
//          xprintf("2222 Data from slave changed! %ld\n",counter/1000);

//             print_split_matrix(matrix);
// }


    return changed;
}


uint8_t matrix_scan(void) {
    matrix_row_t curr_matrix[MATRIX_ROWS] = {0};

    // Set row, read cols
    for (uint8_t current_row = 0; current_row < ROWS_PER_HAND; current_row++) {
        matrix_read_cols_on_row(curr_matrix, current_row);
    }


    bool changed = memcmp(raw_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
    if (changed) memcpy(raw_matrix, curr_matrix, sizeof(curr_matrix));


    changed = debounce(raw_matrix, matrix + thisHand, ROWS_PER_HAND, changed) | matrix_post_scan_qf();

    return (uint8_t)changed;
}
