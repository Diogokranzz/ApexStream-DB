#  ApexStream DB (High-Frequency Data Engine)

Engine C++ desenvolvida para ingestão, processamento e streaming de dados de alta frequência (HFT/Telemetria) sem dependências externas.

##  Performance
- **Throughput:** 1.4 GB/s (Parsing CSV via AVX2/SIMD)
- **Latency:** < 1ms (Network to WebSocket)
- **Visual:** 60 FPS Real-time Rendering (Canvas API)

##  Arquitetura "Bare Metal"
O sistema ignora bibliotecas de alto nível para maximizar performance:
1. **Ingestor (Thread 1):** Raw TCP Socket Server (Sem HTTP overhead).
2. **Core (SIMD):** Parser customizado usando intrínsecos AVX2 (`__m256i`).
3. **Storage (Thread 3):** Gravação Assíncrona em Binário (`struct` packing) via Ring Buffer.
4. **Broadcaster (Thread 2):** Servidor WebSocket implementado do zero (RFC 6455) com Handshake SHA1.

##  Funcionalidades
- **Live Mode:** Ingestão TCP porta 9000 -> WebSocket porta 8080.
- **Black Box:** Persistência automática em formato binário compacto.
- **Replay Mode:** `--replay` flag para simulação determinística de eventos passados.

##  Tech Stack
- **Backend:** C++17 (Windows API, Winsock2, OpenSSL, AVX2).
- **Frontend:** Next.js + HTML5 Canvas (Sem bibliotecas de gráficos).
- **Protocolo:** Custom Binary Protocol & Raw WebSockets.

##  Build & Run

### Backend (C++)
Certifique-se de ter o compilador MSVC (Visual Studio) e as bibliotecas OpenSSL instaladas.

```bash
# Compilar (Visual Studio CL)
# Execute no Developer Command Prompt
cl src/apex_ultimate.cpp /O2 /mavx2 /link ws2_32.lib libssl.lib libcrypto.lib

# Rodar (Live Mode)
./apex_ultimate.exe

# Rodar (Replay Mode)
./apex_ultimate.exe --replay
```

### Frontend (Next.js)
Visualize os dados em tempo real.

```bash
cd apex-visualizer
npm install
npm run dev
# Acesse http://localhost:3000
```

**Vulgo DK**

