#ifndef PSRC_ENGINE_AUDIO_H
#define PSRC_ENGINE_AUDIO_H

/*  TODO:
        - nuke due to complexity
        - make playSound take a uint64_t time offset
        - make stb_vorbis optional
        - make the 3D effect use filters (https://www.youtube.com/watch?v=9lywYAGSgdc)
        - make the 3D effect use inverse square for distance to volume
        - make audio emitters take a size for the 3D effect
        - later, do a ray cast and make cubes multiply volume by 0.0 to 1.0 when passed through
        - reject audio if the volume is less than 1/32768
        - make antialiasing use 4 steps instead of 32
        - keep speed mul of 32
        - give speed mul to 2D sounds
        - make audio offsets wrap to prevent overflow
        - make the server-side store audio play times and transfer the offset during state sync
        - use a priority system (LOWEST, LOW, NORMAL, HIGH, HIGHEST)
        - make the following functions:
            - new3DAudioEmitter
            - edit3DAudioEmitter
            - pause3DAudioEmitter
            - stop3DAudioEmitter
            - delete3DAudioEmitter
            - play3DSound
            - new2DAudioEmitter
            - edit2DAudioEmitter
            - pause2DAudioEmitter
            - stop2DAudioEmitter
            - delete2DAudioEmitter
            - play2DSound
            - setMusic
            - setMusicStyle
*/

#include "../attribs.h"
#include "../resource.h"

#if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
    #include <SDL.h>
#elif defined(PSRC_USESDL1)
    #include <SDL/SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

#include <stdint.h>
#include <stdbool.h>

struct audiostate {
    struct {
        float pos[3];
        float rot[3];
        float rotradx, rotrady, rotradz;
        float sinx, cosx;
        float siny, cosy;
        float sinz, cosz;
    } cam;
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    volatile bool valid;
    bool usecallback;
    #ifndef PSRC_USESDL1
    SDL_AudioDeviceID output;
    #endif
    int freq;
    int channels;
    struct rcopt_sound soundrcopt;
};

extern struct audiostate audiostate;

enum audioopt {
    AUDIOOPT_END,
    AUDIOOPT_FREQ,      // int
    AUDIOOPT_3DVOICES,  // int
    AUDIOOPT_2DVOICES   // int
};

#define AUDIOPRIO_DEFAULT (-128)
#define AUDIOPRIO_MIN (-127)
#define AUDIOPRIO_NORMAL (0)
#define AUDIOPRIO_MAX (127)

#define AUDIOFXMASK_VOL_L (1 << 0)
#define AUDIOFXMASK_VOL_R (1 << 1)
#define AUDIOFXMASK_VOL (AUDIOFXMASK_VOL_L | AUDIOFXMASK_VOL_R)
#define AUDIOFXMASK_SPEED (1 << 2)
#define AUDIOFXMASK_LPFILT (1 << 3)
#define AUDIOFXMASK_HPFILT (1 << 4)
#define AUDIOFXMASK_ALL (-1U)
struct audiofx {
    uint8_t vol[2];
    float speed; 
    float lpfilt;
    float hpfilt;
};

#define AUDIO3DFXMASK_POS (1 << 0)
#define AUDIO3DFXMASK_SIZE (1 << 1)
#define AUDIO3DFXMASK_VOLDAMP (1 << 2)
#define AUDIO3DFXMASK_FREQDAMP (1 << 3)
#define AUDIO3DFXMASK_NODOPPLER (1 << 4)
#define AUDIO3DFXMASK_RELPOS (1 << 5)
#define AUDIO3DFXMASK_RELROT (1 << 6)
#define AUDIO3DFXMASK_ALL (-1U)
struct audio3dfx {
    float pos[3];
    float size[3];
    float voldamp;
    float freqdamp;
    uint8_t nodoppler : 1;
    uint8_t relpos : 1;
    uint8_t relrot : 1;
};

#define AUDIOENVMASK_LPFILT (1 << 0)
#define AUDIOENVMASK_HPFILT (1 << 1)
#define AUDIOENVMASK_REVERB_DELAY (1 << 2)
#define AUDIOENVMASK_REVERB_FEEDBACK (1 << 3)
#define AUDIOENVMASK_REVERB_MIX (1 << 4)
#define AUDIOENVMASK_REVERB_LPFILT (1 << 5)
#define AUDIOENVMASK_REVERB_HPFILT (1 << 6)
#define AUDIOENVMASK_REVERB (AUDIOENVMASK_REVERB_DELAY | AUDIOENVMASK_REVERB_FEEDBACK | AUDIOENVMASK_REVERB_MIX | \
                            AUDIOENVMASK_REVERB_LPFILT | AUDIOENVMASK_REVERB_HPFILT)
#define AUDIOENVMASK_ALL (-1U)
struct audioenv {
    float lpfilt;
    float hpfilt;
    struct {
        float delay;
        float feedback;
        float mix;
        float lpfilt;
        float hpfilt;
    } reverb;
};

#define SOUNDFLAG_LOOP (1 << 0)
#define SOUNDFLAG_WRAP (1 << 1)

bool initAudio(void);
bool startAudio(void);
void updateAudioConfig(enum audioopt, ...);
void updateAudio(float framemult);
void stopAudio(void);
bool restartAudio(void);
void quitAudio(bool quick);

int new3DAudioEmitter(int8_t prio, unsigned max, unsigned fxmask, const struct audiofx*, unsigned fx3dmask, const struct audio3dfx*);
void edit3DAudioEmitter(int, unsigned fxmask, const struct audiofx*, unsigned fx3dmask, const struct audio3dfx*);
void pause3DAudioEmitter(int, bool);
void stop3DAudioEmitter(int);
void delete3DAudioEmitter(int);
bool play3DSound(int e, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx*);

int new2DAudioEmitter(int8_t prio, unsigned max, unsigned fxmask, const struct audiofx*);
void edit2DAudioEmitter(int, unsigned fxmask, const struct audiofx*);
void pause2DAudioEmitter(int, bool);
void stop2DAudioEmitter(int);
void delete2DAudioEmitter(int);
bool play2DSound(int e, struct rc_sound* rc, int8_t prio, uint8_t flags, unsigned fxmask, const struct audiofx*);

void setAudioEnv(unsigned mask, struct audioenv*);

void setMusic(struct rc_sound* rc); // TODO: make rc_music once PTM is added
void setMusicStyle(const char*);

#endif
