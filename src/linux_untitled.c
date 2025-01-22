#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>
#include <math.h>

#include "untitled.h"
#include "untitled_types.h"


global_var UntitledUpdateP *untitled_update_game;
global_var struct stat file_stat;
global_var void *global_gamecode_dll;

internal void
linux_set_size_hint(Display* display, Window window,
        int minWidth, int minHeight,
        int maxWidth, int maxHeight)
{
    XSizeHints hints = {};
    if(minWidth > 0 && minHeight > 0) hints.flags |= PMinSize;
    if(maxWidth > 0 && maxHeight > 0) hints.flags |= PMaxSize;

    hints.min_width = minWidth;
    hints.min_height = minHeight;
    hints.max_width = maxWidth;
    hints.max_height = maxHeight;

    XSetWMNormalHints(display, window, &hints);
}

internal u8
copy_file(char *filename1, char *filename2)
{

    FILE *fp_out = fopen(filename1, "rb");
    FILE *fp_in = fopen(filename2, "wb");

    if(!fp_out || !fp_in) return 0;

    char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp_out)) > 0) {
        fwrite(buffer, 1, bytes, fp_in);
    }

    fclose(fp_in);
    fclose(fp_out);

    return 1;
}
  
internal void 
linux_load_gamecode(void)
{
    if((stat("/home/vito/git/untitled-unmastered/linux_bin/untitled.so", &file_stat)) < 0) {
        stat("untitled.so", &file_stat);
    }


    if(!(copy_file("/home/vito/git/untitled-unmastered/linux_bin/untitled.so", 
                    "/home/vito/git/untitled-unmastered/linux_bin/untitled_temp.so"))) {
        copy_file("untitled.so", "untitled_temp.so");
    }

    global_gamecode_dll = dlopen("/home/vito/git/untitled-unmastered/linux_bin/untitled_temp.so", RTLD_NOW);

    if(!global_gamecode_dll) {
        global_gamecode_dll = dlopen("untitled_temp.so", RTLD_NOW);
    }

    if(global_gamecode_dll) {
        untitled_update_game = 
            (UntitledUpdateP *)dlsym(global_gamecode_dll, "untitled_update_game");
    }
}

internal struct timeval
get_timeval(void) 
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv;
}

internal f32
get_seconds_elapsed(struct timeval tv_start, struct timeval tv_end) 
{
    return (f32)(((tv_end.tv_sec * 1000000) + tv_end.tv_usec) -
            ((tv_start.tv_sec * 1000000) + tv_start.tv_usec)) / (1000.0f*1000.0f);
}


void msleep(u32 milliseconds)  
{
  int ms_remaining = (milliseconds) % 1000;
  long usec = ms_remaining * 1000;
  struct timespec ts_sleep;

  ts_sleep.tv_sec = (milliseconds) / 1000;
  ts_sleep.tv_nsec = 1000*usec;
  nanosleep(&ts_sleep, NULL);
}

global_var snd_pcm_t* _pcm;

internal void
alsa_fill_sound_buffer(UntitledSoundBuffer* sound_output, int samples_to_write) {

   int result;
   while((result = snd_pcm_writei(_pcm, sound_output->sample_out, samples_to_write)) != samples_to_write) {
      if(result == -EPIPE) {
         snd_pcm_prepare(_pcm);
      } else {
        //
        //
      }
   }  
}

internal void
alsa_init(int samples_per_second, int samples_per_write) {
   snd_pcm_hw_params_t* _pcm_hw_params;

   if(snd_pcm_open(&_pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) {
      //TODO(LAG) Diagnostic
   }

   _pcm_hw_params = (snd_pcm_hw_params_t*)mmap(0,
                                               snd_pcm_hw_params_sizeof(),
                                               PROT_READ | PROT_WRITE,
                                               MAP_PRIVATE | MAP_ANONYMOUS,
                                               -1,
                                               0);
   snd_pcm_hw_params_any(_pcm, _pcm_hw_params);

   snd_pcm_hw_params_set_access(_pcm, _pcm_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
   snd_pcm_hw_params_set_format(_pcm, _pcm_hw_params, SND_PCM_FORMAT_S16_LE);
   snd_pcm_hw_params_set_channels(_pcm, _pcm_hw_params, 2);
   snd_pcm_hw_params_set_rate(_pcm, _pcm_hw_params, samples_per_second, 0);
   snd_pcm_hw_params_set_buffer_size(_pcm, _pcm_hw_params, samples_per_write);
   snd_pcm_hw_params_set_period_time(_pcm, _pcm_hw_params, 100000, 0);

   snd_pcm_hw_params(_pcm, _pcm_hw_params);
}

int 
main()
{
    Display *display = XOpenDisplay(0);
    if(!display) {
        //log
        return 1;
    }

    Window window;
    XEvent event; 
    int default_screen = DefaultScreen(display);
    int default_root = XDefaultRootWindow(display);
    XVisualInfo visual_info = {0};

    if(!XMatchVisualInfo(display, default_screen, XDefaultDepth(display, default_screen),
                         TrueColor, &visual_info))
    {
        //log
        XCloseDisplay(display);
        return 1;
    }

    XSetWindowAttributes window_attributes;
    window_attributes.bit_gravity = StaticGravity;
    window_attributes.win_gravity = StaticGravity;
    window_attributes.background_pixel = 0XFFFFFFFF;
    window_attributes.colormap = XCreateColormap(display, default_root, visual_info.visual, AllocNone);

    uint attributes_mask = CWBitGravity | CWBackPixel | CWColormap | CWEventMask;
    window_attributes.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask;

    window = XCreateWindow(display, default_root, 0, 0, 
                            1280, 720, 0, 
                            visual_info.depth, InputOutput, visual_info.visual,
                            attributes_mask, &window_attributes);

    XStoreName(display, window, "Untitled");
    XMapWindow(display, window);
    XFlush(display);

    linux_set_size_hint(display, window, 1280, 720, 1280, 720);
    GC defaultGC = DefaultGC(display, default_screen);

    Atom WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if(!XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1))
    {
        printf("Couldn't register WM_DELETE_WINDOW property\n");
    }

    void *memory = mmap(0, 1280 * 720 * 4, 
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
    XImage *x_img = XCreateImage(display, visual_info.visual, visual_info.depth,
                                 ZPixmap, 0, (char*)memory, 1280, 720, 32, 0);

    UntitledMemory untitled_memory = {0};

    untitled_memory.permanent_storage_size = Megabytes(32);
    untitled_memory.transient_storage_size = Megabytes(128);

    usize total_memsize = untitled_memory.permanent_storage_size + untitled_memory.transient_storage_size;

    untitled_memory.permanent_storage = mmap(0, total_memsize, 
                                            PROT_READ | PROT_WRITE, 
                                            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    untitled_memory.transient_storage = 
        ((u8*)untitled_memory.permanent_storage + untitled_memory.permanent_storage_size);

    untitled_memory.is_init = false;

    Bool running = true;

    int refreshHz = 60;
    int game_updateHz = refreshHz;
    f32 target_seconds = 1.0f / (f32)game_updateHz;
    linux_load_gamecode();





    /*  ALSAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  */

    int samples_per_second = 48000;
    int samples_per_write = 48000 / 15;
    i16 *s_memory = (i16*)mmap(0, 48000 * 4,
                        PROT_WRITE | PROT_READ, 
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    alsa_init(samples_per_second, samples_per_write);


    UntitledInput input[2] = {0};
    UntitledInput *new_input = &input[1];
    UntitledInput *old_input = &input[0];

    

    struct timeval last_counter;
    gettimeofday(&last_counter, 0);

    

    while(running) {

        struct stat temp_file_stat;
        stat("/home/vito/git/untitled-unmastered/linux_bin/untitled.so", &temp_file_stat);

        if(temp_file_stat.st_mtim.tv_sec != file_stat.st_mtim.tv_sec) {
            if(global_gamecode_dll) {
                dlclose(global_gamecode_dll);
                global_gamecode_dll = 0;
                untitled_update_game = 0;
            }
            linux_load_gamecode();
        }

        while(XPending(display) > 0){
            XNextEvent(display, &event);
            switch (event.type) {
                case ClientMessage: 
                {
                    if((Atom)event.xclient.data.l[0] == WM_DELETE_WINDOW) {
                        running = false;
                    }
                } break;
            }
        }

        UntitledOffscreenBuffer screen_buffer = {0};
        screen_buffer.memory = memory;
        screen_buffer.width = 1280;
        screen_buffer.height = 720;


        snd_pcm_sframes_t avail, delay;
        snd_pcm_avail_delay(_pcm, &avail, &delay);
        int samples_to_write = avail;
        if(samples_to_write + delay > samples_per_write) {
            samples_to_write = samples_per_write - delay;
        }

        UntitledSoundBuffer sbf = {0};
        sbf.sample_out = s_memory;
        sbf.samples_per_second = samples_per_second;
        sbf.sample_count = samples_to_write;

        if(global_gamecode_dll) {
            untitled_update_game(&untitled_memory, &screen_buffer, &sbf, new_input);
        } else {
            linux_load_gamecode();
        }

        if(samples_to_write > 0) {
            alsa_fill_sound_buffer(&sbf, samples_to_write);
        }
        struct timeval work_counter = get_timeval();

        f32 work_seconds_elapsed = get_seconds_elapsed(last_counter, work_counter);
        f32 seconds_elapsed_for_frame = work_seconds_elapsed;

        while(seconds_elapsed_for_frame < target_seconds) {
            //Note(LAG) Due to granularity, it cannot hit the right amount of sleep time so we make it sleep for a little less time than what it should and the loop handle the rest
            u32 sleep_ms = (i32)(1000.0f * (target_seconds - seconds_elapsed_for_frame));
            if(sleep_ms > 0) {
                msleep(sleep_ms);
            }

            seconds_elapsed_for_frame = get_seconds_elapsed(last_counter, get_timeval());
     
        }

        struct timeval end_counter = get_timeval();

        //f32 ms_per_frame = 1000.0f * get_seconds_elapsed(last_counter, end_counter);
            //f32 fps = (1000.0f) / ms_per_frame;


        //printf("%.2f\n", fps);

        XPutImage(display, window, defaultGC, x_img, 0, 0, 0, 0,
                  screen_buffer.width, screen_buffer.height);
        XFlush(display);

        last_counter = end_counter;

        UntitledInput *tmp_input = old_input;
        old_input = new_input;
        new_input = tmp_input;
    }

    return 0;
}

/*
 * 
 *  static const char *
    get_audio_device(int channels)
    {
        const char *device;

        device = SDL_getenv("AUDIODEV"); 
        if (device == NULL) {
            switch (channels) {
                case 6:
                    device = "plug:surround51";
                    break;
                case 4:
                    device = "plug:surround40";
                    break;
                default:
                    device = "default";
                    break;
            }
        }
        return device;
    }

 *
 *
 *
 * */
