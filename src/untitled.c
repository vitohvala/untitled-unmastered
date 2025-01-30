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
draw_rectangle(UntitledOffscreenBuffer *screen_buffer, RectangleF32 *rect,
                        f32 r, f32 g, f32 b) 
{
    u32 color = (u32)(r * 255.0f) << 16 |
                (u32)(g * 255.0f) << 8 | 
                (u32)(b * 255.0f) << 0;

    u32 minX = (u32)rect->x;
    u32 minY = (u32)rect->y;
    u32 maxX = (u32)rect->w;
    u32 maxY = (u32)rect->h;

    if(minX < 0) minX = 0;
    if(minY < 0) minY = 0;
    if((int)maxX > screen_buffer->width) maxX = screen_buffer->width;
    if((int)maxY > screen_buffer->height) maxY = screen_buffer->height;
    
    int pitch = screen_buffer->width * BYTES_PER_PIXEL;

    u8 *Row = ((u8 *)screen_buffer->memory + minX * BYTES_PER_PIXEL + minY * pitch);
    for (u32 y = minY; y < maxY; ++y) {
        u32 *Pixel = (u32 *)Row;
        for(u32 x = minX; x < maxX; ++x){
            *Pixel++ = color; 
        }
        Row += pitch;
    }  
}

internal void 
untitled_update_sound_buffer(UntitledSoundBuffer *sound_buffer, 
                             UntitledState *state)
{
    int tone_volume = 3000;
    i16 *sample_out = sound_buffer->sample_out;
    int wave_period = sound_buffer->samples_per_second / state->tone_hz;
    local_persist f32 tsine = 0;

    for(int i = 0; i < sound_buffer->sample_count; i++) {
        f32 sine_value = sinf(tsine);
        i16 value = (i16)(sine_value * tone_volume);
        *sample_out++ = value;
        *sample_out++ = value;
        tsine += 2.0f * PI32 * 1.0f / (f32)wave_period;
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

    draw_rectangle(screen_buffer, 
            &(RectangleF32){.x = 0.0f, .y = 0.0f, 
                            .w = (f32)screen_buffer->width, .h = (f32)screen_buffer->height},
            1.0f, 0.0f, 0.0f);
    
    draw_rectangle(screen_buffer, 
            &(RectangleF32){.x = (f32)game_state->xoffset, .y = 10.0f, 
            .w = (f32)game_state->xoffset + 50.0f, .h = 50.0f},
            0.5f, 0.7f, 0.0f);
    
    //untitled_update_render(screen_buffer, game_state);
    untitled_update_sound_buffer(sound_buffer, game_state); 
}
