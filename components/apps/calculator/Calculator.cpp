#include <math.h>
#include <vector>
#include "Calculator.hpp"


#if defined(CONFIG_CALCULATOR_LOG)
#include "esp_log.h"
static const char *TAG = "calculator";
#endif


using namespace std;

// Declare the calculator app icon image
LV_IMG_DECLARE(img_app_calculator);

// Define constants for keyboard and label styles
#define KEYBOARD_H_PERCENT      65  // Keyboard height as a percentage of the screen height
#define KEYBOARD_FONT           &lv_font_montserrat_48  // Font used for keyboard buttons
#define KEYBOARD_SPECIAL_COLOR  lv_color_make(0, 0x99, 0xff)  // Color for special keyboard buttons

#define LABEL_PAD               5  // Padding for labels
#define LABEL_FONT_SMALL        &lv_font_montserrat_34  // Font for small labels
#define LABEL_FONT_BIG          &lv_font_montserrat_48  // Font for big labels
#define LABEL_COLOR             lv_color_make(170, 170, 170)  // Color for labels
#define LABEL_FORMULA_LEN_MAX   256  // Maximum length of the formula string

// Define the keyboard button map
static const char *keyboard_map[] = {
    "C", "/", "x", LV_SYMBOL_BACKSPACE, "\n",
    "7", "8", "9", "-", "\n",
    "4", "5", "6", "+", "\n",
    "1", "2", "3", "%", "\n",
    "0", ".", "=", ""
};

// Constructor for the Calculator class
Calculator::Calculator():
    ESP_Brookesia_PhoneApp("Calculator", &img_app_calculator, true)
{
}

// Destructor for the Calculator class
Calculator::~Calculator()
{

}

// Main function to run the calculator app
bool Calculator::run(void)
{
    // Get the visual area of the screen
    lv_area_t area = getVisualArea();
    _width = area.x2 - area.x1;  // Calculate the width of the area
    _height = area.y2 - area.y1;  // Calculate the height of the area
    formula_len = 1;  // Initialize formula length

    // Calculate dimensions for keyboard and labels
    int keyboard_h = (int)(_height * KEYBOARD_H_PERCENT / 100.0);
    int label_h = _height - keyboard_h;
    int text_h = label_h - 2 * LABEL_PAD;

    // Create and configure the keyboard
    keyboard = lv_btnmatrix_create(lv_scr_act());
    lv_btnmatrix_set_map(keyboard, keyboard_map);
    lv_btnmatrix_set_btn_width(keyboard, 18, 1);
    lv_obj_set_size(keyboard, _width, keyboard_h);
    lv_obj_set_style_text_font(keyboard, KEYBOARD_FONT, 0);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, this);
    lv_btnmatrix_set_btn_ctrl(keyboard, 18, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_style_border_width(keyboard, 0, 0);
    lv_obj_set_style_radius(keyboard, 0, 0);

    // Create and configure the label container
    lv_obj_t *label_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(label_obj, _width, label_h);
    lv_obj_align(label_obj, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(label_obj, 0, 0);
    lv_obj_set_style_border_width(label_obj, 0, 0);
    lv_obj_set_style_pad_all(label_obj, 0, 0);
    lv_obj_set_style_text_font(label_obj, LABEL_FONT_SMALL, 0);
    lv_obj_set_flex_flow(label_obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(label_obj, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(label_obj, LABEL_PAD, 0);

    // Create and configure the history label
    history_label = lv_textarea_create(label_obj);
    lv_obj_set_style_radius(history_label, 0, 0);
    lv_obj_set_style_border_width(history_label, 0, 0);
    lv_obj_set_style_pad_all(history_label, 0, 0);
    lv_obj_set_size(history_label, _width, text_h / 3);
    lv_obj_add_flag(history_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_align(history_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_opa(history_label, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_text_color(history_label, LABEL_COLOR, 0);
    lv_textarea_set_text(history_label, "");

    // Create and configure the formula label
    lv_obj_t *formula_label_obj = lv_obj_create(label_obj);
    lv_obj_set_size(formula_label_obj, _width, text_h / 3);
    lv_obj_set_style_radius(formula_label_obj, 0, 0);
    lv_obj_set_style_border_width(formula_label_obj, 0, 0);
    lv_obj_set_style_pad_all(formula_label_obj, 0, 0);
    lv_obj_set_style_bg_opa(formula_label_obj, LV_OPA_TRANSP, 0);

    formula_label = lv_label_create(formula_label_obj);
    lv_obj_set_size(formula_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(formula_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_align(formula_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(formula_label, LABEL_FONT_BIG, 0);
    lv_label_set_text(formula_label, "0");

    // Create and configure the result label
    lv_obj_t *result_label_obj = lv_obj_create(label_obj);
    lv_obj_set_size(result_label_obj, _width, text_h / 3);
    lv_obj_set_style_radius(result_label_obj, 0, 0);
    lv_obj_set_style_border_width(result_label_obj, 0, 0);
    lv_obj_set_style_pad_all(result_label_obj, 0, 0);
    lv_obj_set_style_bg_opa(result_label_obj, LV_OPA_TRANSP, 0);

    result_label = lv_label_create(result_label_obj);
    lv_obj_set_size(result_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(result_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(result_label, LABEL_COLOR, 0);
    lv_obj_set_style_text_align(result_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(result_label, "= 0");

    return true;
}

// Function to handle the back button click
bool Calculator::back(void)
{
    notifyCoreClosed();  // Notify the core that the app is closed

    return true;
}

// Function to handle app closure
bool Calculator::close(void)
{
    return true;
}

// Function to initialize the calculator app
bool Calculator::init(void)
{
    return true;
}

// Function to check if the formula starts with zero
bool Calculator::isStartZero(void)
{
    const char *text = lv_label_get_text(formula_label);
    int text_len = strlen(text);

    if ((text_len == 1) && (text[0] == '0')) {
        return true;
    }
    if ((text[text_len - 1] == '0') &&
        (text[text_len - 2] > '9') &&
        (text[text_len - 2] < '0')) {
        return true;
    }

    return false;
}

// Function to check if the formula ends with a number
bool Calculator::isStartNum(void)
{
    const char *text = lv_label_get_text(formula_label);
    int text_len = strlen(text);

    if ((text[text_len - 1] >= '0') && (text[text_len - 1] <= '9')) {
        return true;
    }

    return false;
}

// Function to check if the formula ends with a percent sign
bool Calculator::isStartPercent(void)
{
    const char *text = lv_label_get_text(formula_label);
    int text_len = strlen(text);

    if (text[text_len - 1] == '%') {
        return true;
    }

    return false;
}

// Function to check if adding a dot is legal
bool Calculator::isLegalDot(void)
{
    const char *text = lv_label_get_text(formula_label);
    int text_len = strlen(text);

    while (text_len-- > 0) {
        if (text[text_len] == '.') {
            return false;
        }
        else if ((text[text_len] < '0') ||
                 (text[text_len] > '9')) {
            return true;
        }
    }

    return true;
}

// Function to perform calculations based on the input formula
double Calculator::calculate(const char *input)
{
    vector<double> stk;  // Stack to store intermediate results
    int input_len = strlen(input);
    double num = 0;
    bool dot_flag = false;
    int dot_len = 0;
    char pre_sign = '+';

    for (int i = 0; i < input_len; i++) {
        if (input[i] == '.') {
            dot_flag = true;
            dot_len = 0;
        }
        else if (isdigit(input[i])) {
            if (!dot_flag) {
                num = num * 10 + input[i] - '0';
            }
            else {
                num += (input[i] - '0') / pow(10.0, ++dot_len);
            }
        }
        else if (input[i] == '%') {
            num /= 100.0;
        }
        else if (i != input_len - 1){
            dot_flag = false;
            dot_len = 0;
            switch (pre_sign) {
                case '+':
                    stk.push_back(num);
                    break;
                case '-':
                    stk.push_back(-num);
                    break;
                case 'x':
                    stk.back() *= num;
                    break;
                default:
                    if (num != 0) {
                        stk.back() /= num;
                    }
                    else {
                        return 0;
                    }
            }
            num = 0;
            pre_sign = input[i];
        }

        if (i == input_len - 1) {
            switch (pre_sign) {
                case '+':
                    stk.push_back(num);
                    break;
                case '-':
                    stk.push_back(-num);
                    break;
                case 'x':
                    stk.back() *= num;
                    break;
                default:
                    if (num != 0) {
                        stk.back() /= num;
                    }
                    else {
                        return 0;
                    }
            }
            num = 0;
        }
    }

    for (int i = 0; i < stk.size(); i++) {
        num += stk.at(i);
    }

    return num;
}

// Callback function for keyboard events
void Calculator::keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    Calculator *app= (Calculator *)lv_event_get_user_data(e);

    if (code == LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t * dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_draw_part_dsc(e);

        // Customize the appearance of specific buttons
        if(dsc->class_p == &lv_btnmatrix_class && dsc->type == LV_BTNMATRIX_DRAW_PART_BTN) {
            if((dsc->id == 0) || (dsc->id == 1) ||
               (dsc->id == 2) || (dsc->id == 3) ||
               (dsc->id == 7) || (dsc->id == 11) ||
               (dsc->id == 15)) {
                dsc->label_dsc->color = KEYBOARD_SPECIAL_COLOR;
            }
        }
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        int btn_id = lv_btnmatrix_get_selected_btn(app->keyboard);
        bool calculate_flag = false;
        bool equal_flag = false;
        double res_num;
        char res_str[32];
        char history_str[32];

        // Adjust font sizes for formula and result labels
        if (lv_obj_get_style_text_font(app->formula_label, 0) == LABEL_FONT_SMALL) {
            lv_obj_set_style_text_font(app->formula_label, LABEL_FONT_BIG, 0);
            lv_obj_set_style_text_font(app->result_label, LABEL_FONT_SMALL, 0);
        }

        // Handle button actions based on button ID
        switch (btn_id) {
        case 0:  // "C" button clears the formula
            lv_label_set_text(app->formula_label, "0");
            app->formula_len = 1;
            calculate_flag = true;
            break;
        case 3:  // Backspace button removes the last character
            if ((app->formula_len == 1) && app->isStartZero()) {
                break;
            }
            lv_label_cut_text(
                app->formula_label,
                --(app->formula_len),
                1
            );
            if (app->formula_len == 0) {
                lv_label_set_text(app->formula_label, "0");
                app->formula_len = 1;
            }
            calculate_flag = true;
            break;
        case 18:  // "=" button triggers calculation
            calculate_flag = true;
            equal_flag = true;
            break;
        case 1:  // "/", "x", "-", "+", "%" buttons
        case 2:
        case 7:
        case 11:
        case 15:
            if (app->isStartPercent() || app->isStartNum()) {
                if (((btn_id == 7) || (btn_id == 11)) && app->isStartZero()) {
                    lv_label_cut_text(
                        app->formula_label,
                        --(app->formula_len),
                        1
                    );
                }
                lv_label_ins_text(
                    app->formula_label,
                    app->formula_len++,
                    lv_btnmatrix_get_btn_text(app->keyboard, btn_id)
                );
                if (btn_id == 15) {
                    calculate_flag = true;
                }
            }
            break;
        case 4:  // Number buttons
        case 5:
        case 6:
        case 8:
        case 9:
        case 10:
        case 12:
        case 13:
        case 14:
        case 16:
            if (app->isStartZero()) {
                lv_label_cut_text(
                    app->formula_label,
                    --(app->formula_len),
                    1
                );
            }
            if (!app->isStartPercent()) {
                lv_label_ins_text(
                    app->formula_label,
                    app->formula_len++,
                    lv_btnmatrix_get_btn_text(app->keyboard, btn_id)
                );
                calculate_flag = true;
            }
            break;
        case 17:  // "." button adds a decimal point
            if (app->isLegalDot() && app->isStartNum()) {
                lv_label_ins_text(
                    app->formula_label,
                    app->formula_len++,
                    "."
                );
            }
            break;
        default:
            break;
        }

#if defined(CONFIG_CALCULATOR_LOG)
        ESP_LOGI(TAG, "keyboard_event_cb btn_id:%d",btn_id);
#endif
        // Perform calculation if needed
        if (calculate_flag) {
            lv_obj_set_style_text_font(app->formula_label, LABEL_FONT_BIG, 0);

            res_num = app->calculate(lv_label_get_text(app->formula_label));
            if (int(res_num) == res_num) {
                snprintf(res_str, sizeof(res_str) - 1, "%ld", long(res_num));
            }
            else {
                snprintf(res_str, sizeof(res_str) - 1, "%.6f", res_num);
            }
            lv_label_set_text_fmt(app->result_label, "= %s", res_str);
            lv_obj_set_style_text_font(app->result_label, LABEL_FONT_SMALL, 0);
        }

        // Update history and formula if "=" button was pressed
        if (equal_flag) {
            lv_obj_set_style_text_font(app->result_label, LABEL_FONT_BIG, 0);

            snprintf(history_str, sizeof(history_str) - 1, "\n%s = %s ", lv_label_get_text(app->formula_label), res_str);
            lv_textarea_set_cursor_pos(app->history_label, strlen(lv_textarea_get_text(app->history_label)));
            lv_textarea_add_text(app->history_label, history_str);

            lv_label_set_text_fmt(app->formula_label, "%s", res_str);
            lv_obj_set_style_text_font(app->formula_label, LABEL_FONT_SMALL, 0);
            app->formula_len = strlen(history_str);
        }
    }
}