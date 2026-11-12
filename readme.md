# Streaming de Áudio com GStreamer (RTP/UDP)

Grupo 7 — Leonardo Campos, Matheus Henrique, Wisley Victor, Lucas Rodrigues

## Dependências (Arch Linux)

```bash
sudo pacman -S --needed base-devel pkgconf gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugin-pipewire
```

## Compilação

```bash
g++ transmissor.cpp -o transmissor $(pkg-config --cflags --libs gstreamer-1.0)
g++ receptor.cpp    -o receptor    $(pkg-config --cflags --libs gstreamer-1.0)
```

## Execução

Use dois terminais. Inicie o **receptor primeiro**.

```bash
# Terminal 1
./receptor [porta] [latencia_ms] [nula]

# Terminal 2
./transmissor [host] [porta] [teste|mic] [perda_%] [jitter_ms]
```

| Parâmetro | Padrão | Descrição |
|---|---|---|
| `host` | 127.0.0.1 | IP do receptor |
| `porta` | 5000 | Porta UDP |
| `teste` / `mic` | teste | Tom de 440 Hz ou microfone |
| `perda_%` | 0 | Perda simulada (netsim) |
| `jitter_ms` | 0 | Atraso aleatório máximo (netsim) |
| `latencia_ms` | 200 | Latência do rtpjitterbuffer |
| `nula` | — | Receptor sem som (fakesink) |

## Exemplos

```bash
# Básico (mesmo PC)
./receptor 5000 200
./transmissor 127.0.0.1 5000

# Microfone
./transmissor 127.0.0.1 5000 mic

# Rede ruim simulada: 5% de perda + até 100 ms de jitter
./transmissor 127.0.0.1 5000 teste 5 100
```

## Experimentos de latência

Com o transmissor em `teste 5 100`, troque a latência do receptor:

```bash
./receptor 5000 20
./receptor 5000 200
./receptor 5000 1000
```
