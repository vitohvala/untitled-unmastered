/*
 * 
 *
 * 
 * TODO: 
 *      FILE_IO
 *      OPENGL
 *      USE XAUDIO2 instead
 *      
 * */



#include "untitled.h"
#include "untitled_types.h"


#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>


#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pstate)
#define XINPUT_SET_STATE(name) \
    DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)


typedef XINPUT_GET_STATE(xinput_get_state);
typedef XINPUT_SET_STATE(xinput_set_state);

XINPUT_SET_STATE(XInputSetStateStub) {
    (void)dwUserIndex;
    (void)pVibration;
    return ERROR_DEVICE_NOT_CONNECTED;
}
XINPUT_GET_STATE(XInputGetStateStub) {
    (void)dwUserIndex;
    (void)pstate;
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_var xinput_get_state *_XInputGetState = XInputGetStateStub;
global_var xinput_set_state *_XInputSetState = XInputSetStateStub;
#define XInputGetState _XInputGetState
#define XInputSetState _XInputSetState

#define DIRECT_SOUND_CREATE(name) \
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_var UntitledUpdateP *untitled_update_game;

typedef struct win32_offscreen_buffer_s {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    HDC source_DC;
    HBITMAP source_bitmap;
} Win32OffscreenBuffer;

typedef struct win32_dsound_output_s{
    i32 samples_per_second;
    i32 bytes_per_sample;
    i32 buffer_size;
    u32 running_sample_index;
    int latency_sample_count;
} Win32DSoundOutput;


global_var u8 running = 1;
global_var Win32OffscreenBuffer global_backbuffer;
global_var LARGE_INTEGER global_perf_count_freq; 
global_var HMODULE global_gamecode_dll;
global_var LPDIRECTSOUNDBUFFER sec_buffer;
global_var FILETIME game_dll_last_write;


internal void 
win32_load_xinput(void) 
{
    HMODULE xinput_lib = LoadLibraryA("xinput1_4.dll");

    if(!xinput_lib) {
        xinput_lib = LoadLibraryA("xinput9_1_0.dll");
        if(!xinput_lib)
            xinput_lib = LoadLibraryA("xinput1_3.dll");
    }
    if(xinput_lib) {
        _XInputGetState = (xinput_get_state*)GetProcAddress(xinput_lib, "XInputGetState"); 
        _XInputSetState = (xinput_set_state*)GetProcAddress(xinput_lib, "XInputSetState"); 
    }
}

internal void 
win32_buffer_to_window(Win32OffscreenBuffer *buff, HWND handle)
{
    HDC DeviceContext = GetDC(handle);
    if(!BitBlt(DeviceContext, 
            0, 0, 
            buff->width, buff->height,
            buff->source_DC, 
            0, 0, 
            SRCCOPY)) 
    {
        //printf("bitblt failed");
        exit(2);
    }
    ReleaseDC(handle, DeviceContext);
}

internal void 
win32_resize_DIBSection(Win32OffscreenBuffer *buffer, int width, int height)
{
    if(buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    if (buffer->source_DC) {
        DeleteDC(buffer->source_DC);
    }
    if (buffer->source_bitmap) {
        DeleteObject(buffer->source_bitmap);
    }

    buffer->source_DC = CreateCompatibleDC(NULL);
    buffer->source_bitmap = CreateDIBSection(
            buffer->source_DC, 
            &buffer->info, 
            DIB_RGB_COLORS, 
            &buffer->memory, 
            0, 0);

    SelectObject(buffer->source_DC, buffer->source_bitmap);
}

internal void 
win32_load_dsound(HWND window, i32 samples_per_second, i32 buff_size) 
{
    HMODULE library = LoadLibraryA("dsound.dll");
    if(!library) {
        //log
        return;
    }

    direct_sound_create *DirectSoundCreate = 
        (direct_sound_create*)GetProcAddress(library, "DirectSoundCreate");

    LPDIRECTSOUND direct_sound;

    if(FAILED(DirectSoundCreate(0, &direct_sound, 0))){
        //log
        return;
    }
    if(SUCCEEDED(direct_sound->lpVtbl->SetCooperativeLevel(direct_sound,
                    window, DSSCL_PRIORITY)))
    {
        OutputDebugStringA("Set cooperative level ok\n");
    } else {
        //log
    }

    WAVEFORMATEX wave_format = {0};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 2;
    wave_format.nSamplesPerSec = samples_per_second;
    wave_format.wBitsPerSample = 16;
    wave_format.nBlockAlign = wave_format.nChannels * wave_format.wBitsPerSample / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;

    DSBUFFERDESC buffer_desc = {0};
    buffer_desc.dwSize = sizeof(buffer_desc);
    buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    LPDIRECTSOUNDBUFFER primary_buffer;

    if(SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound,
                                    &buffer_desc, &primary_buffer, 0)))
    {
        OutputDebugStringA("Create Primary buffer ok\n");
        if(SUCCEEDED(primary_buffer->lpVtbl->SetFormat(primary_buffer, &wave_format))) {
            OutputDebugStringA("Primary buffer set foramt ok\n");
        } else {
            //log
        }
    } else {
        
    }

    
    DSBUFFERDESC buffer_desc_sec = {0};
    buffer_desc_sec.dwSize = sizeof(buffer_desc_sec);
    buffer_desc_sec.dwFlags = 0;
    buffer_desc_sec.dwBufferBytes = buff_size;
    buffer_desc_sec.lpwfxFormat = &wave_format;
    
    if(SUCCEEDED(direct_sound->lpVtbl->CreateSoundBuffer(direct_sound, &buffer_desc_sec, 
                    &sec_buffer, 0)))
    {
        OutputDebugStringA("Secondary buffer created\n");
    } else {
        //log
    }
}

internal void 
win32_clear_sound_buffer(Win32DSoundOutput *sound_output) 
{
    void *region1;
    DWORD region1_size;
    void *region2;
    DWORD region2_size;

    if(SUCCEEDED(sec_buffer->lpVtbl->Lock(sec_buffer,
                    0, sound_output->buffer_size,
                    &region1, &region1_size,
                    &region2, &region2_size, 
                    0)))
    {
        u8 *out = (u8*)region1;

        for(DWORD byte_index = 0; byte_index < region1_size; byte_index++) {
            *out++ = 0;
        }

        out = (u8*)region2;

        for(DWORD byte_index = 0; byte_index < region2_size; byte_index++) {
            *out++ = 0;
        }

        sec_buffer->lpVtbl->Unlock(sec_buffer, region1, region1_size,
                                   region2, region2_size);
    }
}

internal void 
win32_fill_sound_buffer(Win32DSoundOutput *sound_output, DWORD bytes_to_lock, 
                        DWORD bytes_to_write, UntitledSoundBuffer *us)
{
    void *region1;
    DWORD region1_size;
    void *region2;
    DWORD region2_size;

    if(SUCCEEDED(sec_buffer->lpVtbl->Lock(sec_buffer, 
                    bytes_to_lock, bytes_to_write, 
                    &region1, &region1_size, 
                    &region2, &region2_size, 0)))
    {
        DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
        i16 *dest_sample = (i16*)region1;
        i16 *src_sample = (i16*)us->sample_out;
        for(DWORD i = 0; i < region1_sample_count; i++) {
            *dest_sample++ = *src_sample++;
            *dest_sample++ = *src_sample++;
            sound_output->running_sample_index++;
        }
        
        dest_sample = (i16*)region2;
        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        for(DWORD i = 0; i < region2_sample_count; i++) {
            *dest_sample++ = *src_sample++;
            *dest_sample++ = *src_sample++;
            sound_output->running_sample_index++;
        }
       
        sec_buffer->lpVtbl->Unlock(sec_buffer, region1, region1_size, region2, region2_size);
    }
}

LRESULT CALLBACK 
win32_win_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            running = 0;
            return 0;
        case WM_CLOSE:
            PostQuitMessage(0);
            running = 0;
            return 0;

        case WM_SIZE:
            { 

            }return 0;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT ClientRect;
                GetClientRect(hwnd, &ClientRect);
                int Width = ClientRect.right - ClientRect.left;
                int Height = ClientRect.bottom - ClientRect.top;
                BitBlt(hdc, 
                        0, 0, 
                        Width, Height,
                        global_backbuffer.source_DC, 
                        0, 0, 
                        SRCCOPY);
                EndPaint(hwnd, &ps);
            }
            return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


static inline FILETIME
win32_get_file_last_writetime(char *FileName) {
    FILETIME Result = {0};
    WIN32_FIND_DATA FindData;
    HANDLE Handle = FindFirstFileA(FileName, &FindData);
    if(Handle != INVALID_HANDLE_VALUE) {
        Result = FindData.ftLastWriteTime;
        FindClose(Handle);
    }

    return Result;
}


internal void 
win32_load_gamecode(char *dll_path, char *dll_tmp_path)
{
    CopyFileA(dll_path, dll_tmp_path, FALSE);

    game_dll_last_write = win32_get_file_last_writetime(dll_path);

    global_gamecode_dll = LoadLibraryA(dll_tmp_path);


    if(global_gamecode_dll) {
        untitled_update_game =
            (UntitledUpdateP *)GetProcAddress(global_gamecode_dll, "untitled_update_game");
    }
}

internal LARGE_INTEGER
win32_get_wall_clock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

internal f32
win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    f32 result = (f32)(end.QuadPart - start.QuadPart) /
                 (f32)global_perf_count_freq.QuadPart;
    
    return result;
}


internal void 
win32_process_XInput_digital(DWORD XInput_button_state, 
                             UntitledButtonState *old_state, DWORD button_bit,
                             UntitledButtonState *new_state)
{
    new_state->ended_down = ((XInput_button_state & button_bit) == button_bit);
    new_state->half_transition_count =
        (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

internal void
win32_process_keyboard_message(UntitledButtonState *new_state, u32 is_down)
{
    untitled_assert(is_down != new_state->ended_down);
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}

internal void 
win32_process_message(UntitledControllerInput *keyboard)
{
    MSG msg = {};
    while(PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
            case WM_QUIT: 
            {
                running = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: 
            {
                u32 vk_code = (u32)msg.wParam;
                u32 was_down = ((msg.lParam & (1 << 30)) != 0);
                u32 is_down = ((msg.lParam & (1 << 31)) == 0);

                if(was_down != is_down) {
                    switch (vk_code) {
                        case 'W': 
                        {
                        } break;
                        case 'A': 
                        {
                        } break;
                        case 'S': 
                        {
                        } break;
                        case 'D': 
                        {
                        } break;
                        case 'E': 
                        {
                            win32_process_keyboard_message(&keyboard->right_shoulder, is_down); 
                        } break;
                        case 'Q': 
                        {
                            win32_process_keyboard_message(&keyboard->left_shoulder, is_down);
                        } break;
                        case VK_UP:
                        {
                            win32_process_keyboard_message(&keyboard->up, is_down);
                        }break;
                        case VK_DOWN:
                        {
                            win32_process_keyboard_message(&keyboard->down, is_down);
                        }break;
                        case VK_RIGHT: 
                        {
                            win32_process_keyboard_message(&keyboard->right, is_down);
                        }break;
                        case VK_LEFT: 
                        {
                            win32_process_keyboard_message(&keyboard->left, is_down);
                        }break;
                        case VK_ESCAPE: 
                        {
                        }break;
                        case VK_SPACE: 
                        {
                        }break;
                        case VK_F4: 
                        {
                            if(msg.lParam & (1 << 29))
                                running = false;
                        } break;
                    } 
                }

            } break;

            default: 
            {
                TranslateMessage(&msg); 
                DispatchMessageA(&msg);
            } break;
        }
    }
}


internal f32
win32_process_XInput_stick_value(SHORT value, SHORT dead_zone)
{
    f32 result = 0;

    if(value < -dead_zone)
    {
        result = (f32)value / 32768.0f;
    }
    else if(value > dead_zone)
    {
        result = (f32)value / 32767.0f;
    }

    return result;
}

int WINAPI 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        PSTR lpCmdLine, int nCmdShow)
{

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    WNDCLASSA wca = {0};
    wca.lpfnWndProc = win32_win_proc;
    wca.hInstance = hInstance;
    wca.lpszClassName = "UntitledClass";
    wca.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; 

    RECT rect = {0, 0, 1280, 720};
    AdjustWindowRect(&rect, WS_VISIBLE | WS_OVERLAPPEDWINDOW, FALSE);

    RegisterClassA(&wca);
    HWND handle = CreateWindowExA(0, wca.lpszClassName, "Untitled",
            WS_VISIBLE | WS_OVERLAPPEDWINDOW, 
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left, rect.bottom - rect.top, 
            0, 0, hInstance, 0);

    if(!handle) {
        //GetLastError();
        return 0; 
    }

    win32_load_xinput();

    Win32DSoundOutput sound_output = {0};
    sound_output.samples_per_second = 48000;
    sound_output.bytes_per_sample = 4;
    sound_output.buffer_size = 
        sound_output.samples_per_second * sound_output.bytes_per_sample;
    sound_output.latency_sample_count = sound_output.samples_per_second / 20;
    sound_output.running_sample_index = 0; 
 
    i16 *sound_memory = (i16*)VirtualAlloc(0, sound_output.buffer_size, 
                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);   

    win32_load_dsound(handle, sound_output.samples_per_second, sound_output.buffer_size);
    win32_clear_sound_buffer(&sound_output);
    sec_buffer->lpVtbl->Play(sec_buffer, 0, 0, DSBPLAY_LOOPING);

    char EXE_path[MAX_PATH];
    DWORD filename_size = GetModuleFileNameA(NULL, EXE_path, sizeof(EXE_path));
    char *OnePastLastSlash = EXE_path;
    for(char *scan = EXE_path + filename_size; ; --scan) {
        if(*scan == '\\') {
            OnePastLastSlash = scan + 1;
            break;
        }
    }
    usize EXEDirLength = OnePastLastSlash - EXE_path;

    char GameDLLName[] = "untitled.dll";
    char GameDLLPath[MAX_PATH];
    strncpy_s(GameDLLPath, sizeof(GameDLLPath), EXE_path, EXEDirLength);
    strncpy_s(GameDLLPath + EXEDirLength, sizeof(GameDLLPath) - EXEDirLength, GameDLLName, sizeof(GameDLLName));

    char GameTempDLLName[] = "untitled_temp.dll";
    char GameTempDLLPath[MAX_PATH];
    strncpy_s(GameTempDLLPath, sizeof(GameTempDLLPath), EXE_path, EXEDirLength);
    strncpy_s(GameTempDLLPath + EXEDirLength, sizeof(GameTempDLLPath) - EXEDirLength, GameTempDLLName, sizeof(GameTempDLLName));

    win32_load_gamecode(GameDLLPath, GameTempDLLPath);

    LARGE_INTEGER last_counter;
    QueryPerformanceFrequency(&global_perf_count_freq);
    QueryPerformanceCounter(&last_counter);

    win32_resize_DIBSection(&global_backbuffer, 1280, 720);

    u8 sleep_is_granular = (timeBeginPeriod(1) == TIMERR_NOERROR);

    int refreshHz = 60;
    int game_updateHz = refreshHz;
    f32 target_seconds = 1.0f / (f32)game_updateHz;

    printf("%f\n", target_seconds);

    UntitledMemory untitled_memory = {0};
    untitled_memory.permanent_storage_size = Megabytes(32);
    untitled_memory.transient_storage_size = Megabytes(128);

    usize total_storage_size = (usize)untitled_memory.permanent_storage_size + 
                               (usize)untitled_memory.transient_storage_size;
    
    untitled_memory.permanent_storage = VirtualAlloc(0, total_storage_size,
                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    untitled_memory.transient_storage = ((u8 *)untitled_memory.permanent_storage + 
                                        untitled_memory.permanent_storage_size);
    
    if(!untitled_memory.permanent_storage || 
       !sound_memory) 
    {
        //log
        return 1;
    }
    untitled_memory.is_init = false;

    UntitledInput input[2] = {0};
    UntitledInput *new_input = &input[1];
    UntitledInput *old_input = &input[0];

    while(running) {
        
        FILETIME tmp_dll_filetime = win32_get_file_last_writetime(GameDLLPath);
    
        if((CompareFileTime(&tmp_dll_filetime, &game_dll_last_write)) != 0) {
            if(global_gamecode_dll) {
                FreeLibrary(global_gamecode_dll);
                global_gamecode_dll = 0;
                untitled_update_game = NULL;
            }
            win32_load_gamecode(GameDLLPath, GameTempDLLPath);
        }

        UntitledControllerInput *new_keyboard = &new_input->cinput[0];
        UntitledControllerInput *old_keyboard = &old_input->cinput[0];
        UntitledControllerInput zero_controller = {0};
        *new_keyboard = zero_controller;
       
        for(int i = 0; i < (int)ArrayCount(new_keyboard->buttons); i++) {
            new_keyboard->buttons[i].ended_down = old_keyboard->buttons[i].ended_down;
        }

        win32_process_message(new_keyboard);
        
        int max_controller_count = XUSER_MAX_COUNT; 
        if(max_controller_count > (int)(ArrayCount(new_input->cinput) - 1))
        {
            max_controller_count = ArrayCount(new_input->cinput) - 1;
        }

        for(int i = 0; i < max_controller_count; i++) {
           XINPUT_STATE  controller_state;

           UntitledControllerInput *old_controller = &old_input->cinput[i + 1];
           UntitledControllerInput *new_controller = &new_input->cinput[i + 1];

            ZeroMemory(&controller_state, sizeof(XINPUT_STATE));
            if(XInputGetState(0, &controller_state) == ERROR_SUCCESS) {
                //plugged in
                XINPUT_GAMEPAD *pad = &controller_state.Gamepad;
                u32 dpad_up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                u32 dpad_down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                u32 dpad_left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                u32 dpad_right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                new_controller->is_analog = true;

                new_controller->averageX = win32_process_XInput_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                new_controller->averageY = win32_process_XInput_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                win32_process_XInput_digital(pad->wButtons,
                        &old_controller->down, XINPUT_GAMEPAD_A, &new_controller->down);
                win32_process_XInput_digital(pad->wButtons,
                        &old_controller->left, XINPUT_GAMEPAD_X, &new_controller->left);
                win32_process_XInput_digital(pad->wButtons,
                        &old_controller->right, XINPUT_GAMEPAD_B, &new_controller->right);
                win32_process_XInput_digital(pad->wButtons,
                        &old_controller->up, XINPUT_GAMEPAD_Y, &new_controller->up);

                win32_process_XInput_digital(pad->wButtons,
                        &old_controller->right_shoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, 
                        &new_controller->right_shoulder);
                win32_process_XInput_digital(pad->wButtons,
                        &old_controller->left_shoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, 
                        &new_controller->left_shoulder);
            }
        }

        DWORD play_cursor;
        DWORD bytes_to_lock = 0;
        DWORD bytes_to_write = 0;
        DWORD write_cursor;
        u8 sound_valid = false;
        if(SUCCEEDED(sec_buffer->lpVtbl->GetCurrentPosition(sec_buffer, &play_cursor, 
                                                            &write_cursor)))
        {
            bytes_to_lock = 
                (sound_output.running_sample_index * sound_output.bytes_per_sample) % 
                sound_output.buffer_size;
            DWORD target = (play_cursor + 
                    (sound_output.latency_sample_count * sound_output.bytes_per_sample))      
                % sound_output.buffer_size;

            if(bytes_to_lock > target) {
                bytes_to_write = (sound_output.buffer_size - bytes_to_lock) + target;
            } else {
                bytes_to_write = target - bytes_to_lock;
            }
            sound_valid = true;

        }

        UntitledOffscreenBuffer untitled_screen_buffer = {0};
        untitled_screen_buffer.memory = global_backbuffer.memory;
        untitled_screen_buffer.width = global_backbuffer.width;
        untitled_screen_buffer.height = global_backbuffer.height;

        UntitledSoundBuffer sbf = {0};
        sbf.samples_per_second = sound_output.samples_per_second;
        sbf.sample_count = bytes_to_write / sound_output.bytes_per_sample;
        sbf.sample_out = sound_memory;

        if(untitled_update_game) {
            untitled_update_game(&untitled_memory, &untitled_screen_buffer, &sbf, new_input);
        }
        else 
            win32_load_gamecode(GameDLLPath, GameTempDLLPath);


        if(sound_valid)
            win32_fill_sound_buffer(&sound_output, bytes_to_lock, bytes_to_write, &sbf);

        
        LARGE_INTEGER end_counter = win32_get_wall_clock();
        f32 seconds_elapsed = win32_get_seconds_elapsed(last_counter, end_counter);
        
        f32 seconds_elapsed_fframe = seconds_elapsed;

        if(seconds_elapsed_fframe < target_seconds) {
            if(sleep_is_granular) {
                DWORD sleep_ms = (DWORD)(1000.0f * (target_seconds - seconds_elapsed_fframe));
                if(sleep_ms > 0) {
                    Sleep(sleep_ms);
                }
            }
            f32 TestSecondsElapsedForFrame = win32_get_seconds_elapsed(last_counter, 
                    win32_get_wall_clock());
            if(TestSecondsElapsedForFrame < target_seconds)
            {
                // TODO(casey): LOG MISSED SLEEP HERE
            }

            while(seconds_elapsed_fframe < target_seconds) {
                seconds_elapsed_fframe = win32_get_seconds_elapsed(last_counter, 
                        win32_get_wall_clock());
            }
        }
        
        win32_buffer_to_window(&global_backbuffer, handle);
    
        end_counter = win32_get_wall_clock();
        f32 ms_per_seconds = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        
        char buffer[100];
        sprintf_s(buffer, 100, "%f\n", ms_per_seconds);
        
        OutputDebugStringA(buffer);

        last_counter = end_counter;

        UntitledInput *tmp_input = old_input;
        old_input = new_input;
        new_input = tmp_input;
    
    }

    return 0;
}
