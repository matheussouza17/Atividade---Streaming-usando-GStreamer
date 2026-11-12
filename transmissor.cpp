//Grupo 7
//Nomes: Leonardo Campos, Matheus Henrique, Wisley Victor, Lucas Rodrigues

// TRANSMISSOR
// fonte -> audioconvert -> audioresample -> opusenc -> rtpopuspay -> [netsim] -> udpsink
// Uso: ./transmissor [host] [porta] [teste|mic] [perda %] [jitter ms]

#include <gst/gst.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static gboolean ao_mensagem(GstBus *, GstMessage *msg, gpointer loop) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err;
        gst_message_parse_error(msg, &err, nullptr);
        g_printerr("Erro em %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_error_free(err);
        g_main_loop_quit((GMainLoop *)loop);
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        g_main_loop_quit((GMainLoop *)loop);
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int porta        = argc > 2 ? atoi(argv[2]) : 5000;
    bool mic         = argc > 3 && strcmp(argv[3], "mic") == 0;
    double perda     = argc > 4 ? atof(argv[4]) : 0;   // simulacao de rede
    int jitter       = argc > 5 ? atoi(argv[5]) : 0;
    bool simular     = perda > 0 || jitter > 0;

    // Elementos
    GstElement *pipeline = gst_pipeline_new("transmissor");
    GstElement *fonte  = gst_element_factory_make(mic ? "autoaudiosrc" : "audiotestsrc", "fonte");
    GstElement *conv   = gst_element_factory_make("audioconvert", "conv");
    GstElement *resamp = gst_element_factory_make("audioresample", "resamp");
    GstElement *enc    = gst_element_factory_make("opusenc", "enc");
    GstElement *pay    = gst_element_factory_make("rtpopuspay", "pay");
    GstElement *sim    = simular ? gst_element_factory_make("netsim", "sim") : nullptr;
    GstElement *sink   = gst_element_factory_make("udpsink", "sink");

    if (!pipeline || !fonte || !conv || !resamp || !enc || !pay || !sink || (simular && !sim)) {
        g_printerr("Elemento nao encontrado. Verifique os plugins com gst-inspect-1.0\n");
        return 1;
    }

    // Configuracao
    if (!mic) g_object_set(fonte, "is-live", TRUE, "freq", 440.0, nullptr);  // tom de 440 Hz
    g_object_set(enc, "bitrate", 64000, nullptr);                            // 20 ms por pacote
    g_object_set(pay, "pt", 96, nullptr);                                    // payload dinamico
    g_object_set(sink, "host", host, "port", porta, nullptr);
    if (sim)  // netsim: descarta e atrasa pacotes para simular uma rede ruim
        g_object_set(sim, "drop-probability", (gfloat)(perda / 100.0),
                     "delay-probability", jitter > 0 ? 1.0f : 0.0f,
                     "min-delay", 0, "max-delay", jitter, nullptr);

    // Montagem
    gst_bin_add_many(GST_BIN(pipeline), fonte, conv, resamp, enc, pay, sink, nullptr);
    gboolean ok;
    if (sim) {
        gst_bin_add(GST_BIN(pipeline), sim);
        ok = gst_element_link_many(fonte, conv, resamp, enc, pay, sim, sink, nullptr);
    } else {
        ok = gst_element_link_many(fonte, conv, resamp, enc, pay, sink, nullptr);
    }
    if (!ok) {
        g_printerr("Falha ao ligar os elementos\n");
        return 1;
    }

    // Execucao
    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, ao_mensagem, loop);
    gst_object_unref(bus);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Nao foi possivel iniciar o pipeline\n");
        return 1;
    }
    printf("Transmitindo %s para %s:%d (Opus/RTP/UDP) | perda simulada %.1f%% | jitter 0-%d ms\n",
           mic ? "microfone" : "tom de 440 Hz", host, porta, perda, jitter);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    return 0;
}
