#ifndef _UNTITLED_H_
#define _UNTITLED_H_

#include "untitled_types.h"

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)


#define untitled_assert(expression) if(!(expression)) { *(int*) 0 = 0; }


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
}UntiledButtonState ;

typedef struct {

    union {
        UntiledButtonState buttons[6];
        struct {
            UntiledButtonState up;
            UntiledButtonState down;
            UntiledButtonState right;
            UntiledButtonState left;
            UntiledButtonState left_shoulder;
            UntiledButtonState right_shoulder;
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
