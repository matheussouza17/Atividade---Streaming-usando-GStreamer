// Grupo 7
// Nomes: Leonardo Campos, Matheus Henrique, Wisley Victor, Lucas Rodrigues
// Uso: ./build/transmissor <ip_destino> <porta> [teste|mic] [rtp|vlc]

#include <gst/gst.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static gboolean ao_mensagem(GstBus *, GstMessage *msg, gpointer loop)
{
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
    {
        GError *err = nullptr;
        gst_message_parse_error(msg, &err, nullptr);
        g_printerr("Erro em %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_error_free(err);
        g_main_loop_quit((GMainLoop *)loop);
    }
    else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS)
    {
        g_main_loop_quit((GMainLoop *)loop);
    }
    return TRUE;
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc < 3)
    {
        g_printerr("Uso: %s <ip_destino> <porta> [teste|mic] [rtp|vlc]\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int porta = atoi(argv[2]);
    bool mic = argc > 3 && strcmp(argv[3], "mic") == 0;
    bool modo_vlc = argc > 4 && strcmp(argv[4], "vlc") == 0;

    GstElement *pipeline = gst_pipeline_new("transmissor");
    GstElement *fonte = gst_element_factory_make(mic ? "autoaudiosrc" : "audiotestsrc", "fonte");
    GstElement *conv = gst_element_factory_make("audioconvert", "conv");
    GstElement *resamp = gst_element_factory_make("audioresample", "resamp");
    GstElement *sink = gst_element_factory_make("udpsink", "sink");

    if (!pipeline || !fonte || !conv || !resamp || !sink)
    {
        g_printerr("Erro ao criar elementos base.\n");
        return 1;
    }

    if (!mic)
        g_object_set(fonte, "is-live", TRUE, "freq", 440.0, nullptr);
    g_object_set(sink, "host", host, "port", porta, nullptr);

    gboolean ok = FALSE;

    if (modo_vlc)
    {
        // MPEG-TS sobre UDP: VLC abre direto com udp://@:5000
        GstElement *enc = gst_element_factory_make("lamemp3enc", "enc");
        GstElement *parse = gst_element_factory_make("mpegaudioparse", "parse");
        GstElement *mux = gst_element_factory_make("mpegtsmux", "mux");

        if (!enc || !parse || !mux)
        {
            g_printerr("Plugin ausente para modo VLC (verifique lamemp3enc, mpegaudioparse, mpegtsmux).\n");
            gst_object_unref(pipeline);
            return 1;
        }

        gst_bin_add_many(GST_BIN(pipeline), fonte, conv, resamp, enc, parse, mux, sink, nullptr);
        ok = gst_element_link_many(fonte, conv, resamp, enc, parse, mux, sink, nullptr);
    }
    else
    {
        // RTP/UDP com Opus: para o seu receptor C++ (Parte 2)
        GstElement *enc = gst_element_factory_make("opusenc", "enc");
        GstElement *pay = gst_element_factory_make("rtpopuspay", "pay");

        if (!enc || !pay)
        {
            g_printerr("Plugin ausente para modo RTP (verifique opusenc, rtpopuspay).\n");
            gst_object_unref(pipeline);
            return 1;
        }

        g_object_set(enc, "bitrate", 64000, nullptr);
        g_object_set(pay, "pt", 96, nullptr);

        gst_bin_add_many(GST_BIN(pipeline), fonte, conv, resamp, enc, pay, sink, nullptr);
        ok = gst_element_link_many(fonte, conv, resamp, enc, pay, sink, nullptr);
    }

    if (!ok)
    {
        g_printerr("Falha ao ligar os elementos do transmissor.\n");
        gst_object_unref(pipeline);
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, ao_mensagem, loop);
    gst_object_unref(bus);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Nao foi possivel iniciar o transmissor.\n");
        gst_object_unref(pipeline);
        g_main_loop_unref(loop);
        return 1;
    }

    printf("Transmitindo %s para %s:%d (%s)\n",
           mic ? "microfone" : "tom de 440 Hz",
           host, porta,
           modo_vlc ? "MP3/MPEG-TS/UDP -> VLC" : "Opus/RTP/UDP -> Receptor C++");
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    return 0;
}