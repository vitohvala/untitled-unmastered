#include "untitled.h"
#include "untitled_types.h"
#include <math.h>


internal void 
untitled_update_render(UntitledOffscreenBuffer *screen_buffer, UntitledState *state)
{
    u8 *Row = (u8 *)screen_buffer->memory;
    int pitch = screen_buffer->width * 4;
    for (int y = 0; y < screen_buffer->height; ++y) {
        u32 *Pixel = (u32 *)Row;
        for(int x = 0; x < screen_buffer->width; ++x){
            u8 Green = (u8)(y + state->yoffset);
            u8 Blue = (u8)(x + state->xoffset);

            *Pixel++ = Green << 16 | Blue; 
        }
        Row += pitch;
    }   
}

internal void 
untitled_update_sound_buffer(UntitledSoundBuffer *sound_buffer, 
                             UntitledState *state)
{
    int tone_volume = 9000;
    i16 *sample_out = sound_buffer->sample_out;
    int wave_period = sound_buffer->samples_per_second / state->tone_hz;
    local_persist f32 tsine;

    for(int i = 0; i < sound_buffer->sample_count; i++) {
        f32 sine_value = sinf(tsine);
        i16 value = (i16)(sine_value * tone_volume);
        *sample_out++ = value;
        *sample_out++ = value;
        tsine += 2.0f * PI32 * (1.0f / (f32)wave_period);
    }

}

UNTITLED_UPDATE(untitled_update_game)
{
    untitled_assert(sizeof(UntitledState) <= memory->permanent_storage_size);
    UntitledState *game_state = (UntitledState *)memory->permanent_storage;

    if(!memory->is_init) {
        game_state->tone_hz = 256;
        game_state->xoffset = 0;
        game_state->yoffset = 0;
        memory->is_init = true;
    }

    UntitledControllerInput input0 = input->cinput[0];

    int delta = 5;

    if(input0.left.ended_down) {
        game_state->xoffset -= delta;
    }
    if(input0.right.ended_down) {
        game_state->xoffset += delta;
    }
    if(input0.down.ended_down) {
        game_state->yoffset += delta;
    }
    if(input0.up.ended_down) {
        game_state->yoffset -= delta;
    }
    if(game_state->yoffset <= 1000 && game_state->yoffset >= -1000) {
        int tmp = (int)(((f32)game_state->yoffset / 1000) * 256) + 512;
        game_state->tone_hz = tmp;
    }

    untitled_update_render(screen_buffer, game_state);
    untitled_update_sound_buffer(sound_buffer, game_state); 
}
