#ifndef _UNTITLED_H_
#define _UNTITLED_H_

#include "untitled_types.h"

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#if CLANG_DEBUG
    #define untitled_assert(expression) if(!(expression))  { __builtin_trap(); }
#else 
    #define untitled_assert(expression) if(!(expression))  { *(int*) 0 = 0; }
#endif
#define BYTES_PER_PIXEL 4
#define ArrayCount(x) (sizeof(x) / sizeof((x)[0]))

typedef struct Untiled_offscreen_buffer {
    void *memory;
    int width;
    int height;
} UntitledOffscreenBuffer;

typedef struct {
    i32 samples_per_second;
    i16 *sample_out;
    int sample_count;
} UntitledSoundBuffer;

typedef struct {
    f32 x, y;
    f32 w, h;
} RectangleF32;

typedef struct {
    int xoffset;  
    int yoffset;  
    int tone_hz;
} UntitledState;

typedef struct {
    void *permanent_storage;
    u64 permanent_storage_size;

    void *transient_storage;
    u64 transient_storage_size;

    u8 is_init;
} UntitledMemory;

typedef struct {
    i32 half_transition_count;
    u32 ended_down;
}UntitledButtonState ;

typedef struct {

    union {
        UntitledButtonState buttons[6];
        struct {
            UntitledButtonState up;
            UntitledButtonState down;
            UntitledButtonState right;
            UntitledButtonState left;
            UntitledButtonState left_shoulder;
            UntitledButtonState right_shoulder;
        };
    };

    f32 averageX;
    f32 averageY;
    
    u8 is_analog;
}UntitledControllerInput;

typedef struct {
    UntitledControllerInput cinput[4];
}UntitledInput;


#define UNTITLED_UPDATE(name) \
    void name(UntitledMemory *memory, UntitledOffscreenBuffer *screen_buffer, \
              UntitledSoundBuffer *sound_buffer, UntitledInput *input)


typedef UNTITLED_UPDATE(UntitledUpdateP);

#endif
