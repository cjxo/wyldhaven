#if defined(OS_WINDOWS)
# include "./impl/winmm_audio.c"
// #if defined(OS_LINUX)
#endif

fun u8
audio_channel_count(void) {
    return g_audio_device.channels;
}

fun u64
audio_sample_rate_hz(void) {
    return g_audio_device.samples_per_second_hz;
}

fun u32
audio_bytes_per_channel(void) {
    return g_audio_device.bytes_per_channel;
}

fun void
audio_attatch_audio_provide_callback(AudioProvideFunc audio_provide, void *user_data) {
    g_audio_device.audio_provide_callback = audio_provide;
    g_audio_device.user_data = user_data;
}

fun void
snd_generate_square_wave(Memory_Arena *arena, Sound *sound, u64 duration_secs, s16 frequency_hz, s16 amplitude) {
    sound->size_in_bytes = duration_secs * audio_sample_rate_hz() * audio_channel_count() * audio_bytes_per_channel();
    sound->data = (s16 *)arena_push_array(arena, u8, sound->size_in_bytes);
    u64 sample_count = duration_secs * audio_sample_rate_hz();
    
    u32 square_wave_period = (u32)audio_sample_rate_hz() / frequency_hz;
    
    s16 *sample_out = (s16 *)sound->data;
    for (u32 sample_index = 0; sample_index < sample_count; ++sample_index) {
        s16 sample_to_write = ((sample_index / (square_wave_period / 2)) % 2) ? -amplitude : amplitude;
        *sample_out++ = sample_to_write;
        *sample_out++ = sample_to_write;
    }
}

fun void
snd_generate_sine_wave(Memory_Arena *arena, Sound *sound, u64 duration_secs, f32 frequency_hz, s16 amplitude) {
    sound->size_in_bytes = duration_secs * audio_sample_rate_hz() * audio_channel_count() * audio_bytes_per_channel();
    sound->data = (s16 *)arena_push_array(arena, u8, sound->size_in_bytes);
    u64 sample_count = duration_secs * audio_sample_rate_hz();
    
    //f64 t_increments = two_pi_f32 / (((f64)audio_device->samples_per_second_hz / frequency_hz));
    //f64 t_increments = 1.0 / (f64)audio_device->samples_per_second_hz;
    //f64 factored = t_increments * frequency_hz * two_pi_f64;
    f64 factored = (frequency_hz * two_pi_f64) / (f64)audio_sample_rate_hz();
    //f64 t_sine = 0.0;
    
    s16 *sample_out = (s16 *)sound->data;
    for (u64 sample_index = 0; sample_index < sample_count; ++sample_index) {
        //s16 sample = (s16)(sin(t_sine) * (u64)amplitude);
        s16 sample = (s16)(sin(factored * (f64)sample_index) * (u64)amplitude);
        *sample_out++ = sample;
        *sample_out++ = sample;
        //t_sine += t_increments;
    }
    
    assert_true(sample_out == (s16 *)((u8 *)sound->data + sound->size_in_bytes));
}
