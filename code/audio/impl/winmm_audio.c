#include <mmeapi.h>

typedef struct {
    u64 samples_per_second_hz;
    u8 channels;
    u32 bytes_per_channel; //sizeof(s16)
    
    OS_Handle fill_thread;
    OS_Handle semaphore_swap_buffer;
    
    void *user_data;
    AudioProvideFunc *audio_provide_callback;
    
    b32 has_initialized;
    
    // NOTE(christian): platform specific stuff
    u32 buffer_index;
    HWAVEOUT output_device_handle;
    u64 size_in_bytes_per_buffer;
    WAVEHDR wave_hdrs[2];
    u8 *wave_hdr_buffer;
} Audio_Device;

glb Audio_Device g_audio_device;

fun void __stdcall
winmm_wave_out_proc(HWAVEOUT device_handle, UINT message, DWORD_PTR instance,
                    DWORD_PTR param_0, DWORD_PTR param_1) {
    unused(device_handle);
    unused(param_0);
    unused(param_1);
    unused(instance);
    switch (message) {
        case WOM_CLOSE: {
        } break;
        
        case WOM_DONE: {
            OutputDebugStringA("Please give me more audio\n");
            
            // the device is done with block. Now, Request for new block
            os_semaphore_release(g_audio_device.semaphore_swap_buffer, 1, null);
        } break;
        
        case WOM_OPEN: {
            OutputDebugString("Successfully created audio device.\n");
        } break;
    }
}

fun void
audio_swap_buffers(Audio_Device *device) {
    u32 current_index = device->buffer_index;
    WAVEHDR *hdr = device->wave_hdrs + current_index;
    
    u8 *buffer_ptr = device->wave_hdr_buffer + device->size_in_bytes_per_buffer * current_index;
    waveOutUnprepareHeader(device->output_device_handle, hdr, sizeof(WAVEHDR));
    hdr->lpData = (void *)buffer_ptr;
    hdr->dwBufferLength = (u32)device->size_in_bytes_per_buffer;
    
    if (device->audio_provide_callback) {
        device->audio_provide_callback(hdr->dwBufferLength, buffer_ptr, device->user_data);
    } else {
        clear_memory(hdr->lpData, hdr->dwBufferLength);
    }
    
    hdr->dwFlags = 0;
    waveOutPrepareHeader(device->output_device_handle, hdr, sizeof(WAVEHDR));
    waveOutWrite(device->output_device_handle, hdr, sizeof(WAVEHDR));
    
    device->buffer_index = (device->buffer_index + 1) % array_count(device->wave_hdrs);
}

fun s32
audio_play(void *param) {
    unused(param);
    while (true) {
        audio_swap_buffers(&g_audio_device);
        os_wait_for_object(g_audio_device.semaphore_swap_buffer, OSWait_Infinite);
    }
    
    return 0;
}

fun void
audio_init(u64 samples_per_sec) {
    if (g_audio_device.has_initialized) {
        return;
    }
    
    WAVEFORMATEX wave_format = {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = 2,
        .nSamplesPerSec = (u32)samples_per_sec,
    };
    
    wave_format.wBitsPerSample = sizeof(s16) * 8;
    wave_format.nBlockAlign = (wave_format.wBitsPerSample / 8) * wave_format.nChannels;
    wave_format.nAvgBytesPerSec = wave_format.nBlockAlign * wave_format.nSamplesPerSec;
    wave_format.cbSize = 0;
    
    MMRESULT wave_return_result = waveOutOpen(&(g_audio_device.output_device_handle), WAVE_MAPPER, &wave_format,
                                              (DWORD_PTR)&winmm_wave_out_proc, null,
                                              CALLBACK_FUNCTION);
    
    switch (wave_return_result) {
        case MMSYSERR_NOERROR: {
        } break;
    }
    
    g_audio_device.samples_per_second_hz = samples_per_sec;
    g_audio_device.channels = 2;
    g_audio_device.bytes_per_channel = sizeof(s16);
    
    g_audio_device.size_in_bytes_per_buffer = 4096 * wave_format.nBlockAlign;
    u64 hdr_buffer_size = 0;
    for (u32 hdr_index = 0; hdr_index < array_count(g_audio_device.wave_hdrs); ++hdr_index) {
        WAVEHDR *hdr = g_audio_device.wave_hdrs + hdr_index;
        hdr->lpData = null;
        hdr->dwBufferLength = (u32)g_audio_device.size_in_bytes_per_buffer;
        hdr->dwBytesRecorded = 0;
        hdr->dwUser = null;
        hdr->dwFlags = 0;
        hdr->dwLoops = 0;
        hdr->lpNext = null;
        hdr->reserved = null;
        
        hdr_buffer_size += g_audio_device.size_in_bytes_per_buffer;
    }
    
    assert_true(hdr_buffer_size > 0);
    g_audio_device.wave_hdr_buffer = HeapAlloc(GetProcessHeap(),
                                               HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY,
                                               hdr_buffer_size);
    
    g_audio_device.fill_thread = os_thread_create(&audio_play, true, null);
    assert_false(os_match_handle(g_audio_device.fill_thread, os_bad_handle()));
    
    g_audio_device.semaphore_swap_buffer = os_semaphore_create(1, array_count(g_audio_device.wave_hdrs), null);
    assert_false(os_match_handle(g_audio_device.semaphore_swap_buffer, os_bad_handle()));
    
    g_audio_device.has_initialized = true;
}