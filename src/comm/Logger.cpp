#include "comm/Logger.h"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Notas de thread-safety (1 produtor / 1 consumidor):
//
//  _head  — escrito exclusivamente pelo Core 0 (push).
//  _tail  — escrito exclusivamente pelo Core 1 (drain).
//  Ambos são lidos pelo outro core apenas para verificar condição de
//  cheio/vazio. Com variáveis volatile e índices inteiros alinhados,
//  a leitura/escrita é atômica no Xtensa LX6 (ESP32), sendo suficiente
//  para o padrão Single-Producer / Single-Consumer sem mutex.
// ---------------------------------------------------------------------------

Logger::Logger() : _head(0), _tail(0), _enabled(false) {}

// --- Core 0 (produtor) -----------------------------------------------------

void Logger::push(const LogEntry& entry) {
#ifdef DEBUG_LOG
    if (!_enabled) return;

    int next = (_head + 1) % BUFFER_SIZE;
    if (next == _tail) {
        // Buffer cheio: descarta a nova entrada para não bloquear Core 0.
        return;
    }

    _buffer[_head] = entry;
    _head = next;     // publicação: tail vê a entrada somente após esta linha
#else
    (void)entry;
#endif
}

// --- Core 1 (consumidor) ---------------------------------------------------

void Logger::drain(void (*formatFn)(const char*)) {
#ifdef DEBUG_LOG
    if (!formatFn) return;

    char buf[128];
    // Captura _head uma única vez para evitar condição de corrida durante
    // o laço (tail nunca ultrapassa head, pois só Core 1 avança tail).
    int head = _head;
    while (_tail != head) {
        entryToCsv(_buffer[_tail], buf, sizeof(buf));
        formatFn(buf);
        _tail = (_tail + 1) % BUFFER_SIZE;
    }
#else
    (void)formatFn;
#endif
}

// --- Utilitários (ambos os cores, somente leitura do estado) ---------------

bool Logger::isEmpty() const {
    return _head == _tail;
}

int Logger::count() const {
    // Snapshot atômico dos índices para cálculo consistente.
    int h = _head;
    int t = _tail;
    if (h >= t) return h - t;
    return BUFFER_SIZE - t + h;
}

// static
void Logger::entryToCsv(const LogEntry& e, char* buf, int bufSize) {
    // Formato: "ts,pos,err,p,i,d,corr,vl,vr,dt\n"
    snprintf(buf, bufSize,
        "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%lu\n",
        static_cast<unsigned long>(e.ts),
        e.position,
        e.error,
        e.termP,
        e.termI,
        e.termD,
        e.correction,
        e.vLeft,
        e.vRight,
        static_cast<unsigned long>(e.dtUs)
    );
}
