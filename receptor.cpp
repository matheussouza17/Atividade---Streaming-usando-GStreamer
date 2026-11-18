// Grupo 7
// Nomes: Leonardo Campos, Matheus Henrique, Wisley Victor, Lucas Rodrigues
// RECEPTOR RTP/UDP (Opus)
// Uso: ./build/receptor <porta> [latencia_ms]

#include <gst/gst.h>
#include <cstdio>
#include <cstdlib>

static GstElement *jb = nullptr;

static GstPadProbeReturn ao_evento(GstPad *, GstPadProbeInfo *info, gpointer)
{
    const GstStructure *st = gst_event_get_structure(GST_PAD_PROBE_INFO_EVENT(info));
    if (st && gst_structure_has_name(st, "GstRTPPacketLost"))
    {
        guint seq = 0;
        gst_structure_get_uint(st, "seqnum", &seq);
        printf("[PERDA] pacote %u\n", seq);
    }
    return GST_PAD_PROBE_OK;
}

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
    else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
             gst_structure_has_name(gst_message_get_structure(msg), "drop-msg"))
    {
        guint seq = 0;
        gst_structure_get_uint(gst_message_get_structure(msg), "seqnum", &seq);
        printf("[DESCARTE] pacote %u chegou tarde\n", seq);
    }
    return TRUE;
}

static gboolean placar(gpointer)
{
    GstStructure *st = nullptr;
    guint64 recebidos = 0, perdidos = 0, atrasados = 0;
    g_object_get(jb, "stats", &st, nullptr);
    if (st)
    {
        gst_structure_get(st,
                          "num-pushed", G_TYPE_UINT64, &recebidos,
                          "num-lost", G_TYPE_UINT64, &perdidos,
                          "num-late", G_TYPE_UINT64, &atrasados,
                          nullptr);
        gst_structure_free(st);
        printf("recebidos=%llu perdidos=%llu atrasados=%llu\n",
               (unsigned long long)recebidos,
               (unsigned long long)perdidos,
               (unsigned long long)atrasados);
    }
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc < 2)
    {
        g_printerr("Uso: %s <porta> [latencia_ms]\n", argv[0]);
        return 1;
    }

    int porta = atoi(argv[1]);
    int latencia = argc > 2 ? atoi(argv[2]) : 200;

    GstElement *pipeline = gst_pipeline_new("receptor");
    GstElement *src = gst_element_factory_make("udpsrc", "src");
    jb = gst_element_factory_make("rtpjitterbuffer", "jb");
    GstElement *depay = gst_element_factory_make("rtpopusdepay", "depay");
    GstElement *dec = gst_element_factory_make("opusdec", "dec");
    GstElement *conv = gst_element_factory_make("audioconvert", "conv");
    GstElement *res = gst_element_factory_make("audioresample", "res");
    GstElement *sink = gst_element_factory_make("autoaudiosink", "sink");

    if (!pipeline || !src || !jb || !depay || !dec || !conv || !res || !sink)
    {
        g_printerr("Elemento nao encontrado. Verifique os plugins com gst-inspect-1.0\n");
        return 1;
    }

    GstCaps *caps = gst_caps_from_string(
        "application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=96");
    g_object_set(src, "address", "0.0.0.0", "port", porta, "caps", caps, nullptr);
    gst_caps_unref(caps);

    g_object_set(jb,
                 "latency", latencia,
                 "do-lost", TRUE,
                 "post-drop-messages", TRUE,
                 nullptr);
    g_object_set(dec, "plc", TRUE, nullptr);

    gst_bin_add_many(GST_BIN(pipeline), src, jb, depay, dec, conv, res, sink, nullptr);
    if (!gst_element_link_many(src, jb, depay, dec, conv, res, sink, nullptr))
    {
        g_printerr("Falha ao ligar os elementos do receptor\n");
        gst_object_unref(pipeline);
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    GstPad *pad = gst_element_get_static_pad(jb, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, ao_evento, nullptr, nullptr);
    gst_object_unref(pad);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, ao_mensagem, loop);
    gst_object_unref(bus);

    g_timeout_add_seconds(1, placar, nullptr);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Nao foi possivel iniciar o receptor\n");
        gst_object_unref(pipeline);
        g_main_loop_unref(loop);
        return 1;
    }

    printf("Escutando na porta UDP %d | latencia %d ms\n", porta, latencia);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    return 0;
}