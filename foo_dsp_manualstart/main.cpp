// foo_dsp_manualstart
// DSP per foobar2000 2.x: ospita un'istanza REALE di "Affix silence"
// (preset 1000 ms / 0 ms / vuoto) e la attiva solo per la prima traccia
// avviata "manualmente", con la stessa logica dello script js_panel:
//   - manuale = primo avvio dopo l'apertura di foobar2000 oppure dopo uno
//     Stop dell'utente (stop_reason_user)
//   - sequenziale / Next / doppio clic durante la riproduzione = bypass
//
// Va messo nella catena DSP DOPO "Pregap DSP".

#include <foobar2000/SDK/foobar2000.h>
#include <atomic>

DECLARE_COMPONENT_VERSION(
    "Manual start silence host",
    "0.1",
    "Ospita Affix silence solo all'avvio manuale della riproduzione.");

VALIDATE_COMPONENT_FILENAME("foo_dsp_manualstart.dll");

// ---------------------------------------------------------------------------
// Dati del preset di Affix silence, ricavati dal tuo preset Manual esportato
// ---------------------------------------------------------------------------

// GUID: 750FBC09-5337-42D5-9A0F-9E76611D7EC2
static const GUID guid_affix_silence =
    { 0x750FBC09, 0x5337, 0x42D5, { 0x9A, 0x0F, 0x9E, 0x76, 0x61, 0x1D, 0x7E, 0xC2 } };

// base64 "BgAAAAAAAADoAwAAAA==" decodificato (13 byte, contiene 1000 = 0x3E8)
static const uint8_t affix_preset_data[] =
    { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00, 0x00 };

// GUID di questo DSP
static const GUID guid_this_dsp =
    { 0xB3A1F2C4, 0x6D58, 0x4E1A, { 0x9C, 0x07, 0x2F, 0x4A, 0x8D, 0x1E, 0x5B, 0x93 } };

// ---------------------------------------------------------------------------
// Stato condiviso tra callback di riproduzione (main thread) e DSP (thread
// di decodifica)
// ---------------------------------------------------------------------------

// true = la prossima partenza di riproduzione conta come "manuale".
// Inizialmente true: copre il cold start (primo play dopo l'avvio di foobar2000).
static std::atomic<bool> g_next_start_is_manual{ true };

// true = il DSP, al prossimo blocco audio, deve creare un'istanza fresca di
// Affix silence.
static std::atomic<bool> g_arm_silence{ false };

// ---------------------------------------------------------------------------
// Callback di riproduzione
// ---------------------------------------------------------------------------

class manualstart_play_callback : public play_callback_static {
public:
    unsigned get_flags() override {
        return flag_on_playback_starting | flag_on_playback_stop;
    }

    void on_playback_starting(play_control::t_track_command, bool) override {
        // Consumiamo il flag: se la partenza e' manuale, armiamo il DSP.
        if (g_next_start_is_manual.exchange(false)) {
            g_arm_silence = true;
        } else {
            g_arm_silence = false;
        }
    }

    void on_playback_stop(play_control::t_stop_reason reason) override {
        // Come lo script: solo lo Stop dell'utente riporta in modalita' manuale.
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
// Il DSP
// ---------------------------------------------------------------------------

class dsp_manualstart : public dsp {
public:
    dsp_manualstart() {}

    static GUID g_get_guid() { return guid_this_dsp; }

    static void g_get_name(pfc::string_base& out) {
        out = "Silenzio avvio manuale (host Affix silence)";
    }

    void run(dsp_chunk_list* list, const metadb_handle_ptr& cur_file,
             int flags, abort_callback& abort) override {

        // Nuova armatura: crea un'istanza fresca di Affix silence.
        if (g_arm_silence.exchange(false)) {
            create_inner();
        }

        if (m_inner.is_valid()) {
            m_inner->run(list, cur_file, flags, abort);

            // Fine traccia o flush: l'effetto vale solo per la prima traccia.
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
        // Vogliamo ricevere END_OF_TRACK per sapere quando rilasciare l'istanza.
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
