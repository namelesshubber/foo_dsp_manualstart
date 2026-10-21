// foo_dsp_manualstart (v0.4)
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
// Additionally, the FIRST audio format change (sample rate or channel count) seen
// since foobar2000 was launched also receives the silence, once. This covers
// output devices/modes that need to be re-initialised the first time the format
// changes (for example switching from a DSD-converted stream to plain PCM).
//
// Technique: the silence is prepended to the first chunk with
// audio_chunk::insert_silence_fromstart(), the same public SDK call used by
// other silence-inserting DSPs such as Affix silence.
//
// The pending manual-start silence stays "armed" until a chunk actually consumes
// it, so it is not lost if the first file played bypasses the DSP chain.

#include <foobar2000/SDK/foobar2000.h>
#include <atomic>

DECLARE_COMPONENT_VERSION(
    "Manual start silence",
    "0.4",
    "Inserts initial silence only when playback is started manually.");

VALIDATE_COMPONENT_FILENAME("foo_dsp_manualstart.dll");

// Length of the initial silence in milliseconds.
static constexpr t_uint64 kSilenceMs = 1000;

// Diagnostic logging to the foobar2000 console (View > Console).
// Set to false to disable.
static constexpr bool kLog = true;

// GUID of this DSP. Keep it unchanged between versions so that existing DSP
// chains continue to reference the component.
static const GUID guid_this_dsp =
    { 0xB3A1F2C4, 0x6D58, 0x4E1A, { 0x9C, 0x07, 0x2F, 0x4A, 0x8D, 0x1E, 0x5B, 0x93 } };

// true = the next playback start counts as "manual".
// Initially true, which covers the very first playback after launching foobar2000.
static std::atomic<bool> g_next_start_is_manual{ true };

// true = the next chunk that goes through the DSP gets the initial silence.
// Cleared ONLY when the silence has actually been inserted.
static std::atomic<bool> g_arm_silence{ false };

// true = the next audio format change is the first one since launch and gets
// the silence. Cleared when that first format change happens.
static std::atomic<bool> g_first_format_change_pending{ true };

// Last audio format seen by the DSP.
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
        // A non-manual start does NOT disarm a silence that is still pending.
        if (kLog) {
            FB2K_console_formatter() << "foo_dsp_manualstart: playback starting, manual="
                << (manual ? "yes" : "no") << ", silence armed="
                << (g_arm_silence.load() ? "yes" : "no");
        }
    }

    void on_playback_stop(play_control::t_stop_reason reason) override {
        // Only a user-initiated Stop makes the next start manual again.
        if (reason == play_control::stop_reason_user) {
            g_next_start_is_manual = true;
        }
        if (kLog) {
            FB2K_console_formatter() << "foo_dsp_manualstart: playback stopped, reason="
                << (int)reason;
        }
    }

    void on_playback_new_track(metadb_handle_ptr track) override {
        if (kLog && track.is_valid()) {
            FB2K_console_formatter() << "foo_dsp_manualstart: new track: " << track->get_path();
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

        const unsigned last_srate = g_last_srate.exchange(srate);
        const unsigned last_nch = g_last_nch.exchange(nch);
        const bool format_changed =
            (last_srate != 0) && (srate != last_srate || nch != last_nch);

        if (kLog && (srate != last_srate || nch != last_nch)) {
            FB2K_console_formatter() << "foo_dsp_manualstart: DSP is receiving audio at "
                << (t_uint64)srate << " Hz, " << (t_uint64)nch << " channels";
        }

        // Silence is inserted at most once per chunk, for either reason.
        const bool manual_start = g_arm_silence.exchange(false);
        const bool first_format_change =
            format_changed && g_first_format_change_pending.exchange(false);

        if (manual_start || first_format_change) {
            const t_size frames = (t_size)((t_uint64)srate * kSilenceMs / 1000);
            if (frames > 0) {
                chunk->insert_silence_fromstart(frames);
            }
            if (kLog) {
                FB2K_console_formatter() << "foo_dsp_manualstart: silence inserted ("
                    << (t_uint64)frames << " frames at " << (t_uint64)srate << " Hz), reason="
                    << (manual_start ? "manual start" : "first format change");
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
