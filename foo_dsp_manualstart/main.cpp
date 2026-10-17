// foo_dsp_manualstart (v0.2)
// DSP for foobar2000 2.x: inserts 1000 ms of silence at the beginning of
// the first audio chunk ONLY when playback is started "manually."
//
// The initial silence is achieved using
// audio_chunk::insert_silence_fromstart() on the first chunk.
//
// "Manual" means the first start after opening foobar2000 or after the user
// presses Stop. Sequential playback / Next / double-clicking while already
// playing = bypass.
// Must be placed in the DSP chain before "Pregap" (Pregap DSP).

#include <foobar2000/SDK/foobar2000.h>
#include <atomic>

DECLARE_COMPONENT_VERSION(
    "Manual start silence",
    "0.2",
    "Inserts an initial silence only when playback is started manually.");

VALIDATE_COMPONENT_FILENAME("foo_dsp_manualstart.dll");

// Duration of the initial silence
static constexpr t_uint64 kSilenceMs = 1000;

// GUID of this DSP
static const GUID guid_this_dsp =
    { 0xB3A1F2C4, 0x6D58, 0x4E1A, { 0x9C, 0x07, 0x2F, 0x4A, 0x8D, 0x1E, 0x5B, 0x93 } };

// true = the next playback start counts as "manual".
// Initially true: covers the cold start (first play after launching foobar2000).
static std::atomic<bool> g_next_start_is_manual{ true };

// true = initial silence should be inserted before the next audio chunk.
static std::atomic<bool> g_arm_silence{ false };

class manualstart_play_callback : public play_callback_static {
public:
    unsigned get_flags() override {
        return flag_on_playback_starting | flag_on_playback_stop;
    }

    void on_playback_starting(play_control::t_track_command, bool) override {
        // If the start is manual, we enable the silence; otherwise, we disable it.
        g_arm_silence = g_next_start_is_manual.exchange(false);
    }

    void on_playback_stop(play_control::t_stop_reason reason) override {
        // Only a user-initiated stop returns the system to manual mode.
        if (reason == play_control::stop_reason_user) {
            g_next_start_is_manual = true;
        }
    }

    void on_playback_new_track(metadb_handle_ptr) override {}
    void on_playback_seek(double) override {}
    void on_playback_pause(bool) override {}
    void on_playback_edited(metadb_handle_ptr) override {}
    void on_playback_dynamic_info(const file_info&) override {}
    void on_playback_dynamic_info_track(const file_info&) override {}
    void on_playback_time(double) override {}
    void on_volume_change(float) override {}
};

static play_callback_static_factory_t<manualstart_play_callback> g_play_callback_factory;

class dsp_manualstart : public dsp_impl_base {
public:
    static GUID g_get_guid() { return guid_this_dsp; }

    static void g_get_name(pfc::string_base& out) { out = "Manual start silence"; }

    bool on_chunk(audio_chunk* chunk, abort_callback&) override {
        // Only the first chunk following a manual start receives silence.
        if (g_arm_silence.exchange(false)) {
            const t_size frames = (t_size)((t_uint64)chunk->get_srate() * kSilenceMs / 1000);
            if (frames > 0) {
                chunk->insert_silence_fromstart(frames);
            }
        }
        return true;
    }

    void on_endofplayback(abort_callback&) override {}
    void on_endoftrack(abort_callback&) override {}
    void flush() override {}
    double get_latency() override { return 0.0; }
    bool need_track_change_mark() override { return false; }
};

static dsp_factory_nopreset_t<dsp_manualstart> g_dsp_factory;
