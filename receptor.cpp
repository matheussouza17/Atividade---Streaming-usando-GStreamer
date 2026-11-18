

#include <gst/gst.h>
#include <cstdio>
#include <cstdlib>

static GstElement *jb;  // jitter buffer (sala de espera)

// FISCAL 1: o jitter buffer desistiu de esperar um pacote -> PERDA
static GstPadProbeReturn ao_evento(GstPad *, GstPadProbeInfo *info, gpointer) {
    const GstStructure *st = gst_event_get_structure(GST_PAD_PROBE_INFO_EVENT(info));
    if (st && gst_structure_has_name(st, "GstRTPPacketLost")) {
        guint seq;
        gst_structure_get_uint(st, "seqnum", &seq);
        printf("[PERDA] pacote %u\n", seq);
    }
    return GST_PAD_PROBE_OK;
}

// FISCAL 2: recados do bus -> DESCARTE (chegou tarde) ou ERRO
static gboolean ao_mensagem(GstBus *, GstMessage *msg, gpointer loop) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        g_printerr("Erro no pipeline\n");
        g_main_loop_quit((GMainLoop *)loop);
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
               gst_structure_has_name(gst_message_get_structure(msg), "drop-msg")) {
        guint seq;
        gst_structure_get_uint(gst_message_get_structure(msg), "seqnum", &seq);
        printf("[DESCARTE] pacote %u chegou tarde\n", seq);
    }
    return TRUE;
}

// FISCAL 3: placar a cada 1 segundo
static gboolean placar(gpointer) {
    GstStructure *st;
    guint64 recebidos, perdidos, atrasados;
    g_object_get(jb, "stats", &st, nullptr);
    gst_structure_get(st, "num-pushed", G_TYPE_UINT64, &recebidos,
                          "num-lost",   G_TYPE_UINT64, &perdidos,
                          "num-late",   G_TYPE_UINT64, &atrasados, nullptr);
    gst_structure_free(st);
    printf("recebidos=%llu perdidos=%llu atrasados=%llu\n",
           (unsigned long long)recebidos, (unsigned long long)perdidos,
           (unsigned long long)atrasados);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    int porta    = argc > 1 ? atoi(argv[1]) : 5000;
    int latencia = argc > 2 ? atoi(argv[2]) : 200;

    // 1. CRIAR as maquinas da linha
    GstElement *pipeline = gst_pipeline_new("receptor");
    GstElement *src   = gst_element_factory_make("udpsrc", nullptr);          // caixa de correio
    jb                = gst_element_factory_make("rtpjitterbuffer", nullptr); // sala de espera
    GstElement *depay = gst_element_factory_make("rtpopusdepay", nullptr);    // tira etiqueta
    GstElement *dec   = gst_element_factory_make("opusdec", nullptr);         // abre o vacuo
    GstElement *conv  = gst_element_factory_make("audioconvert", nullptr);    // adaptador
    GstElement *res   = gst_element_factory_make("audioresample", nullptr);   // ajusta taxa
    GstElement *sink  = gst_element_factory_make("autoaudiosink", nullptr);   // alto-falante

    // 2. CONFIGURAR
    GstCaps *caps = gst_caps_from_string(   // bilhete: "isto e Opus via RTP"
        "application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=96");
    g_object_set(src, "port", porta, "caps", caps, nullptr);
    gst_caps_unref(caps);
    g_object_set(jb, "latency", latencia,           // tempo de espera (ms)
                     "do-lost", TRUE,               // liga aviso de PERDA
                     "post-drop-messages", TRUE,    // liga aviso de DESCARTE
                     nullptr);
    g_object_set(dec, "plc", TRUE, nullptr);        // improvisa o que faltar

    // 3. LIGAR na esteira
    gst_bin_add_many(GST_BIN(pipeline), src, jb, depay, dec, conv, res, sink, nullptr);
    gst_element_link_many(src, jb, depay, dec, conv, res, sink, nullptr);

    // 4. PLUGAR os fiscais
    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    GstPad *pad = gst_element_get_static_pad(jb, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, ao_evento, nullptr, nullptr);
    gst_object_unref(pad);
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, ao_mensagem, loop);
    gst_object_unref(bus);
    g_timeout_add_seconds(1, placar, nullptr);

    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    printf("Recebendo na porta %d | latencia %d ms\n", porta, latencia);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}