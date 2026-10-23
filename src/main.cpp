// Click Assist - corrige clics equivocados para no morir.
//
// Idea: se guardan "muestras" (posicion + velocidad) del jugador en cada paso
// de fisica y se recuerda cuando fue la ultima pulsacion/soltada. Si el jugador
// va a morir dentro del rango configurado desde ese ultimo input, el mod:
//   1) cancela la muerte,
//   2) devuelve al jugador al estado justo antes del input,
//   3) invierte el input (si pulsaste, suelta; si soltaste, pulsa).
// Hay un limite de correcciones seguidas para no salvarte de errores que no
// fueron de click (picos, etc.).

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <deque>

using namespace geode::prelude;

namespace {

struct Sample {
    double time;
    CCPoint pos;
    double yVel;
};

struct InputEvent {
    double time = -1000.0;
    bool down = false;
};

struct AssistState {
    std::deque<Sample> samples[2]; // 0 = jugador 1, 1 = jugador 2
    InputEvent lastInput[2];
    bool injecting = false;
    double windowStart = -1000.0;
    int correctionsInWindow = 0;
    double lastCorrectionTime = -1000.0;
    bool flagged = false;
    bool origTestMode = false;

    void reset() {
        samples[0].clear();
        samples[1].clear();
        lastInput[0] = InputEvent{};
        lastInput[1] = InputEvent{};
        injecting = false;
        windowStart = -1000.0;
        correctionsInWindow = 0;
        lastCorrectionTime = -1000.0;
    }
};

AssistState g_state;

bool optEnabled() { return Mod::get()->getSettingValue<bool>("enabled"); }
bool optOnlyWave() { return Mod::get()->getSettingValue<bool>("only-wave"); }
bool optSafeMode() { return Mod::get()->getSettingValue<bool>("safe-mode"); }
double optRangeSec() {
    return static_cast<double>(Mod::get()->getSettingValue<int64_t>("range-ms")) / 1000.0;
}
int optMaxCorrections() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-corrections"));
}

} // namespace

// ---------------------------------------------------------------------------
// Registro de inputs y muestras de posicion
// ---------------------------------------------------------------------------
class $modify(CAGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1 && !g_state.injecting) {
            int idx = isPlayer1 ? 0 : 1;
            g_state.lastInput[idx].time = m_gameState.m_levelTime;
            g_state.lastInput[idx].down = down;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt) {
        auto pl = PlayLayer::get();
        if (optEnabled() && pl && static_cast<GJBaseGameLayer*>(pl) == this) {
            double now = m_gameState.m_levelTime;
            PlayerObject* players[2] = {m_player1, m_player2};
            for (int i = 0; i < 2; i++) {
                if (!players[i]) continue;
                auto& dq = g_state.samples[i];
                dq.push_back({now, players[i]->getPosition(), players[i]->m_yVelocity});
                // Guardamos un poco mas que el rango maximo (250 ms)
                while (!dq.empty() && now - dq.front().time > 0.6) dq.pop_front();
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }
};

// ---------------------------------------------------------------------------
// Correccion al morir
// ---------------------------------------------------------------------------
class $modify(CAPlayLayer, PlayLayer) {
    void resetLevel() {
        if (g_state.flagged) {
            m_isTestMode = g_state.origTestMode;
            g_state.flagged = false;
        }
        g_state.reset();
        PlayLayer::resetLevel();
    }

    bool tryCorrect(PlayerObject* player) {
        int idx = (player == m_player1) ? 0 : (player == m_player2 ? 1 : -1);
        if (idx < 0) return false;
        if (optOnlyWave() && !player->m_isDart) return false;

        double now = m_gameState.m_levelTime;

        // Varias colisiones en el mismo paso: ya corregido
        if (now == g_state.lastCorrectionTime) return true;

        InputEvent& in = g_state.lastInput[idx];
        if (now - in.time > optRangeSec()) return false;

        // Limite de correcciones por ventana de 0.5 s
        if (now - g_state.windowStart > 0.5) {
            g_state.windowStart = now;
            g_state.correctionsInWindow = 0;
        }
        if (g_state.correctionsInWindow >= optMaxCorrections()) return false;

        // Buscar la muestra justo antes del input
        auto& dq = g_state.samples[idx];
        int found = -1;
        for (int i = static_cast<int>(dq.size()) - 1; i >= 0; i--) {
            if (dq[i].time <= in.time) { found = i; break; }
        }
        if (found < 0) return false;
        Sample s = dq[found];

        // Rebobinar
        player->setPosition(s.pos);
        player->m_yVelocity = s.yVel;
        while (static_cast<int>(dq.size()) > found + 1) dq.pop_back();

        // Invertir input
        bool newDown = !in.down;
        g_state.injecting = true;
        this->handleButton(newDown, 1, idx == 0);
        g_state.injecting = false;

        in.time = now;
        in.down = newDown;
        g_state.correctionsInWindow++;
        g_state.lastCorrectionTime = now;

        if (optSafeMode() && !g_state.flagged) {
            g_state.flagged = true;
            g_state.origTestMode = m_isTestMode;
            m_isTestMode = true; // evita que se guarde progreso/completado
            Notification::create(
                "Click Assist: este intento no guardara progreso",
                NotificationIcon::Info, 2.f
            )->show();
        }
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (!optEnabled() || (object && object == m_anticheatSpike)) {
            return PlayLayer::destroyPlayer(player, object);
        }
        if (tryCorrect(player)) return;
        PlayLayer::destroyPlayer(player, object);
    }
};
