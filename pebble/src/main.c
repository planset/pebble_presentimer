#include <pebble.h>

#define debug(...) APP_LOG(APP_LOG_LEVEL_DEBUG, __VA_ARGS__);

const int LONG_CLICK_DELAY_MS = 500; // A value of 0 means to user the system default 500ms.

static Window *window;

static TextLayer *tl_title;
static TextLayer *tl_state;
static TextLayer *tl_timer_minutes;
static TextLayer *tl_timer_seconds;
static TextLayer *tl_keynote_control;

static ActionBarLayer *action_bar;

static GBitmap *action_icon_back;
static GBitmap *action_icon_next;
static GBitmap *action_icon_down;
static GBitmap *action_icon_up;
static GBitmap *action_icon_back_timer;
static GBitmap *action_icon_pause;
static GBitmap *action_icon_cancel;
static GBitmap *action_icon_play;
static GBitmap *action_icon_keynote;
static GBitmap *action_icon_setting_timer;
static GBitmap *action_icon_setting_vibe;

static GFont s_res_gothic_24_bold;
static GFont s_res_bitham_30_black;
static GFont s_res_roboto_condensed_21;
static GFont s_res_gothic_18;


static bool is_running = false;

/*
 * タイマーの設定秒数
 */
static int start_timer_seconds = 0;

/*
 * タイマー開始時の時刻
 */
static int start_time = -1;

/*
 * 定期的にバイブ鳴動する間隔(seconds)
 */
static int notification_vibe_delay = 60;

/*
 * 次に定期的にバイブを鳴動する時間
 */
static int next_notification_vibe = 0;


/*
 * 画面の番号
 */
typedef enum {
    PAGE_MAIN,                  /* メイン画面 */
    PAGE_TIMER_SETTING,         /* タイマー設定画面 */
    PAGE_TIMER_SETTING_VIBE,    /* バイブ鳴動間隔設定画面 */
    PAGE_TIMER_RUNNING,         /* タイマーカウントダウン画面 */
    PAGE_KEYNOTE,               /* キーノート操作画面 */
    PAGE_COUNT
} PageType;

/*
 *       *start*
 *         ↓
 *      PAGE_MAIN ---> [select] ----> PAGE_TIMER_RUNNING  MAINに戻るときは[up]
 *          |                              ↓   ↑
 *          |                           [select long]
 *          |                              ↓   ↑
 *          | <--- [select long] ---> PAGE_TIMER_KEYNOTE
 *          |
 *          | <-- [up] -----> PAGE_TIMER_SETTING
 *          | <-- [down] ---> PAGE_TIMER_SETTING_VIBE
 * 
 */


/*
 * タイマーカウントダウンの動作状態
 */
typedef enum {
    STATE_STOP,     /* 停止中 */
    STATE_RUNNING,  /* カウントダウン中 */
} StateType;

/*
 * キーノートの操作ID
 */
typedef enum {
    KEYNOTE_CONTROL_BACK = 0,
    KEYNOTE_CONTROL_NEXT = 1
} KeynoteControlType;

/*
 * 画面の定義 
 */
typedef struct {
    PageType page;

    GBitmap *icon_up;
    GBitmap *icon_select;
    GBitmap *icon_down;

    void (*up_single_click_handler)(ClickRecognizerRef , void *);
    void (*select_single_click_handler)(ClickRecognizerRef , void *);
    void (*down_single_click_handler)(ClickRecognizerRef , void *);

    void (*up_long_click_up_handler)(ClickRecognizerRef , void *);
    void (*up_long_click_down_handler)(ClickRecognizerRef , void *);
    void (*select_long_click_up_handler)(ClickRecognizerRef , void *);
    void (*select_long_click_down_handler)(ClickRecognizerRef , void *);
    void (*down_long_click_up_handler)(ClickRecognizerRef , void *);
    void (*down_long_click_down_handler)(ClickRecognizerRef , void *);
} ActionBarApp;

/*
 * 画面データの生成
 */
static void new_ActionBarApp(ActionBarApp *self,
        PageType page,
        GBitmap *icon_up,
        GBitmap *icon_select,
        GBitmap *icon_down,
        void (*up_single_click_handler)(ClickRecognizerRef , void *),
        void (*select_single_click_handler)(ClickRecognizerRef , void *),
        void (*down_single_click_handler)(ClickRecognizerRef , void *),
        void (*up_long_click_up_handler)(ClickRecognizerRef , void *),
        void (*up_long_click_down_handler)(ClickRecognizerRef , void *),
        void (*select_long_click_up_handler)(ClickRecognizerRef , void *),
        void (*select_long_click_down_handler)(ClickRecognizerRef , void *),
        void (*down_long_click_up_handler)(ClickRecognizerRef , void *),
        void (*down_long_click_down_handler)(ClickRecognizerRef , void *)
        ) {

    self->page = page;
    self->icon_up = icon_up;
    self->icon_select = icon_select;
    self->icon_down = icon_down;

    self->up_single_click_handler = up_single_click_handler;
    self->select_single_click_handler = select_single_click_handler;
    self->down_single_click_handler = down_single_click_handler;
    self->up_long_click_up_handler = up_long_click_up_handler;
    self->up_long_click_down_handler = up_long_click_down_handler;
    self->select_long_click_up_handler = select_long_click_up_handler;
    self->select_long_click_down_handler = select_long_click_down_handler;
    self->down_long_click_up_handler = down_long_click_up_handler;
    self->down_long_click_down_handler = down_long_click_down_handler;

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

static ActionBarApp apps[PAGE_COUNT];
static int current_app_index = 0;
static int prev_app_index = 0;
static ActionBarApp *current_app;


static void set_title(PageType);
static void set_state(PageType);
static void set_next_notification_vibe();


/*
 * アクションバーの変更
 */
static void action_bar_app_change_app(int app_index){

    prev_app_index = current_app_index;
    current_app_index = app_index;
    current_app = &apps[app_index];

    action_bar_app_set_icon(current_app);

    set_title(app_index);
    set_state(app_index);

}


/*
 * テキストレイヤーにタイマーの数字を表示
 */
static void update_time(int sec) {
    static char timer_minutes_text[5] = "999:";
    static char timer_seconds_text[3] = "99";

    snprintf(timer_minutes_text, 5, "%d:", sec / 60);
    snprintf(timer_seconds_text, 3, "%d", sec % 60);

    text_layer_set_text(tl_timer_minutes, timer_minutes_text);
    text_layer_set_text(tl_timer_seconds, timer_seconds_text);
}


/*
 * タイトルの設定
 */
static void set_title(PageType pt) {
    static char title[] = "12345678901234567890";
    
    if (pt == PAGE_MAIN) {
        snprintf(title, 20, "TIMER");
    } else if (pt == PAGE_TIMER_SETTING) {
        snprintf(title, 20, "SETTING TIME");
    } else if (pt == PAGE_TIMER_SETTING_VIBE) {
        snprintf(title, 20, "SETTING VIBE");
    } else if (pt == PAGE_TIMER_RUNNING) {
        snprintf(title, 20, "TIMER");
    } else if (pt == PAGE_KEYNOTE) {
        snprintf(title, 20, "KEYNOTE");
    }

    text_layer_set_text(tl_title, title);
}


/*
 * 状態の設定
 */
static void set_state(PageType pt) {
    static char state[] = "12345678901234567890";

    if (pt == PAGE_MAIN)
    {
        is_running = false;
        snprintf(state, 20, "STOP");
    }
    else if (pt == PAGE_TIMER_RUNNING)
    {
        is_running = true;
        snprintf(state, 20, "RUNNING");
    }
    else
    {
        /* no change */
    }

    text_layer_set_text(tl_state, state);
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
 * 上ボタン長押し
 */
static void up_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->up_long_click_down_handler != NULL) {
        current_app->up_long_click_down_handler(recognizer, context);
    }
}
static void up_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->up_long_click_up_handler != NULL) {
        current_app->up_long_click_up_handler(recognizer, context);
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
 * 下ボタン長押し
 */
static void down_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->down_long_click_down_handler != NULL) {
        current_app->down_long_click_down_handler(recognizer, context);
    }
}
static void down_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {
    if (current_app->down_long_click_up_handler != NULL) {
        current_app->down_long_click_up_handler(recognizer, context);
    }
}

/*
 * クリックイベントの登録
 */
static void click_config_provider(void *context){
    window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);

    window_long_click_subscribe(BUTTON_ID_UP, LONG_CLICK_DELAY_MS, 
            up_long_click_down_handler, 
            up_long_click_up_handler);

    window_long_click_subscribe(BUTTON_ID_SELECT, LONG_CLICK_DELAY_MS, 
            select_long_click_down_handler, 
            select_long_click_up_handler);

    window_long_click_subscribe(BUTTON_ID_DOWN, LONG_CLICK_DELAY_MS, 
            down_long_click_down_handler, 
            down_long_click_up_handler);
}

/*
 * データの保存
 */
static void save_data(){
    persist_write_bool(0, is_running);
    persist_write_int(1, start_time);
    persist_write_int(2, start_timer_seconds);
}

/*
 * データの読み込み
 */
static void read_data(){
    is_running = persist_read_bool(0);
    start_time = persist_read_int(1);
    start_timer_seconds = persist_read_int(2);
}

/* 
 * 残り時間を返す
 */
static int get_remaining_seconds(){
    int sec = start_timer_seconds - (time(NULL) - start_time);
    return sec < 0 ? 0 : sec;
}

/*
 * カウントダウンタイマーの表示を更新する
 */
static void update_timer_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    
    int sec = get_remaining_seconds();

    debug("diffsec = %d", sec);

    update_time(sec);

    if (sec <= next_notification_vibe) {
        set_next_notification_vibe();
        vibes_short_pulse();
    }

    if (sec <= 0) {
        tick_timer_service_unsubscribe();
        vibes_short_pulse();
    }

}

/*
 * 次の定期通知バイブの時間を決める
 */
static void set_next_notification_vibe(){
    next_notification_vibe = get_remaining_seconds() - notification_vibe_delay;
}

/* 
 * カウントダウンタイマーを開始する
 */
static void start_timer(){
    set_next_notification_vibe();
    tick_timer_service_subscribe(SECOND_UNIT, update_timer_tick_handler);
} 

/* ================================================================================
 * 
 * MAIN PAGE
 * 
 * ================================================================================ */

/*
 * カウントダウンを開始する
 */
static void MAIN_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    // 現在時刻を取ってストレージに保存、1秒毎に画面更新するようにTimerを開始
    start_time = (int)time(NULL);

    debug("start_time = %d", start_time);

    start_timer();

    action_bar_app_change_app(PAGE_TIMER_RUNNING);
}

/*
 * カウントダウンタイマーの設定画面を開く
 */
static void MAIN_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(PAGE_TIMER_SETTING);
}

/*
 * バイブ鳴動間隔設定画面を開く
 */
static void MAIN_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(PAGE_TIMER_SETTING_VIBE);
    update_time(notification_vibe_delay);
}

/*
 * キーノート操作モードに切り替える
 */
static void MAIN_select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(PAGE_KEYNOTE);
    vibes_short_pulse();
}



/* ================================================================================
 * 
 * SETTING TIMER
 * 
 * ================================================================================ */

/*
 * PAGE_MAIN に戻る
 */
static void TIMER_SETTING_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {

    action_bar_app_change_app(PAGE_MAIN);
}

/*
 * カウントを1分増やす
 */
static void TIMER_SETTING_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (start_timer_seconds == 1000 * 60){
        vibes_short_pulse();
        return;
    }

    if (start_timer_seconds < 1000 * 60 - 60) {
        start_timer_seconds += 60 - start_timer_seconds % 60;
    } else {
        start_timer_seconds = 1000 * 60;
    }
    
    update_time(start_timer_seconds);
}

/*
 * カウントを1分減らす
 */
static void TIMER_SETTING_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (start_timer_seconds == 60){
        vibes_short_pulse();
        return;
    }

    if (start_timer_seconds > 60) {
        start_timer_seconds -= (start_timer_seconds % 60 ? start_timer_seconds % 60 : 60);
    } else {
        start_timer_seconds = 60;
    }
    
    update_time(start_timer_seconds);
}


/* ================================================================================
 * 
 * SETTING_VIBE
 * 
 * ================================================================================ */

/*
 * 上ボタン短押下：バイブ鳴動設定値を増やす
 */
static void TIMER_SETTING_VIBE_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {

    if (notification_vibe_delay < 60 * 60 - 60) {
        notification_vibe_delay += 60 - notification_vibe_delay % 60;
    } else {
        notification_vibe_delay = 60 * 60;
    }

    update_time(notification_vibe_delay);

}

/*
 * 中ボタン短押下：保存して戻る
 */
static void TIMER_SETTING_VIBE_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(PAGE_MAIN);
    update_time(start_timer_seconds);
}

/*
 * 下ボタン短押下：バイブ鳴動設定値を減らす
 */
static void TIMER_SETTING_VIBE_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {

    if (notification_vibe_delay > 60 ) {
        notification_vibe_delay -= 60;
    } else {
        notification_vibe_delay = 60;
    }
    update_time(notification_vibe_delay);

}


/* ================================================================================
 * 
 * Count down timer running application
 * 
 * ================================================================================ */

/*
 * キャンセル
 */
static void TIMER_RUNNING_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    start_timer_seconds = get_remaining_seconds();
    update_time(start_timer_seconds);
    tick_timer_service_unsubscribe();
    action_bar_app_change_app(PAGE_MAIN);
}

/*
 * 一時停止
 */
static void TIMER_RUNNING_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    // いらないかも
}

/*
 * キーノート操作モードに切り替える
 */
static void TIMER_RUNNING_select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    action_bar_app_change_app(PAGE_KEYNOTE);
    vibes_short_pulse();
}


/* ================================================================================
 * 
 * Control keynote application
 * 
 * ================================================================================ */

/*
 * スライドを1枚戻す
 */
static void KEYNOTE_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    text_layer_set_text(tl_keynote_control, "Back");
    control_keynote(KEYNOTE_CONTROL_BACK);
}

/*
 * スライドを1枚進める
 */
static void KEYNOTE_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    text_layer_set_text(tl_keynote_control, "Next");
    control_keynote(KEYNOTE_CONTROL_NEXT);
}

/*
 * タイマー設定モードまたはタイマーモードに切り替える
 */
static void KEYNOTE_select_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
    text_layer_set_text(tl_keynote_control, "");
    action_bar_app_change_app(prev_app_index);
    vibes_short_pulse();
}


static void window_load(Window *window) {

    read_data();

    Layer *window_layer = window_get_root_layer(window);
    
    // tl_title
    tl_title = text_layer_create(GRect(0, 0, 144, 25));
    text_layer_set_text(tl_title, "TITLE");
    text_layer_set_font(tl_title, s_res_gothic_24_bold);
    layer_add_child(window_get_root_layer(window), (Layer *)tl_title);

    // tl_state
    tl_state = text_layer_create(GRect(0, 25, 144, 20));
    text_layer_set_text(tl_state, "STATE");
    text_layer_set_font(tl_state, s_res_gothic_18);
    layer_add_child(window_get_root_layer(window), (Layer *)tl_state);

    // tl_timer_minutes
    tl_timer_minutes = text_layer_create(GRect(2, 55, 65, 35));
    text_layer_set_text(tl_timer_minutes, "999:");
    text_layer_set_font(tl_timer_minutes, s_res_bitham_30_black);
    text_layer_set_text_alignment(tl_timer_minutes, GTextAlignmentRight);
    layer_add_child(window_layer, (Layer *)tl_timer_minutes);

    // tl_timer_seconds
    tl_timer_seconds = text_layer_create(GRect(67, 55, 45, 35));
    text_layer_set_text(tl_timer_seconds, "99");
    text_layer_set_font(tl_timer_seconds, s_res_bitham_30_black);
    text_layer_set_text_alignment(tl_timer_seconds, GTextAlignmentRight);
    layer_add_child(window_layer, (Layer *)tl_timer_seconds);

    // text_layer
    tl_keynote_control = text_layer_create(GRect(20, 110, 100, 30));
    text_layer_set_text(tl_keynote_control, "");
    text_layer_set_text_alignment(tl_keynote_control, GTextAlignmentCenter);
    text_layer_set_font(tl_keynote_control, s_res_roboto_condensed_21);
    layer_add_child(window_layer, (Layer *)tl_keynote_control);

    // action bar
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, window);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);

    // 状態の復元
    if (is_running == STATE_RUNNING) {
        action_bar_app_change_app(PAGE_TIMER_RUNNING);
        start_timer();
        update_time(get_remaining_seconds());
    } else {
        action_bar_app_change_app(PAGE_MAIN);
        update_time(start_timer_seconds);
    }

}

static void window_unload(Window *window) {
    save_data();
    text_layer_destroy(tl_state);
    text_layer_destroy(tl_title);
    text_layer_destroy(tl_keynote_control);
    text_layer_destroy(tl_timer_minutes);
    text_layer_destroy(tl_timer_seconds);
    action_bar_layer_destroy(action_bar);
}

static void create_resource(){
    s_res_bitham_30_black = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
    s_res_roboto_condensed_21 = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);
    s_res_gothic_18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    s_res_gothic_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
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
    action_icon_pause = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_PAUSE);
    action_icon_cancel = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_CANCEL);
    action_icon_play = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_PLAY);
    action_icon_keynote = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_KEYNOTE);
    action_icon_setting_timer = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_SETTING_TIMER);
    action_icon_setting_vibe = gbitmap_create_with_resource(
            RESOURCE_ID_IMAGE_ACTION_ICON_SETTING_VIBE);
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
    // MAIN
    new_ActionBarApp(&apps[PAGE_MAIN],
            PAGE_MAIN,
            action_icon_setting_timer,
            action_icon_play,
            action_icon_setting_vibe,
            &MAIN_up_single_click_handler,
            &MAIN_select_single_click_handler,
            &MAIN_down_single_click_handler,
            NULL,
            NULL,
            NULL,
            &MAIN_select_long_click_down_handler,
            NULL,
            NULL
            );

    // TIMER SETTING
    new_ActionBarApp(&apps[PAGE_TIMER_SETTING],
            PAGE_TIMER_SETTING,
            action_icon_up,
            action_icon_back_timer,
            action_icon_down,
            &TIMER_SETTING_up_single_click_handler,
            &TIMER_SETTING_select_single_click_handler,
            &TIMER_SETTING_down_single_click_handler,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            );

    // TIMER SETTING VIBE
    new_ActionBarApp(&apps[PAGE_TIMER_SETTING_VIBE],
            PAGE_TIMER_SETTING_VIBE,
            action_icon_up,
            action_icon_back,
            action_icon_down,
            &TIMER_SETTING_VIBE_up_single_click_handler,
            &TIMER_SETTING_VIBE_select_single_click_handler,
            &TIMER_SETTING_VIBE_down_single_click_handler,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            );

    // TIMER RUNNING
    new_ActionBarApp(&apps[PAGE_TIMER_RUNNING],
            PAGE_TIMER_RUNNING,
            action_icon_cancel,
            action_icon_keynote,
            NULL, /* action_icon_pause,*/
            &TIMER_RUNNING_up_single_click_handler,
            NULL,
            &TIMER_RUNNING_down_single_click_handler,
            NULL,
            NULL,
            NULL,
            &TIMER_RUNNING_select_long_click_down_handler,
            NULL,
            NULL
            );

    // Keynote
    new_ActionBarApp(&apps[PAGE_KEYNOTE],
            PAGE_KEYNOTE,
            action_icon_back,
            action_icon_back_timer,
            action_icon_next,

            &KEYNOTE_up_single_click_handler,
            NULL,
            &KEYNOTE_down_single_click_handler,

            NULL,   /* up long up */
            NULL,   /* up long down */
            NULL,   /*  select long up  */
            &KEYNOTE_select_long_click_down_handler, /*  select long down */
            NULL,   /* down long up */
            NULL    /* down long down */
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
