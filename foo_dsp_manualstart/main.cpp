// foo_dsp_manualstart
// DSP for foobar2000 2.x: hosts an ACTUAL instance of "Affix silence"
// (preset 1000 ms / 0 ms / empty) and activates it only for the first track
// started "manually":
//   - manual = first start after opening foobar2000 or after a
//     user-initiated stop (stop_reason_user)
//   - sequential / Next / double-click during playback = bypass
//
// Must be placed in the DSP chain before "Pregap" (Pregap DSP).

#include <foobar2000/SDK/foobar2000.h>
#include <atomic>

DECLARE_COMPONENT_VERSION(
    "Manual start Affix silence host",
    "0.1",
    "Hosts Affix silence only when playback is started manually.");

VALIDATE_COMPONENT_FILENAME("foo_dsp_manualstart.dll");

// ---------------------------------------------------------------------------
// Affix silence preset data
// ---------------------------------------------------------------------------

// GUID: 750FBC09-5337-42D5-9A0F-9E76611D7EC2
static const GUID guid_affix_silence =
    { 0x750FBC09, 0x5337, 0x42D5, { 0x9A, 0x0F, 0x9E, 0x76, 0x61, 0x1D, 0x7E, 0xC2 } };

// decoded base64 "BgAAAAAAAADoAwAAAA==" (13 bytes, contains 1000 = 0x3E8)
static const uint8_t affix_preset_data[] =
    { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00, 0x00 };

// GUID of this DSP
static const GUID guid_this_dsp =
    { 0xB3A1F2C4, 0x6D58, 0x4E1A, { 0x9C, 0x07, 0x2F, 0x4A, 0x8D, 0x1E, 0x5B, 0x93 } };

// ---------------------------------------------------------------------------
// State shared between playback callbacks (main thread) and DSP (decoding
// thread)
// ---------------------------------------------------------------------------

// true = the next playback start counts as "manual".
// Initially true: covers the cold start (first play after launching foobar2000).
static std::atomic<bool> g_next_start_is_manual{ true };

// true = the DSP must create a fresh instance of Affix silence
// at the next audio block.
static std::atomic<bool> g_arm_silence{ false };

// ---------------------------------------------------------------------------
// Playback callback
// ---------------------------------------------------------------------------

class manualstart_play_callback : public play_callback_static {
public:
    unsigned get_flags() override {
        return flag_on_playback_starting | flag_on_playback_stop;
    }

    void on_playback_starting(play_control::t_track_command, bool) override {
        // Consume the flag: if the start is manual, let's arm the DSP.
        if (g_next_start_is_manual.exchange(false)) {
            g_arm_silence = true;
        } else {
            g_arm_silence = false;
        }
    }

    void on_playback_stop(play_control::t_stop_reason reason) override {
        // Only a user-initiated stop returns to manual mode.
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

// ---------------------------------------------------------------------------
// The DSP
// ---------------------------------------------------------------------------

class dsp_manualstart : public dsp {
public:
    dsp_manualstart() {}

    static GUID g_get_guid() { return guid_this_dsp; }

    static void g_get_name(pfc::string_base& out) {
        out = "Host Affix silence";
    }

    // In this SDK, dsp::run has 3 arguments (no abort_callback).
    void run(dsp_chunk_list* list, const metadb_handle_ptr& cur_file,
             int flags) override {

        // New arming: creates a fresh instance of Affix silence.
        if (g_arm_silence.exchange(false)) {
            create_inner();
        }

        if (m_inner.is_valid()) {
            m_inner->run(list, cur_file, flags);

            // End of track or flush: the effect applies only to the first track.
            if (flags & (dsp::END_OF_TRACK | dsp::FLUSH)) {
                m_inner.release();
            }
        }
    }

    void flush() override {
        if (m_inner.is_valid()) {
            m_inner->flush();
            m_inner.release();
        }
    }

    double get_latency() override {
        return m_inner.is_valid() ? m_inner->get_latency() : 0.0;
    }

    bool need_track_change_mark() override {
        // We want to receive END_OF_TRACK so we know when to release the instance.
        return true;
    }

private:
    void create_inner() {
        m_inner.release();

        dsp_preset_impl preset;
        preset.set_owner(guid_affix_silence);
        preset.set_data(affix_preset_data, sizeof(affix_preset_data));

        service_ptr_t<dsp> inst;
        if (dsp_entry::g_instantiate(inst, preset)) {
            m_inner = inst;
        } else {
            FB2K_console_formatter()
                << "foo_dsp_manualstart: Affix silence non trovato o preset rifiutato, bypass.";
        }
    }

    service_ptr_t<dsp> m_inner;
};

static dsp_factory_nopreset_t<dsp_manualstart> g_dsp_factory;
