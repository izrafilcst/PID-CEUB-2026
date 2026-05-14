#pragma once
#include <cstdint>
#include <cstdio>

/// Entrada de telemetria coletada a cada ciclo do loop PID.
struct LogEntry {
    uint32_t ts;        ///< timestamp em millis()
    float    position;  ///< posição calculada do array de sensores (-1.0 .. +1.0 normalizado)
    float    error;     ///< erro atual (setpoint - position)
    float    termP;     ///< contribuição proporcional
    float    termI;     ///< contribuição integral
    float    termD;     ///< contribuição derivativa
    float    correction; ///< saída total do PID
    int      vLeft;     ///< velocidade PWM motor esquerdo (0-255)
    int      vRight;    ///< velocidade PWM motor direito (0-255)
    uint32_t dtUs;      ///< duração do ciclo em microssegundos
};

/**
 * @brief Logger de telemetria com buffer circular lock-free (1 produtor / 1 consumidor).
 *
 * - Core 0 chama push() — jamais bloqueia.
 * - Core 1 chama drain() — drena para callback BLE.
 * - Coleta ativa somente com #define DEBUG_LOG e setEnabled(true).
 */
class Logger {
public:
    static constexpr int BUFFER_SIZE = 50;

    Logger();

    // --- API do Core 0 (produtor) ---

    /// Insere entrada no buffer circular. Descarta silenciosamente se cheio.
    void push(const LogEntry& entry);

    // --- API do Core 1 (consumidor) ---

    /**
     * @brief Drena todas as entradas pendentes chamando formatFn com a linha CSV.
     * @param formatFn  Callback que recebe a string CSV terminada em '\n'
     *                  (ex: BLETuner::notifyStatus). Não chamado se nullptr.
     */
    void drain(void (*formatFn)(const char*));

    // --- Utilitários ---

    bool isEmpty() const;
    int  count()   const;

    /**
     * @brief Serializa uma entrada no formato CSV:
     *        "ts,pos,err,p,i,d,corr,vl,vr,dt\n"
     */
    static void entryToCsv(const LogEntry& e, char* buf, int bufSize);

    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled()  const       { return _enabled; }

private:
    LogEntry     _buffer[BUFFER_SIZE];
    volatile int  _head;    ///< próximo índice de escrita (Core 0)
    volatile int  _tail;    ///< próximo índice de leitura (Core 1)
    volatile bool _enabled;
};
