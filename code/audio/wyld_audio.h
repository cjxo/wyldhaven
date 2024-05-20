/* date = May 15th 2024 9:58 pm */

#ifndef WYLD_AUDIO_H
#define WYLD_AUDIO_H

typedef struct {
    s16 *data;
    u64 size_in_bytes;
    u64 sound_index_in_bytes;
} Sound;

fun void snd_generate_square_wave(Memory_Arena *arena, Sound *sound, u64 duration_secs, s16 frequency_hz, s16 amplitude);
fun void snd_generate_sine_wave(Memory_Arena *arena, Sound *sound, u64 duration_secs, f32 frequency_hz, s16 amplitude);

typedef void AudioProvideFunc(u32 buffer_sz_bytes, u8 *buffer_ptr, void *user_data);
fun void audio_init(u64 samples_per_sec);
fun void audio_attatch_audio_provide_callback(AudioProvideFunc audio_provide, void *user_data);
fun u8 audio_channel_count(void);
fun u64 audio_sample_rate_hz(void);
fun u32 audio_bytes_per_channel(void);

#endif //WYLD_AUDIO_H
