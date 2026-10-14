#pragma once
#include <cassert>
#include <climits>
#include <cstdint>

/** Min/max por canal — o que vai para a NVS (ConfigStore, chave "cal"). */
struct CalibrationData {
    int16_t min[8];
    int16_t max[8];
};

class Calibration {
public:
    static constexpr int MAX_SENSORS = 8;
    static constexpr float POSITION_SCALE = 1000.0f;
    static constexpr int NORM_MAX = 1000;
    static constexpr int MIN_VALID_RANGE = 200;  // contagens do MCP3008 (0–1023)

    explicit Calibration(int sensorCount);

    void update(const int* rawValues);
    void normalize(const int* rawValues, int* normalizedOut) const;
    float weightedPosition(const int* normalized) const;
    void reset();

    /**
     * true = linha branca sobre fundo preto. O QTR-8A dá menos tensão sobre o branco, então
     * a leitura normalizada é invertida para que "linha" continue sendo valor alto (F-03).
     */
    void setWhiteLine(bool whiteLine) { _whiteLine = whiteLine; }
    bool whiteLine() const { return _whiteLine; }

    /** True when every channel has max − min ≥ MIN_VALID_RANGE; lists failing channels. */
    bool isValid(uint8_t* badChannels = nullptr, int* badCount = nullptr) const;

    CalibrationData exportData() const;
    void importData(const CalibrationData& data);

    int count() const { return _count; }
    int getMin(int idx) const;
    int getMax(int idx) const;
    bool isLineLost() const { return _lineLost; }

private:
    Calibration(const Calibration&) = delete;
    Calibration& operator=(const Calibration&) = delete;
    Calibration(Calibration&&) = delete;
    Calibration& operator=(Calibration&&) = delete;

    int _range(int idx) const;

    int _count;
    int _min[MAX_SENSORS];
    int _max[MAX_SENSORS];
    bool _whiteLine = false;
    mutable float _lastPosition;
    mutable bool _lineLost;

    static constexpr int LINE_LOST_THRESHOLD = 200;
};
