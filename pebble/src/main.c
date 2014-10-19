#include <pebble.h>

const int LONG_CLICK_DELAY_MS = 500; // A value of 0 means to user the system default 500ms.

static Window *window;
static TextLayer *text_layer;
static ActionBarLayer *action_bar;

static GBitmap *action_icon_back;
static GBitmap *action_icon_next;
static GBitmap *action_icon_down;
static GBitmap *action_icon_up;
static GBitmap *action_icon_back_timer;

static GFont s_res_bitham_30_black;
static GFont s_res_roboto_condensed_21;

static TextLayer *tl_timer_minutes;
static TextLayer *tl_timer_seconds;

static int timer_minutes = 5;
static int timer_seconds = 0;

typedef enum {
    KEYNOTE_CONTROL_BACK = 0,
    KEYNOTE_CONTROL_NEXT = 1
} KeynoteControlType;

typedef struct {
    GBitmap *icon_up;
    GBitmap *icon_select;
    GBitmap *icon_down;
    void (*up_single_click_handler)(ClickRecognizerRef , void *);
    void (*select_single_click_handler)(ClickRecognizerRef , void *);
    void (*down_single_click_handler)(ClickRecognizerRef , void *);
    void (*select_long_click_up_handler)(ClickRecognizerRef , void *);
    void (*select_long_click_down_handler)(ClickRecognizerRef , void *);
} ActionBarApp;

static void new_ActionBarApp(ActionBarApp *self,
        GBitmap *icon_up,
        GBitmap *icon_select,
        GBitmap *icon_down,
        void (*up_single_click_handler)(ClickRecognizerRef , void *),
        void (*select_single_click_handler)(ClickRecognizerRef , void *),
        void (*down_single_click_handler)(ClickRecognizerRef , void *),
        void (*select_long_click_up_handler)(ClickRecognizerRef , void *),
        void (*select_long_click_down_handler)(ClickRecognizerRef , void *)
        ) {

    self->icon_up = icon_up;
    self->icon_select = icon_select;
    self->icon_down = icon_down;

    self->up_single_click_handler = up_single_click_handler;
    self->select_single_click_handler = select_single_click_handler;
    self->down_single_click_handler = down_single_click_handler;
    self->select_long_click_up_handler = select_long_click_up_handler;
    self->select_long_click_down_handler = select_long_click_down_handler;

}

static void action_bar_app_set_icon(ActionBarApp *app){

    if (app->icon_up){
        action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, app->icon_up);
    } else {
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
    }

    if (app->icon_select){
        action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, app->icon_select);
    } else {
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_SELECT);
    }

    if (app->icon_down){
        action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, app->icon_down);
    } else {
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
    }

}

static ActionBarApp apps[3];
static int current_app_index = 0;
static ActionBarApp *current_app;

#define debug(...) APP_LOG(APP_LOG_LEVEL_DEBUG, __VA_ARGS__);

/*
 * アクションバーの変更
 */
static void action_bar_app_change_app(int app_index){

    current_app_index = app_index;
    current_app = &apps[app_index];

    action_bar_app_set_icon(current_app);
}


static void update_timer() {
    static char timer_minutes_text[5] = "999:";
    static char timer_seconds_text[3] = "99";

    snprintf(timer_minutes_text, 5, "%d:", timer_minutes);
    snprintf(timer_seconds_text, 3, "%d", timer_seconds);
    text_layer_set_text(tl_timer_minutes, timer_minutes_text);
    text_layer_set_text(tl_timer_seconds, timer_seconds_text);
}


/*
 * JSを経由してキーノートを操作する
 */
static void control_keynote(int control) {
    Tuplet value = TupletInteger(0, control);

    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    if (iter == NULL) {
        return;
    }

    dict_write_tuplet(iter, &value);
    dict_write_end(iter);

    app_message_outbox_send();
}


/* */

/*
 * 真ん中ボタンクリック
 */
static void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->select_single_click_handler != NULL) {
        current_app->select_single_click_handler(recognizer, context);
    }
}

/*
 * 上ボタンクリック
 */
static void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->up_single_click_handler != NULL) {
        current_app->up_single_click_handler(recognizer, context);
    }
}

/*
 * 下ボタンクリック
 */
static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->down_single_click_handler != NULL) {
        current_app->down_single_click_handler(recognizer, context);
    }
}

/*
 * 真ん中ボタン長押し
 */
static void select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->select_long_click_down_handler != NULL) {
        current_app->select_long_click_down_handler(recognizer, context);
    }
}
static void select_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->select_long_click_up_handler != NULL) {
        current_app->select_long_click_up_handler(recognizer, context);
    }
}

/*
 * クリックイベントの登録
 */
static void click_config_provider(void *context){
    window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);

    window_long_click_subscribe(BUTTON_ID_SELECT, LONG_CLICK_DELAY_MS, 
            select_long_click_down_handler, 
            select_long_click_up_handler);
}

static void save_time(){
    persist_write_int(0, timer_minutes);
    persist_write_int(1, timer_seconds);
}
static void read_time(){
    timer_minutes = persist_read_int(0);
    timer_seconds = persist_read_int(1);
}

/* Count down timer setting application */

/*
 * カウントダウンを開始する
 */
static void TIMER_SETTING_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {

    // 現在時刻を取ってストレージに保存、1秒毎に画面更新するようにTimerを開始
    
    timer_minutes = 100;
    timer_seconds = 59;
    update_timer();

}

/*
 * カウントを1分増やす
 */
static void TIMER_SETTING_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (timer_minutes < 1000) {
        timer_minutes += 1;
        timer_seconds = 0;
        update_timer();
        save_time();
    } else {
        vibes_short_pulse();
    }
}

/*
 * カウントを1分減らす
 */
static void TIMER_SETTING_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (timer_minutes > 0) {
        timer_minutes -= 1;
        timer_seconds = 0;
        update_timer();
        save_time();
    } else {
        vibes_short_pulse();
    }
}

/*
 * キーノート操作モードに切り替える
 */
static void TIMER_SETTING_select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(2);
    vibes_short_pulse();
}
static void TIMER_SETTING_select_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {

}


/* Count down timer running application */

/*
 * キャンセル
 */
static void TIMER_RUNNING_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
}

/*
 * 一時停止
 */
static void TIMER_RUNNING_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
}

/*
 * キーノート操作モードに切り替える
 */
static void TIMER_RUNNING_select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(2);
    vibes_short_pulse();
}
static void TIMER_RUNNING_select_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {

}


/* Control keynote application */

/*
 * スライドを1枚戻す
 */
static void KEYNOTE_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    text_layer_set_text(text_layer, "Back");
    control_keynote(KEYNOTE_CONTROL_BACK);
}

/*
 * スライドを1枚進める
 */
static void KEYNOTE_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    text_layer_set_text(text_layer, "Next");
    control_keynote(KEYNOTE_CONTROL_NEXT);
}

/*
 * タイマー設定モードまたはタイマーモードに切り替える
 */
static void KEYNOTE_select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(0);
    vibes_short_pulse();
}
static void KEYNOTE_select_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {

}




static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    //GRect bounds = layer_get_bounds(window_layer);

    read_time();

    // tl_timer_minutes
    tl_timer_minutes = text_layer_create(GRect(2, 55, 70, 35));
    text_layer_set_text(tl_timer_minutes, "999:");
    text_layer_set_font(tl_timer_minutes, s_res_bitham_30_black);
    text_layer_set_text_alignment(tl_timer_minutes, GTextAlignmentRight);
    layer_add_child(window_layer, (Layer *)tl_timer_minutes);

    // tl_timer_seconds
    tl_timer_seconds = text_layer_create(GRect(72, 55, 40, 35));
    text_layer_set_text(tl_timer_seconds, "99");
    text_layer_set_font(tl_timer_seconds, s_res_bitham_30_black);
    text_layer_set_text_alignment(tl_timer_seconds, GTextAlignmentRight);
    layer_add_child(window_layer, (Layer *)tl_timer_seconds);

    update_timer();

    // text_layer
    text_layer = text_layer_create(GRect(20, 110, 100, 30));
    text_layer_set_text(text_layer, "");
    text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
    text_layer_set_font(text_layer, s_res_roboto_condensed_21);
    layer_add_child(window_layer, (Layer *)text_layer);

    // action bar
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, window);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);

    action_bar_app_change_app(0);
}

static void window_unload(Window *window) {
    text_layer_destroy(text_layer);
    action_bar_layer_destroy(action_bar);
}

static void create_resource(){
    s_res_bitham_30_black = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
    s_res_roboto_condensed_21 = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);
    action_icon_back = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_BACK);
    action_icon_next = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_NEXT);
    action_icon_down = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_DOWN);
    action_icon_up = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_UP);
    action_icon_back_timer = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_BACK_TIMER);
}

static void init(void) {
    app_message_open(64, 64);

    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
            .load = window_load,
            .unload = window_unload,
            });

    const bool animated = true;
    window_stack_push(window, animated);
}

static void deinit(void) {
    window_destroy(window);
}

static void create_app() {
    // TIMER SETTING
    new_ActionBarApp(&apps[0],
            action_icon_up,
            NULL,
            action_icon_down,
            &TIMER_SETTING_up_single_click_handler,
            &TIMER_SETTING_select_single_click_handler,
            &TIMER_SETTING_down_single_click_handler,
            &TIMER_SETTING_select_long_click_up_handler,
            &TIMER_SETTING_select_long_click_down_handler
            );
    // TIMER RUNNING
    new_ActionBarApp(&apps[1],
            NULL,
            NULL,
            NULL,
            &TIMER_RUNNING_up_single_click_handler,
            NULL,
            &TIMER_RUNNING_down_single_click_handler,
            &TIMER_RUNNING_select_long_click_up_handler,
            &TIMER_RUNNING_select_long_click_down_handler
            );
    // Keynote
    new_ActionBarApp(&apps[2],
            action_icon_back,
            action_icon_back_timer,
            action_icon_next,
            &KEYNOTE_up_single_click_handler,
            NULL,
            &KEYNOTE_down_single_click_handler,
            &KEYNOTE_select_long_click_up_handler,
            &KEYNOTE_select_long_click_down_handler
            );
}

int main(void) {
    create_resource();

    create_app();

    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}
