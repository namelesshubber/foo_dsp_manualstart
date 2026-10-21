// foo_dsp_manualstart  (v0.3)
// DSP per foobar2000 2.x: inserisce 1000 ms di silenzio all'inizio del primo
// blocco audio SOLO quando la riproduzione parte "manualmente". Non dipende
// dal componente Affix silence.
//
// Manuale = primo avvio dopo l'apertura di foobar2000 oppure dopo uno Stop
// dell'utente. Sequenziale / Next / doppio clic in riproduzione = bypass.
//
// v0.3:
//  - il silenzio "armato" resta in attesa finche' un chunk lo consuma: non
//    viene piu' annullato da un avvio successivo non manuale (caso: DSD come
//    primo file, poi PCM avviato durante la riproduzione).
//  - log diagnostico nella console di foobar2000 (kLog).

#include <foobar2000/SDK/foobar2000.h>
#include <atomic>

DECLARE_COMPONENT_VERSION(
    "Manual start silence",
    "0.3",
    "Inserisce silenzio iniziale solo all'avvio manuale della riproduzione.");

VALIDATE_COMPONENT_FILENAME("foo_dsp_manualstart.dll");

// Durata del silenzio iniziale (come il preset Affix silence: pre = 1000 ms)
static constexpr t_uint64 kSilenceMs = 1000;

// Log diagnostico nella console (View > Console). Metti false per disattivarlo.
static constexpr bool kLog = true;

// Stesso GUID delle versioni precedenti: la voce nella catena DSP resta valida.
static const GUID guid_this_dsp =
    { 0xB3A1F2C4, 0x6D58, 0x4E1A, { 0x9C, 0x07, 0x2F, 0x4A, 0x8D, 0x1E, 0x5B, 0x93 } };

// true = la prossima partenza di riproduzione conta come "manuale".
// Inizialmente true: copre il cold start (primo play dopo l'avvio di foobar2000).
static std::atomic<bool> g_next_start_is_manual{ true };

// true = al prossimo chunk audio che passa dal DSP va inserito il silenzio.
// Viene azzerato SOLO quando il silenzio e' stato inserito.
static std::atomic<bool> g_arm_silence{ false };

// Ultimo formato visto dal DSP (solo per il log).
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
        // Un avvio non manuale NON disarma un silenzio ancora in attesa.
        if (kLog) {
            FB2K_console_formatter() << "foo_dsp_manualstart: playback starting, manual="
                << (manual ? "si" : "no") << ", silenzio armato="
                << (g_arm_silence.load() ? "si" : "no");
        }
    }

    void on_playback_stop(play_control::t_stop_reason reason) override {
        // Come lo script js_panel: solo lo Stop dell'utente torna in modalita' manuale.
        if (reason == play_control::stop_reason_user) {
            g_next_start_is_manual = true;
        }
        if (kLog) {
            FB2K_console_formatter() << "foo_dsp_manualstart: playback stop, motivo="
                << (int)reason;
        }
    }

    void on_playback_new_track(metadb_handle_ptr track) override {
        if (kLog && track.is_valid()) {
            FB2K_console_formatter() << "foo_dsp_manualstart: nuova traccia: " << track->get_path();
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

    static void g_get_name(pfc::string_base& out) { out = "Silenzio avvio manuale"; }

    bool on_chunk(audio_chunk* chunk, abort_callback&) override {
        const unsigned srate = chunk->get_srate();
        const unsigned nch = chunk->get_channel_count();

        if (kLog && (srate != g_last_srate.load() || nch != g_last_nch.load())) {
            g_last_srate = srate;
            g_last_nch = nch;
            FB2K_console_formatter() << "foo_dsp_manualstart: il DSP riceve audio a "
                << (t_uint64)srate << " Hz, " << (t_uint64)nch << " canali";
        }

        // Solo il primo chunk dopo un avvio manuale riceve il silenzio.
        if (g_arm_silence.exchange(false)) {
            const t_size frames = (t_size)((t_uint64)srate * kSilenceMs / 1000);
            if (frames > 0) {
                chunk->insert_silence_fromstart(frames);
            }
            if (kLog) {
                FB2K_console_formatter() << "foo_dsp_manualstart: silenzio inserito ("
                    << (t_uint64)frames << " frame a " << (t_uint64)srate << " Hz)";
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
