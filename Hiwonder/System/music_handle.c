/**
 * @file music_handle.c
 * @brief 蜂鸣器旋律播放，KEY1 触发《歌唱祖国》(C调)
 */

#include "cmsis_os2.h"
#include "global.h"
#include "music_handle.h"

typedef struct {
    uint16_t freq;
    uint16_t duration_ms;
} MusicNote;

/*
 * 《歌唱祖国》主旋律 (C大调, 1=C)，128 音符，由 F 调整体移低纯五度
 * 频率与小星星一致: Do=262 Re=294 Mi=330 Fa=349 Sol=392 La=440 Si=494, 高八度 +1 倍
 */
static const MusicNote sing_motherland[] = {
    { 262,  300}, { 262,  100}, { 262,  400}, { 196,  400},
    { 330,  400}, { 262,  400}, { 392,  600}, { 440,  200},
    { 392,  400}, { 392,  300}, { 392,  100}, { 440,  400},
    { 440,  400}, { 440,  300}, { 392,  100}, { 349,  200},
    { 440,  200}, { 392,  800}, { 392,  400}, { 392,  300},
    { 392,  100}, { 440,  400}, { 440,  400}, { 294,  400},
    { 294,  300}, { 294,  100}, { 392,  600}, { 349,  200},
    { 330,  400}, { 196,  300}, { 196,  100}, { 392,  400},
    { 392,  200}, { 440,  200}, { 392,  200}, { 349,  200},
    { 330,  200}, { 294,  200}, { 262,  800}, { 262,  400},
    { 392,  300}, { 392,  100}, { 523,  400}, { 523,  400},
    { 440,  400}, { 440,  300}, { 392,  100}, { 349,  600},
    { 392,  200}, { 440,  400}, { 294,  300}, { 294,  100},
    { 392,  400}, { 392,  200}, { 440,  200}, { 392,  200},
    { 349,  200}, { 330,  200}, { 294,  200}, { 262,  800},
    { 262,  600}, {   0,  200}, { 262,  300}, { 196,  100},
    { 330,  400}, { 330,  600}, {   0,  200}, { 330,  300},
    { 262,  100}, { 440,  400}, { 440,  600}, {   0,  200},
    { 220,  600}, { 220,  200}, { 294,  400}, { 294,  300},
    { 330,  100}, { 294,  200}, { 262,  200}, { 262,  200},
    { 220,  200}, { 196,  800}, { 262,  400}, { 196,  400},
    { 220,  200}, { 220,  400}, { 196,  200}, { 262,  400},
    { 294,  400}, { 330,  400}, {   0,  400}, { 294,  400},
    { 440,  200}, { 440,  200}, { 392,  200}, { 392,  400},
    { 330,  200}, { 294,  400}, { 440,  400}, { 392,  400},
    {   0,  200}, { 523,  200}, { 523,  300}, { 523,  100},
    { 523,  200}, { 392,  200}, { 440,  600}, { 262,  200},
    { 440,  300}, { 392,  100}, { 349,  200}, { 440,  200},
    { 392,  400}, {   0,  400}, { 523,  300}, { 523,  100},
    { 523,  200}, { 523,  200}, { 392,  400}, { 392,  200},
    { 440,  200}, { 392,  200}, { 349,  200}, { 330,  200},
    { 294,  200}, { 262,  400}, { 196,  300}, { 196,  100},
};

#define MUSIC_PLAY_FLAG 0x01u

static osThreadId_t music_task_handle;
static volatile bool music_playing;

/* 进行曲风格：按谱面完整时值发声 */
static void music_play_note(uint16_t freq, uint16_t duration_ms)
{
    if(freq > 0) {
        buzzer_on(buzzers[0], freq);
        osDelay(duration_ms);
        buzzer_off(buzzers[0]);
    } else {
        osDelay(duration_ms);
    }
}

static void music_play_notes(const MusicNote *notes, uint32_t count)
{
    music_playing = true;
    for(uint32_t i = 0; i < count; ++i) {
        music_play_note(notes[i].freq, notes[i].duration_ms);
    }
    buzzer_off(buzzers[0]);
    music_playing = false;
}

static void music_task_entry(void *argument)
{
    (void)argument;
    for(;;) {
        osThreadFlagsWait(MUSIC_PLAY_FLAG, osFlagsWaitAny, osWaitForever);
        osThreadFlagsClear(MUSIC_PLAY_FLAG);
        music_play_notes(sing_motherland,
                         sizeof(sing_motherland) / sizeof(sing_motherland[0]));
    }
}

static void music_play_sing_motherland_request(void)
{
    if(music_playing) {
        return;
    }
    osThreadFlagsSet(music_task_handle, MUSIC_PLAY_FLAG);
}

void music_key1_callback(ButtonObjectTypeDef *self, ButtonEventIDEnum event)
{
    if(self->id == 1 && event == BUTTON_EVENT_CLICK) {
        music_play_sing_motherland_request();
    }
}

void music_play_init(void)
{
    const osThreadAttr_t music_task_attributes = {
        .name = "music_task",
        .stack_size = 256 * 4,
        .priority = osPriorityBelowNormal,
    };
    music_task_handle = osThreadNew(music_task_entry, NULL, &music_task_attributes);
}
