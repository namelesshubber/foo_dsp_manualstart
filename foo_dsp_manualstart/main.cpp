// foo_dsp_manualstart (v0.3)
//
// A foobar2000 2.x DSP that inserts a short silence (1000 ms by default) at the
// start of the first audio chunk, but ONLY when playback is started manually.
// It is meant for DACs/receivers that need a moment to wake up and would
// otherwise cut off the beginning of the first track.
//
// "Manual" means: the first playback after foobar2000 is launched, or the first
// playback after the user pressed Stop. Gapless/sequential track changes and
// Next / double-click while playing are passed through untouched.
//
// Technique: the silence is prepended to the first chunk with
// audio_chunk::insert_silence_fromstart().
//
// The pending silence stays "armed" until a chunk actually consumes it, so it is
// not lost when the first file played bypasses the DSP chain (e.g. native DSD)
// and a PCM file is started afterwards.
// Must be placed in the DSP chain after "Pregap" (Pregap DSP).

#include <foobar2000/SDK/foobar2000.h>
#include <atomic>

DECLARE_COMPONENT_VERSION(
    "Manual start silence",
    "0.3",
    "Inserts 1000 ms of initial silence on manual start and format changes.");

VALIDATE_COMPONENT_FILENAME("foo_dsp_manualstart.dll");

static constexpr t_uint64 kSilenceMs = 1000;
static constexpr bool kLog = true;

static const GUID guid_this_dsp =
    { 0xB3A1F2C4, 0x6D58, 0x4E1A, { 0x9C, 0x07, 0x2F, 0x4A, 0x8D, 0x1E, 0x5B, 0x93 } };

static std::atomic<bool> g_next_start_is_manual{ true };
static std::atomic<bool> g_arm_silence{ true };

// Ultimo formato audio visto dal DSP
static std::atomic<unsigned> g_last_srate{ 0 };
static std::atomic<unsigned> g_last_nch{ 0 };

class manualstart_play_callback : public play_callback_static {
public:
    unsigned get_flags() override {
        return flag_on_playback_starting | flag_on_playback_stop | flag_on_playback_new_track;
    }

    void on_playback_starting(play_control::t_track_command, bool) override {
        const bool manual = g_next_start_is_manual.exchange(false);
        if (manual) {
            g_arm_silence = true;
        }
        if (kLog) {
            FB2K_console_formatter() << "foo_dsp_manualstart: playback starting, manual="
                << (manual ? "yes" : "no") << ", silence armed="
                << (g_arm_silence.load() ? "yes" : "no");
        }
    }

    void on_playback_stop(play_control::t_stop_reason reason) override {
        if (reason == play_control::stop_reason_user) {
            g_next_start_is_manual = true;
            g_arm_silence = true;
        }
        if (kLog) {
            FB2K_console_formatter() << "foo_dsp_manualstart: playback stopped, reason="
                << (int)reason;
        }
    }

    void on_playback_new_track(metadb_handle_ptr track) override {
        g_arm_silence = true;
        
        if (kLog && track.is_valid()) {
            FB2K_console_formatter() << "foo_dsp_manualstart: new track (silence re-armed): " << track->get_path();
        }
    }

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
        const unsigned srate = chunk->get_srate();
        const unsigned nch = chunk->get_channel_count();

        unsigned last_sr = g_last_srate.load();
        unsigned last_ch = g_last_nch.load();

        if (last_sr != 0 && (last_sr != srate || last_ch != nch)) {
            g_arm_silence = true;
            if (kLog) {
                FB2K_console_formatter() << "foo_dsp_manualstart: Format change detected (" 
                    << last_sr << "Hz -> " << srate << "Hz). Silence re-armed.";
            }
        }

        g_last_srate = srate;
        g_last_nch = nch;

        if (g_arm_silence.exchange(false)) {
            const t_size frames = (t_size)((t_uint64)srate * kSilenceMs / 1000);
            if (frames > 0) {
                chunk->insert_silence_fromstart(frames);
            }
            if (kLog) {
                FB2K_console_formatter() << "foo_dsp_manualstart: silence inserted ("
                    << (t_uint64)frames << " frames at " << (t_uint64)srate << " Hz)";
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
