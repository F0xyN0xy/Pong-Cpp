#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────────────────────
static const int BASE_W   = 1280;
static const int BASE_H   = 720;
static const int PADDLE_W = 14;
static const int PADDLE_H = 100;
static const int BALL_R   = 8;

static const float PADDLE_SPD = 480.0f;

static const float BALL_SPD_INIT = 360.0f;
static const float BALL_SPD_INC  = 18.0f;
static const float BALL_SPD_MAX  = 900.0f;
static const int   WIN_SCORE     = 7;

// ─────────────────────────────────────────────────────────────
//  ENUMS
// ─────────────────────────────────────────────────────────────
enum GameState {
    STATE_MAIN_MENU = 0,
    STATE_DIFFICULTY,
    STATE_MODE_SELECT,
    STATE_CONTROLS,
    STATE_SETTINGS,
    STATE_STATS,
    STATE_ACHIEVEMENTS,
    STATE_SKINS,
    STATE_COUNTDOWN,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_ROUND_OVER,
    STATE_GAME_OVER,
};

enum Difficulty { DIFF_EASY = 0, DIFF_MEDIUM, DIFF_HARD };
enum GameMode   { MODE_CLASSIC = 0, MODE_SPEED, MODE_ENDLESS, MODE_PRACTICE };
enum SkinId     { SKIN_WHITE = 0, SKIN_CYAN, SKIN_MAGENTA, SKIN_GOLD, SKIN_COUNT };

// ─────────────────────────────────────────────────────────────
//  COLOUR PALETTE
// ─────────────────────────────────────────────────────────────
static const Color COL_BG    = {  8,  8, 20, 255};
static const Color COL_LINE  = { 40, 40, 80, 220};
static const Color COL_TEXT  = {200,200,255, 255};
static const Color COL_DIM   = {100,100,160, 200};
static const Color COL_CYAN  = { 60,220,255, 255};
static const Color COL_MAG   = {255, 60,200, 255};
static const Color COL_GOLD  = {255,210,  0, 255};
static const Color COL_GREEN = { 60,255,130, 255};
static const Color COL_RED   = {255, 70, 70, 255};

static const Color SKIN_COLORS[SKIN_COUNT] = {
    {255,255,255,255}, {60,220,255,255}, {255,60,200,255}, {255,210,0,255}
};
static const char* SKIN_NAMES[SKIN_COUNT] = {"Classic White","Neon Cyan","Neon Magenta","Gold"};

// ─────────────────────────────────────────────────────────────
//  PARTICLES
// ─────────────────────────────────────────────────────────────
struct Particle {
    Vector2 pos, vel;
    float   life, maxLife;
    float   size;
    Color   col;
    bool    active;
};

static const int MAX_PARTICLES = 512;
static Particle  gParticles[MAX_PARTICLES];

static void SpawnParticles(Vector2 pos, Color col, int count, float speed) {
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++) {
        if (!gParticles[i].active) {
            float angle = (float)GetRandomValue(0,360) * DEG2RAD;
            float spd   = (float)GetRandomValue(50, (int)(speed*100)) * 0.01f * speed;
            gParticles[i] = {
                pos,
                {cosf(angle)*spd, sinf(angle)*spd},
                0.6f, 0.6f,
                (float)GetRandomValue(2,5),
                col,
                true
            };
            spawned++;
        }
    }
}

static void UpdateParticles(float dt) {
    for (auto &p : gParticles) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0) { p.active = false; continue; }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.y += 120.0f * dt; // slight gravity
        p.size   = (p.life / p.maxLife) * 5.0f;
    }
}

static void DrawParticles() {
    for (auto &p : gParticles) {
        if (!p.active) continue;
        float alpha = p.life / p.maxLife;
        Color c = p.col;
        c.a = (unsigned char)(alpha * 255);
        DrawCircleV(p.pos, p.size, c);
    }
}

// ─────────────────────────────────────────────────────────────
//  BALL TRAIL
// ─────────────────────────────────────────────────────────────
static const int TRAIL_LEN = 18;
struct TrailPoint { Vector2 pos; float alpha; };
static TrailPoint gTrail[TRAIL_LEN];
static int        gTrailHead = 0;

static void PushTrail(Vector2 pos) {
    gTrail[gTrailHead] = {pos, 1.0f};
    gTrailHead = (gTrailHead + 1) % TRAIL_LEN;
}

static void DrawTrail(Color ballCol) {
    for (int i = 0; i < TRAIL_LEN; i++) {
        int idx = (gTrailHead - 1 - i + TRAIL_LEN*2) % TRAIL_LEN;
        float a = 1.0f - (float)i / TRAIL_LEN;
        Color c = ballCol;
        c.a = (unsigned char)(a * a * 160);
        float r = BALL_R * (1.0f - (float)i / TRAIL_LEN * 0.7f);
        DrawCircleV(gTrail[idx].pos, r, c);
    }
}

// ─────────────────────────────────────────────────────────────
//  SCREEN SHAKE
// ─────────────────────────────────────────────────────────────
static float gShakeTimer = 0;
static float gShakeMag   = 0;

static void TriggerShake(float mag, float dur) {
    if (mag > gShakeMag) { gShakeMag = mag; gShakeTimer = dur; }
}

static Vector2 GetShakeOffset() {
    if (gShakeTimer <= 0) return {0,0};
    float s = gShakeMag * (gShakeTimer / 0.15f);
    return {(float)GetRandomValue(-1,1)*s, (float)GetRandomValue(-1,1)*s};
}

// ─────────────────────────────────────────────────────────────
//  SCORE POP-UP ANIMATIONS
// ─────────────────────────────────────────────────────────────
struct ScorePop {
    Vector2 pos;
    float   life, maxLife;
    std::string text;
    Color   col;
    bool    active;
};
static const int MAX_POPS = 8;
static ScorePop  gPops[MAX_POPS];

static void SpawnPop(Vector2 pos, const std::string &txt, Color col) {
    for (auto &p : gPops) {
        if (!p.active) {
            p = {pos, 1.2f, 1.2f, txt, col, true};
            return;
        }
    }
}

static void UpdatePops(float dt) {
    for (auto &p : gPops) {
        if (!p.active) continue;
        p.life -= dt;
        p.pos.y -= 40.0f * dt;
        if (p.life <= 0) p.active = false;
    }
}

static void DrawPops() {
    for (auto &p : gPops) {
        if (!p.active) continue;
        float t = p.life / p.maxLife;
        float scale = 1.0f + (1.0f - t) * 0.5f;
        int   fs  = (int)(36 * scale);
        Color c   = p.col;
        c.a = (unsigned char)(t * 255);
        int tw = MeasureText(p.text.c_str(), fs);
        DrawText(p.text.c_str(), (int)(p.pos.x - tw/2), (int)p.pos.y, fs, c);
    }
}

// ─────────────────────────────────────────────────────────────
//  ANIMATED BACKGROUND STARS
// ─────────────────────────────────────────────────────────────
struct Star { float x,y,z,pz; };
static const int STAR_COUNT = 200;
static Star      gStars[STAR_COUNT];

static void InitStars() {
    for (auto &s : gStars) {
        s.x  = (float)GetRandomValue(-BASE_W/2, BASE_W/2);
        s.y  = (float)GetRandomValue(-BASE_H/2, BASE_H/2);
        s.z  = (float)GetRandomValue(1, BASE_W);
        s.pz = s.z;
    }
}

static void UpdateStars(float dt, float speed = 200.0f) {
    for (auto &s : gStars) {
        s.pz = s.z;
        s.z -= speed * dt;
        if (s.z <= 0) {
            s.x  = (float)GetRandomValue(-BASE_W/2, BASE_W/2);
            s.y  = (float)GetRandomValue(-BASE_H/2, BASE_H/2);
            s.z  = BASE_W;
            s.pz = s.z;
        }
    }
}

static void DrawStars() {
    for (auto &s : gStars) {
        float sx = s.x / s.z  * BASE_W + BASE_W/2.0f;
        float sy = s.y / s.z  * BASE_H + BASE_H/2.0f;
        float px = s.x / s.pz * BASE_W + BASE_W/2.0f;
        float py = s.y / s.pz * BASE_H + BASE_H/2.0f;
        float br = (1.0f - s.z / BASE_W) * 3.0f;
        Color c  = {200,200,255, (unsigned char)((1.0f - s.z/BASE_W)*200)};
        DrawLineEx({px,py},{sx,sy},br,c);
    }
}

// ─────────────────────────────────────────────────────────────
//  SAVE DATA
// ─────────────────────────────────────────────────────────────
struct SaveData {
    int  highScores[4];   // one per game mode
    int  totalGames;
    int  totalRallies;
    int  longestRally;
    int  wins[2];         // wins for p1, p2/ai
    int  unlockedSkins;   // bitmask
    int  achievements;    // bitmask
    int  p1Skin, p2Skin;
    // settings
    float masterVol;
    float sfxVol;
    float musicVol;
    bool  fullscreen;
    bool  showFPS;
    bool  neonTheme;
};

static const char SAVE_PATH[] = "pong_save.dat";

static void DefaultSave(SaveData &s) {
    memset(&s, 0, sizeof(s));
    s.masterVol = 0.8f;
    s.sfxVol    = 1.0f;
    s.musicVol  = 0.5f;
    s.unlockedSkins = 1; // SKIN_WHITE unlocked by default
}

static void LoadSave(SaveData &s) {
    DefaultSave(s);
    std::ifstream f(SAVE_PATH, std::ios::binary);
    if (f) f.read(reinterpret_cast<char*>(&s), sizeof(s));
}

static void WriteSave(const SaveData &s) {
    std::ofstream f(SAVE_PATH, std::ios::binary);
    if (f) f.write(reinterpret_cast<const char*>(&s), sizeof(s));
}

// ─────────────────────────────────────────────────────────────
//  ACHIEVEMENTS
// ─────────────────────────────────────────────────────────────
enum Achievement {
    ACH_FIRST_WIN   = 1 << 0,
    ACH_RALLY_10    = 1 << 1,
    ACH_RALLY_25    = 1 << 2,
    ACH_PERFECT     = 1 << 3,  // win 7-0
    ACH_SPEED_DEMON = 1 << 4,  // ball > 700 speed
    ACH_ALL_SKINS   = 1 << 5,
    ACH_VETERAN     = 1 << 6,  // 50 games
};

static const char* ACH_NAMES[] = {
    "First Victory","Rally x10","Rally x25",
    "Perfect Game","Speed Demon","Collector","Veteran"
};
static const Achievement ACH_VALUES[] = {
    ACH_FIRST_WIN, ACH_RALLY_10, ACH_RALLY_25,
    ACH_PERFECT, ACH_SPEED_DEMON, ACH_ALL_SKINS, ACH_VETERAN
};

// ─────────────────────────────────────────────────────────────
//  GAME STRUCTS
// ─────────────────────────────────────────────────────────────
struct Paddle {
    float x, y, targetY;
    float dy;
    int   score;
    Color col;
    float w, h;
    // for smooth movement
    float visualY;
};

struct Ball {
    float x, y;
    float vx, vy;
    float speed;
    Color col;
};

// ─────────────────────────────────────────────────────────────
//  AUDIO (generated procedurally via wave synthesis)
// ─────────────────────────────────────────────────────────────
static Sound sSfxPaddle, sSfxWall, sSfxScore, sSfxMenu, sSfxWin;
static Music sBgMusic;
static bool  gMusicLoaded = false;

// Generate a simple tone as a Sound
static Sound GenTone(float freq, float dur, float vol, int waveType=0) {
    int sampleRate = 44100;
    int samples    = (int)(sampleRate * dur);
    std::vector<short> data(samples);
    for (int i = 0; i < samples; i++) {
        float t  = (float)i / sampleRate;
        float env = 1.0f - (float)i / samples; // simple decay
        float v  = 0;
        if (waveType == 0) v = sinf(2.0f * PI * freq * t);
        else if (waveType == 1) v = (sinf(2.0f * PI * freq * t) > 0) ? 1.0f : -1.0f;
        else v = (float)GetRandomValue(-100,100) / 100.0f;
        data[i] = (short)(v * env * vol * 32767);
    }
    Wave w = {(unsigned int)samples, (unsigned int)sampleRate, 16, 1, data.data()};
    return LoadSoundFromWave(w);
}

static void InitAudio() {
    InitAudioDevice();
    sSfxPaddle = GenTone(440.0f, 0.08f, 0.6f, 1);
    sSfxWall   = GenTone(220.0f, 0.06f, 0.4f, 1);
    sSfxScore  = GenTone(150.0f, 0.35f, 0.7f, 2);
    sSfxMenu   = GenTone(660.0f, 0.05f, 0.4f, 0);
    sSfxWin    = GenTone(523.0f, 0.6f,  0.7f, 0);
    
    sBgMusic = LoadMusicStream("music.mp3");
    gMusicLoaded = (sBgMusic.frameCount > 0);
    gMusicLoaded = true;
}

static void PlaySfx(Sound &snd, float vol) {
    SetSoundVolume(snd, vol);
    PlaySound(snd);
}

// ─────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────
static void DrawCenteredText(const char *txt, int y, int fs, Color col) {
    int tw = MeasureText(txt, fs);
    DrawText(txt, BASE_W/2 - tw/2, y, fs, col);
}

static bool DrawMenuButton(const char *txt, int y, int fs, bool selected, Color hlCol={255,210,0,255}) {
    Color col = selected ? hlCol : COL_DIM;
    if (selected) {
        // glow bar behind
        int tw = MeasureText(txt, fs);
        DrawRectangle(BASE_W/2 - tw/2 - 20, y - 6, tw + 40, fs + 12, {hlCol.r,hlCol.g,hlCol.b,30});
        // left/right arrows
        DrawText(">", BASE_W/2 - tw/2 - 30, y, fs, hlCol);
        DrawText("<", BASE_W/2 + tw/2 + 14, y, fs, hlCol);
    }
    DrawCenteredText(txt, y, fs, col);
    return false;
}

// ─────────────────────────────────────────────────────────────
//  GAME LOGIC HELPERS
// ─────────────────────────────────────────────────────────────
static void ResetBall(Ball &b, int towardPlayer, GameMode mode) {
    b.x = BASE_W / 2.0f;
    b.y = BASE_H / 2.0f;
    b.speed = BALL_SPD_INIT * (mode == MODE_SPEED ? 1.4f : 1.0f);
    float angle = (float)GetRandomValue(-25, 25) * DEG2RAD;
    b.vx = b.speed * (float)towardPlayer * cosf(angle);
    b.vy = b.speed * sinf(angle);
    // clear trail
    for (auto &tp : gTrail) tp = {{b.x,b.y},0};
}

// Improved bounce: offset from centre maps to angle steepness
static void PaddleBounce(Ball &b, Paddle &p, float normalDir, SaveData &sd,
                          int &rally, float sfxVol, GameMode mode) {
    Rectangle pr = {p.x, p.y, p.w, p.h};
    Rectangle br = {b.x - BALL_R, b.y - BALL_R, (float)BALL_R*2, (float)BALL_R*2};
    if (!CheckCollisionRecs(pr, br)) return;

    // Offset from paddle centre  (-1 top edge, +1 bottom edge)
    float centre   = p.y + p.h * 0.5f;
    float offset   = Clamp((b.y - centre) / (p.h * 0.5f), -1.0f, 1.0f);
    float maxAngle = 65.0f * DEG2RAD;
    float angle    = offset * maxAngle;

    float inc = (mode == MODE_SPEED) ? BALL_SPD_INC * 1.6f : BALL_SPD_INC;
    b.speed = Clamp(b.speed + inc, BALL_SPD_INIT, BALL_SPD_MAX);

    b.vx = normalDir * b.speed * cosf(angle);
    b.vy =             b.speed * sinf(angle);
    // spin from paddle movement
    b.vy += p.dy * 0.15f;
    b.vy  = Clamp(b.vy, -b.speed*0.95f, b.speed*0.95f);

    // push out
    b.x = (normalDir > 0) ? p.x + p.w + BALL_R + 1
                           : p.x - BALL_R - 1;

    rally++;
    sd.totalRallies++;
    if (rally > sd.longestRally) sd.longestRally = rally;

    // particles
    SpawnParticles({b.x, b.y}, p.col, 12, 2.5f);
    TriggerShake(b.speed > 700.0f ? 6.0f : 3.0f, 0.12f);
    PlaySfx(sSfxPaddle, sfxVol);
}

// ─────────────────────────────────────────────────────────────
//  AI
// ─────────────────────────────────────────────────────────────
struct AI {
    float reactionDelay;   // seconds before AI responds
    float reactionTimer;
    float targetY;         // where AI thinks it should go
    float errorBias;       // random offset applied to target
    float errorTimer;
    float speed;           // paddle movement speed
};

static void InitAI(AI &ai, Difficulty d) {
    switch (d) {
    case DIFF_EASY:
        ai.reactionDelay = 0.18f; ai.speed = 240.0f; break;
    case DIFF_MEDIUM:
        ai.reactionDelay = 0.08f; ai.speed = 360.0f; break;
    case DIFF_HARD:
        ai.reactionDelay = 0.02f; ai.speed = 520.0f; break;
    }
    ai.reactionTimer = 0;
    ai.errorBias     = 0;
    ai.errorTimer    = 0;
    ai.targetY       = BASE_H / 2.0f;
}

static void UpdateAI(AI &ai, Paddle &p, const Ball &ball, Difficulty d, float dt) {
    // Occasionally generate a new error offset
    ai.errorTimer -= dt;
    if (ai.errorTimer <= 0) {
        float maxErr = (d == DIFF_EASY) ? 80.0f : (d == DIFF_MEDIUM) ? 35.0f : 10.0f;
        ai.errorBias  = (float)GetRandomValue((int)(-maxErr*10),(int)(maxErr*10)) * 0.1f;
        ai.errorTimer = (float)GetRandomValue(8,25) * 0.1f;
    }

    ai.reactionTimer -= dt;
    if (ai.reactionTimer <= 0) {
        ai.reactionTimer = ai.reactionDelay;
        // Only track if ball is moving toward AI
        if (ball.vx > 0)
            ai.targetY = ball.y - p.h * 0.5f + ai.errorBias;
        else
            ai.targetY = BASE_H * 0.5f - p.h * 0.5f + ai.errorBias * 0.3f;
    }

    // Move paddle toward target
    float diff = Clamp(ai.targetY, 0, BASE_H - p.h) - p.y;
    float step = Clamp(diff, -ai.speed*dt, ai.speed*dt);
    p.dy = step / dt;
    p.y  = Clamp(p.y + step, 0, BASE_H - p.h);
}

// ─────────────────────────────────────────────────────────────
//  RANDOM GAME MODIFIER
// ─────────────────────────────────────────────────────────────
enum Modifier { MOD_NONE=0, MOD_BIGPADDLE, MOD_SMALLPADDLE, MOD_FAST, MOD_WOBBLE, MOD_COUNT };
static const char* MOD_NAMES[] = {"","Big Paddles","Small Paddles","Turbocharged","Wobbly Ball"};
static Modifier gModifier = MOD_NONE;

// ─────────────────────────────────────────────────────────────
//  GLOBAL GAME STATE
// ─────────────────────────────────────────────────────────────
static GameState  gState      = STATE_MAIN_MENU;
static Difficulty gDifficulty = DIFF_MEDIUM;
static GameMode   gMode       = MODE_CLASSIC;
static bool       gSinglePlayer = false;

static Paddle     gP1, gP2;
static Ball       gBall;
static AI         gAI;
static SaveData   gSave;

static int   gMenuSel    = 0;
static float gCountdown  = 3.0f;
static int   gCountInt   = 3;
static bool  gGameOver   = false;
static int   gWinner     = 0;
static int   gRally      = 0;
static float gLastSpeed  = 0;

// celebration
static float gCelebTimer = 0;

// ─────────────────────────────────────────────────────────────
//  PADDLE INIT
// ─────────────────────────────────────────────────────────────
static void ApplyModifier(float &ph) {
    switch (gModifier) {
    case MOD_BIGPADDLE:   ph = 140.0f; break;
    case MOD_SMALLPADDLE: ph =  60.0f; break;
    default:              ph = PADDLE_H; break;
    }
}

static void InitPaddles() {
    float ph = PADDLE_H;
    ApplyModifier(ph);
    gP1 = {30.0f,
            BASE_H/2.0f - ph/2.0f,
            BASE_H/2.0f - ph/2.0f,
            0, gP1.score,
            SKIN_COLORS[gSave.p1Skin],
            (float)PADDLE_W, ph,
            BASE_H/2.0f - ph/2.0f};
    gP2 = {(float)(BASE_W - 30 - PADDLE_W),
            BASE_H/2.0f - ph/2.0f,
            BASE_H/2.0f - ph/2.0f,
            0, gP2.score,
            SKIN_COLORS[gSave.p2Skin],
            (float)PADDLE_W, ph,
            BASE_H/2.0f - ph/2.0f};
}

static void StartNewGame() {
    gP1.score = 0; gP2.score = 0;
    gRally = 0;
    gGameOver = false; gWinner = 0;
    gModifier = (Modifier)GetRandomValue(0, MOD_COUNT - 1);
    InitPaddles();
    ResetBall(gBall, 1, gMode);
    gBall.col = COL_GOLD;
    InitAI(gAI, gDifficulty);
    gCountdown = 3.0f;
    gCountInt  = 3;
    gState = STATE_COUNTDOWN;
    gSave.totalGames++;
}

// ─────────────────────────────────────────────────────────────
//  ACHIEVEMENT CHECK
// ─────────────────────────────────────────────────────────────
static void CheckAchievements(SaveData &s) {
    auto unlock = [&](Achievement a, const char *name) {
        if (!(s.achievements & a)) {
            s.achievements |= a;
            SpawnPop({BASE_W/2.0f, BASE_H/2.0f + 60}, std::string("ACHIEVEMENT: ") + name, COL_GOLD);
        }
    };
    if (gWinner > 0)                unlock(ACH_FIRST_WIN,   "First Victory");
    if (gRally >= 10)               unlock(ACH_RALLY_10,    "Rally x10");
    if (gRally >= 25)               unlock(ACH_RALLY_25,    "Rally x25");
    if (gWinner==1 && gP2.score==0) unlock(ACH_PERFECT,     "Perfect Game");
    if (gLastSpeed > 700.0f)        unlock(ACH_SPEED_DEMON, "Speed Demon");
    if (s.unlockedSkins == (1<<SKIN_COUNT)-1) unlock(ACH_ALL_SKINS, "Collector");
    if (s.totalGames >= 50)         unlock(ACH_VETERAN,     "Veteran");
}

static void UnlockSkin(SaveData &s, int id) {
    if (!(s.unlockedSkins & (1 << id))) {
        s.unlockedSkins |= (1 << id);
        SpawnPop({BASE_W/2.0f, BASE_H/2.0f + 100}, std::string("SKIN UNLOCKED: ") + SKIN_NAMES[id], COL_CYAN);
    }
}

// ─────────────────────────────────────────────────────────────
//  DRAW FUNCTIONS
// ─────────────────────────────────────────────────────────────
static void DrawCentreNet() {
    for (int y = 0; y < BASE_H; y += 28)
        DrawRectangle(BASE_W/2 - 2, y, 4, 18, COL_LINE);
}

static void DrawPaddle(const Paddle &p) {
    // glow
    Color gc = p.col; gc.a = 40;
    DrawRectangleRounded({p.x-5, p.visualY-5, p.w+10, p.h+10}, 0.5f, 4, gc);
    // paddle
    DrawRectangleRounded({p.x, p.visualY, p.w, p.h}, 0.4f, 4, p.col);
}

static void DrawBall(const Ball &b) {
    DrawTrail(b.col);
    // glow layers
    Color gc = b.col; gc.a = 30;
    DrawCircle((int)b.x, (int)b.y, BALL_R + 8, gc);
    gc.a = 60;
    DrawCircle((int)b.x, (int)b.y, BALL_R + 4, gc);
    DrawCircle((int)b.x, (int)b.y, BALL_R, b.col);
}

static void DrawHUD() {
    // Scores
    std::string s1 = std::to_string(gP1.score);
    std::string s2 = std::to_string(gP2.score);
    DrawText(s1.c_str(), BASE_W/2 - 100 - MeasureText(s1.c_str(), 72), 18, 72, {200,200,255,200});
    DrawText(s2.c_str(), BASE_W/2 + 100, 18, 72, {200,200,255,200});

    // Rally & speed
    char buf[64];
    snprintf(buf, 64, "RALLY  %d", gRally);
    DrawText(buf, BASE_W/2 - MeasureText(buf,22)/2, 4, 22, COL_DIM);

    snprintf(buf, 64, "SPEED  %.0f", gLastSpeed);
    DrawText(buf, BASE_W - 150, BASE_H - 26, 18, COL_DIM);

    // Modifier
    if (gModifier != MOD_NONE) {
        snprintf(buf, 64, "MOD: %s", MOD_NAMES[gModifier]);
        DrawCenteredText(buf, BASE_H - 26, 18, COL_MAG);
    }

    // Mode indicator
    const char *modes[] = {"CLASSIC","SPEED","ENDLESS","PRACTICE"};
    DrawText(modes[gMode], 10, 10, 18, COL_DIM);

    // Difficulty (single player)
    if (gSinglePlayer) {
        const char *diffs[] = {"EASY","MEDIUM","HARD"};
        DrawText(diffs[gDifficulty], 10, 32, 18, COL_DIM);
    }

    // Controls hint
    DrawText("W/S", 10, BASE_H - 26, 18, COL_DIM);
    if (!gSinglePlayer) DrawText("\x18/\x19", BASE_W - 40, BASE_H - 26, 18, COL_DIM);

    if (gSave.showFPS) DrawFPS(BASE_W - 90, 10);
}

// ─────────────────────────────────────────────────────────────
//  MENU DRAWING
// ─────────────────────────────────────────────────────────────
static void DrawMainMenu(float t) {
    // Background stars
    DrawStars();

    // Scanline overlay
    for (int y = 0; y < BASE_H; y += 4)
        DrawRectangle(0, y, BASE_W, 2, {0,0,0,40});

    // Title glow
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);
    Color titleCol = {(unsigned char)(200+55*pulse), (unsigned char)(180+75*pulse), 255, 255};
    const char *title = "PONG";
    int tw = MeasureText(title, 96);
    // shadow / glow
    for (int dx = -3; dx <= 3; dx += 3)
        DrawText(title, BASE_W/2 - tw/2 + dx, 80 + dx, 96, {titleCol.r, titleCol.g, titleCol.b, 40});
    DrawText(title, BASE_W/2 - tw/2, 80, 96, titleCol);

    // Subtitle
    DrawCenteredText("C++ EDITION", 185, 22, COL_DIM);

    // Menu items
    const char *items[] = {"SINGLE PLAYER", "TWO PLAYERS", "GAME MODE", "SETTINGS", "STATS", "ACHIEVEMENTS", "CONTROLS", "QUIT"};
    int count = 8;
    int startY = 240;
    for (int i = 0; i < count; i++) {
        DrawMenuButton(items[i], startY + i * 52, 34, i == gMenuSel);
    }

    // Version tag
    DrawText("v1.1", BASE_W - 50, BASE_H - 22, 16, COL_DIM);
}

static void DrawPauseMenu() {
    DrawRectangle(0, 0, BASE_W, BASE_H, {0,0,0,170});
    DrawCenteredText("PAUSED", BASE_H/2 - 120, 64, COL_CYAN);
    const char *items[] = {"RESUME", "RESTART", "SETTINGS", "MAIN MENU"};
    for (int i = 0; i < 4; i++)
        DrawMenuButton(items[i], BASE_H/2 - 20 + i * 56, 34, i == gMenuSel);
}

static void DrawCountdown() {
    DrawStars();
    DrawCentreNet();
    DrawPaddle(gP1); DrawPaddle(gP2);

    // Modifier banner
    if (gModifier != MOD_NONE) {
        char buf[64];
        snprintf(buf, 64, "ROUND MODIFIER: %s", MOD_NAMES[gModifier]);
        DrawCenteredText(buf, BASE_H/2 + 70, 26, COL_MAG);
    }

    std::string cd = (gCountInt > 0) ? std::to_string(gCountInt) : "GO!";
    Color cdCol    = (gCountInt > 0) ? COL_GOLD : COL_GREEN;
    int   fs       = (gCountInt > 0) ? 120 : 100;
    float scale    = 1.0f + (1.0f - (gCountdown - (int)gCountdown)) * 0.4f;
    int   tw       = MeasureText(cd.c_str(), (int)(fs * scale));
    DrawText(cd.c_str(), BASE_W/2 - tw/2, BASE_H/2 - (int)(fs*scale)/2, (int)(fs*scale), cdCol);
}

static void DrawGameOver() {
    DrawRectangle(0, 0, BASE_W, BASE_H, {0,0,0,170});

    // Celebration particles burst over time
    if (gCelebTimer > 0) {
        Color cc = (gWinner == 1) ? COL_CYAN : COL_MAG;
        SpawnParticles({(float)GetRandomValue(0,BASE_W), (float)GetRandomValue(0,BASE_H)}, cc, 6, 3.0f);
    }

    std::string msg = (gMode == MODE_ENDLESS) ?
        "GAME OVER" :
        "PLAYER " + std::to_string(gWinner) + " WINS!";
    DrawCenteredText(msg.c_str(), BASE_H/2 - 80, 72, COL_GOLD);

    char buf[64];
    snprintf(buf, 64, "Final Score:  %d - %d", gP1.score, gP2.score);
    DrawCenteredText(buf, BASE_H/2 + 10, 32, COL_TEXT);

    snprintf(buf, 64, "Longest Rally: %d", gSave.longestRally);
    DrawCenteredText(buf, BASE_H/2 + 56, 26, COL_DIM);

    DrawCenteredText("R - Restart    ESC - Menu", BASE_H/2 + 110, 24, COL_DIM);
}

// ─────────────────────────────────────────────────────────────
//  DIFFICULTY SCREEN
// ─────────────────────────────────────────────────────────────
static void DrawDifficultyScreen() {
    DrawStars();
    DrawCenteredText("SELECT DIFFICULTY", 100, 52, COL_CYAN);
    const char *items[] = {"EASY - AI makes mistakes","MEDIUM - Balanced challenge","HARD - Formidable opponent"};
    for (int i = 0; i < 3; i++)
        DrawMenuButton(items[i], 240 + i * 80, 32, i == gMenuSel);
    DrawCenteredText("BACK", 520, 28, gMenuSel == 3 ? COL_GOLD : COL_DIM);
    if (gMenuSel == 3) {
        int tw = MeasureText("BACK",28);
        DrawText(">", BASE_W/2 - tw/2 - 30, 520, 28, COL_GOLD);
    }
}

// ─────────────────────────────────────────────────────────────
//  MODE SELECT SCREEN
// ─────────────────────────────────────────────────────────────
static void DrawModeScreen() {
    DrawStars();
    DrawCenteredText("SELECT GAME MODE", 100, 52, COL_CYAN);
    const char *items[] = {
        "CLASSIC  - First to 7",
        "SPEED    - Ball accelerates faster",
        "ENDLESS  - Survive as long as possible",
        "PRACTICE - No scoring, free play"
    };
    for (int i = 0; i < 4; i++)
        DrawMenuButton(items[i], 230 + i * 80, 28, i == (int)gMenuSel);
    DrawCenteredText("BACK", 570, 28, gMenuSel == 4 ? COL_GOLD : COL_DIM);
}

// ─────────────────────────────────────────────────────────────
//  CONTROLS SCREEN
// ─────────────────────────────────────────────────────────────
static void DrawControlsScreen() {
    DrawStars();
    DrawCenteredText("CONTROLS", 80, 52, COL_CYAN);
    const char *lines[] = {
        "Player 1: W / S",
        "Player 2: UP / DOWN",
        "Pause:    ESC",
        "Restart:  R",
        "Menu Nav: Arrow Keys + Enter",
        "Fullscreen: F11 / F",
    };
    for (int i = 0; i < 6; i++)
        DrawCenteredText(lines[i], 180 + i * 54, 28, COL_TEXT);
    DrawCenteredText("BACK - ESC", BASE_H - 70, 26, COL_DIM);
}

// ─────────────────────────────────────────────────────────────
//  SETTINGS SCREEN
// ─────────────────────────────────────────────────────────────
static void DrawSettingsScreen() {
    DrawStars();
    DrawCenteredText("SETTINGS", 80, 52, COL_CYAN);

    const char *labels[] = {
        "Master Volume",
        "SFX Volume",
        "Music Volume",
        "Fullscreen",
        "Show FPS",
        "Neon Theme",
        "BACK"
    };
    float *vols[] = {&gSave.masterVol, &gSave.sfxVol, &gSave.musicVol};

    for (int i = 0; i < 7; i++) {
        bool sel = (i == gMenuSel);
        Color c  = sel ? COL_GOLD : COL_DIM;
        DrawText(labels[i], BASE_W/2 - 280, 180 + i*58, 28, c);

        if (i < 3) {
            // slider
            float barX = BASE_W/2 + 20, barY = 180 + i*58 + 10, barW = 240, barH = 14;
            DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH, COL_LINE);
            DrawRectangle((int)barX, (int)barY, (int)(barW * (*vols[i])), (int)barH, sel ? COL_GOLD : COL_CYAN);
            char pct[8]; snprintf(pct,8,"%.0f%%", (*vols[i])*100);
            DrawText(pct, (int)barX + (int)barW + 10, (int)barY - 4, 22, c);
        } else if (i < 6) {
            bool *flags[] = {&gSave.fullscreen, &gSave.showFPS, &gSave.neonTheme};
            bool val = *flags[i-3];
            DrawText(val ? "[ON]" : "[OFF]", BASE_W/2 + 20, 180 + i*58, 28, val ? COL_GREEN : COL_RED);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  STATS SCREEN
// ─────────────────────────────────────────────────────────────
static void DrawStatsScreen() {
    DrawStars();
    DrawCenteredText("STATISTICS", 80, 52, COL_CYAN);
    char buf[128];
    const char* labels[] = {
        "Total Games Played", "Total Rallies", "Longest Rally",
        "Player 1 Wins", "Player 2/AI Wins",
        "High Score (Classic)", "High Score (Speed)"
    };
    int vals[] = {
        gSave.totalGames, gSave.totalRallies, gSave.longestRally,
        gSave.wins[0], gSave.wins[1],
        gSave.highScores[0], gSave.highScores[1]
    };
    for (int i = 0; i < 7; i++) {
        snprintf(buf, 128, "%s:  %d", labels[i], vals[i]);
        DrawCenteredText(buf, 180 + i * 52, 26, COL_TEXT);
    }
    DrawCenteredText("BACK - ESC", BASE_H - 60, 24, COL_DIM);
}

// ─────────────────────────────────────────────────────────────
//  ACHIEVEMENTS SCREEN
// ─────────────────────────────────────────────────────────────
static void DrawAchievementsScreen() {
    DrawStars();
    DrawCenteredText("ACHIEVEMENTS", 80, 52, COL_GOLD);
    int count = 7;
    for (int i = 0; i < count; i++) {
        bool unlocked = (gSave.achievements & ACH_VALUES[i]) != 0;
        Color c = unlocked ? COL_GOLD : COL_DIM;
        const char *prefix = unlocked ? "[X] " : "[ ] ";
        std::string txt = std::string(prefix) + ACH_NAMES[i];
        DrawCenteredText(txt.c_str(), 180 + i * 56, 28, c);
    }
    DrawCenteredText("BACK - ESC", BASE_H - 60, 24, COL_DIM);
}

// ─────────────────────────────────────────────────────────────
//  SKINS SCREEN (integrated into settings flow)
// ─────────────────────────────────────────────────────────────
static void DrawSkinsScreen() {
    DrawStars();
    DrawCenteredText("PADDLE SKINS", 80, 52, COL_CYAN);
    DrawCenteredText("Player 1", 150, 28, COL_TEXT);
    DrawCenteredText("Player 2", 150, 28, COL_TEXT);

    for (int i = 0; i < SKIN_COUNT; i++) {
        bool locked   = !(gSave.unlockedSkins & (1 << i));
        bool selP1    = (gMenuSel == i);
        Color skCol   = locked ? COL_DIM : SKIN_COLORS[i];
        Color bgCol   = selP1  ? (Color){skCol.r,skCol.g,skCol.b,40} : (Color){0,0,0,0};

        DrawRectangle(BASE_W/2 - 280, 200 + i*70, 240, 55, bgCol);
        DrawRectangleRounded({(float)(BASE_W/2 - 270), (float)(200 + i*70 + 12), 14, 36}, 0.4f, 4, skCol);
        DrawText(locked ? (std::string(SKIN_NAMES[i])+" [LOCKED]").c_str() : SKIN_NAMES[i],
                 BASE_W/2 - 245, 200 + i*70 + 14, 24, skCol);

        // P1 selector
        if (gSave.p1Skin == i) DrawText("<P1>", BASE_W/2 + 20, 200+i*70+14, 22, COL_CYAN);
        if (gSave.p2Skin == i) DrawText("<P2>", BASE_W/2 + 80, 200+i*70+14, 22, COL_MAG);
    }
    DrawCenteredText("ENTER to equip P1  |  SPACE to equip P2", BASE_H - 80, 22, COL_DIM);
    DrawCenteredText("BACK - ESC", BASE_H - 50, 22, COL_DIM);
}

// ─────────────────────────────────────────────────────────────
//  INPUT HANDLING
// ─────────────────────────────────────────────────────────────
static void HandleMenuInput() {
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) { gMenuSel--; PlaySfx(sSfxMenu, gSave.sfxVol*gSave.masterVol); }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { gMenuSel++; PlaySfx(sSfxMenu, gSave.sfxVol*gSave.masterVol); }
}

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(BASE_W, BASE_H, "PONG");
    Image icon = LoadImage("logo.png");
    SetWindowIcon(icon);
    UnloadImage(icon);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    LoadSave(gSave);
    InitAudio();
    InitStars();
    srand((unsigned)time(nullptr));

    // Unlock initial skins based on games played
    if (gSave.totalGames >= 5)  UnlockSkin(gSave, SKIN_CYAN);
    if (gSave.totalGames >= 15) UnlockSkin(gSave, SKIN_MAGENTA);
    if (gSave.totalGames >= 30) UnlockSkin(gSave, SKIN_GOLD);

    float t = 0;
    RenderTexture2D canvas = LoadRenderTexture(BASE_W, BASE_H);

    while (!WindowShouldClose()) {
        float dt  = GetFrameTime();
        float sfx = gSave.sfxVol * gSave.masterVol;
        t += dt;

        // Fullscreen toggle
        if (IsKeyPressed(KEY_F11) || IsKeyPressed(KEY_F)) {
            gSave.fullscreen = !gSave.fullscreen;
            if (gSave.fullscreen) SetWindowState(FLAG_FULLSCREEN_MODE);
            else                  ClearWindowState(FLAG_FULLSCREEN_MODE);
        }

        // ── UPDATE ──────────────────────────────────────────
        UpdateStars(dt, gState == STATE_PLAYING ? 80.0f : 220.0f);
        UpdateParticles(dt);
        UpdatePops(dt);

        if (gShakeTimer > 0) gShakeTimer -= dt;

        // ADDED: Background music playback control
        if (gMusicLoaded) {
            UpdateMusicStream(sBgMusic);
            SetMusicVolume(sBgMusic, gSave.musicVol * gSave.masterVol);
            
            // Play music in menu and game, pause when paused
            if (gState == STATE_PAUSED) {
                if (IsMusicStreamPlaying(sBgMusic)) PauseMusicStream(sBgMusic);
            } else {
                if (!IsMusicStreamPlaying(sBgMusic)) {
                    PlayMusicStream(sBgMusic);
                }
            }
        }

        switch (gState) {

        // ── MAIN MENU ──────────────────────────────────────
        case STATE_MAIN_MENU: {
            HandleMenuInput();
            int itemCount = 8;
            gMenuSel = ((gMenuSel % itemCount) + itemCount) % itemCount;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                PlaySfx(sSfxMenu, sfx);
                switch (gMenuSel) {
                case 0: gSinglePlayer = true;  gState = STATE_DIFFICULTY; gMenuSel=1; break;
                case 1: gSinglePlayer = false; StartNewGame(); break;
                case 2: gState = STATE_MODE_SELECT; gMenuSel = (int)gMode; break;
                case 3: gState = STATE_SETTINGS;    gMenuSel = 0; break;
                case 4: gState = STATE_STATS;       break;
                case 5: gState = STATE_ACHIEVEMENTS; break;
                case 6: gState = STATE_CONTROLS;    break;
                case 7: WriteSave(gSave); CloseWindow(); return 0;
                }
            }
            break;
        }

        // ── DIFFICULTY ─────────────────────────────────────
        case STATE_DIFFICULTY: {
            HandleMenuInput();
            gMenuSel = ((gMenuSel % 4) + 4) % 4;
            if (IsKeyPressed(KEY_ENTER)) {
                if (gMenuSel <= 2) {
                    gDifficulty = (Difficulty)gMenuSel;
                    StartNewGame();
                } else {
                    gState = STATE_MAIN_MENU; gMenuSel = 0;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) { gState = STATE_MAIN_MENU; gMenuSel = 0; }
            break;
        }

        // ── MODE SELECT ────────────────────────────────────
        case STATE_MODE_SELECT: {
            HandleMenuInput();
            gMenuSel = ((gMenuSel % 5) + 5) % 5;
            if (IsKeyPressed(KEY_ENTER)) {
                if (gMenuSel <= 3) { gMode = (GameMode)gMenuSel; }
                gState = STATE_MAIN_MENU; gMenuSel = 2;
            }
            if (IsKeyPressed(KEY_ESCAPE)) { gState = STATE_MAIN_MENU; gMenuSel = 2; }
            break;
        }

        // ── CONTROLS ───────────────────────────────────────
        case STATE_CONTROLS: {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
            { gState = STATE_MAIN_MENU; gMenuSel = 6; }
            break;
        }

        // ── SETTINGS ───────────────────────────────────────
        case STATE_SETTINGS: {
            HandleMenuInput();
            gMenuSel = ((gMenuSel % 7) + 7) % 7;

            float *vols[] = {&gSave.masterVol, &gSave.sfxVol, &gSave.musicVol};
            bool  *flags[] = {&gSave.fullscreen, &gSave.showFPS, &gSave.neonTheme};

            if (gMenuSel < 3) {
                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) *vols[gMenuSel] = Clamp(*vols[gMenuSel] + 0.1f, 0, 1);
                if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) *vols[gMenuSel] = Clamp(*vols[gMenuSel] - 0.1f, 0, 1);
            } else if (gMenuSel < 6) {
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT)) {
                    int fi = gMenuSel - 3;
                    *flags[fi] = !(*flags[fi]);
                    if (fi == 0) {
                        if (gSave.fullscreen) SetWindowState(FLAG_FULLSCREEN_MODE);
                        else                  ClearWindowState(FLAG_FULLSCREEN_MODE);
                    }
                }
            }
            if (gMenuSel == 6 && IsKeyPressed(KEY_ENTER)) {
                WriteSave(gSave);
                gState = STATE_MAIN_MENU; gMenuSel = 3;
            }
            if (IsKeyPressed(KEY_ESCAPE)) { WriteSave(gSave); gState = STATE_MAIN_MENU; gMenuSel = 3; }
            break;
        }

        // ── STATS ──────────────────────────────────────────
        case STATE_STATS: {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
            { gState = STATE_MAIN_MENU; gMenuSel = 4; }
            break;
        }

        // ── ACHIEVEMENTS ───────────────────────────────────
        case STATE_ACHIEVEMENTS: {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
            { gState = STATE_MAIN_MENU; gMenuSel = 5; }
            break;
        }

        // ── SKINS ──────────────────────────────────────────
        case STATE_SKINS: {
            HandleMenuInput();
            gMenuSel = ((gMenuSel % SKIN_COUNT) + SKIN_COUNT) % SKIN_COUNT;
            bool locked = !(gSave.unlockedSkins & (1 << gMenuSel));
            if (!locked) {
                if (IsKeyPressed(KEY_ENTER)) { gSave.p1Skin = gMenuSel; gP1.col = SKIN_COLORS[gMenuSel]; }
                if (IsKeyPressed(KEY_SPACE))  { gSave.p2Skin = gMenuSel; gP2.col = SKIN_COLORS[gMenuSel]; }
            }
            if (IsKeyPressed(KEY_ESCAPE)) { WriteSave(gSave); gState = STATE_SETTINGS; gMenuSel = 0; }
            break;
        }

        // ── COUNTDOWN ──────────────────────────────────────
        case STATE_COUNTDOWN: {
            int prev = gCountInt;
            gCountdown -= dt;
            gCountInt = (int)ceilf(gCountdown);
            if (gCountInt != prev && gCountInt >= 0)
                PlaySfx(sSfxMenu, sfx);
            if (gCountdown <= -0.6f)
                gState = STATE_PLAYING;
            break;
        }

        // ── PLAYING ────────────────────────────────────────
        case STATE_PLAYING: {
            if (IsKeyPressed(KEY_ESCAPE)) { gState = STATE_PAUSED; gMenuSel = 0; break; }

            // P1 input
            float p1dy = 0;
            if (IsKeyDown(KEY_W)) p1dy = -PADDLE_SPD;
            if (IsKeyDown(KEY_S)) p1dy =  PADDLE_SPD;
            gP1.dy = p1dy;
            gP1.y  = Clamp(gP1.y + p1dy * dt, 0, BASE_H - gP1.h);
            gP1.visualY = Lerp(gP1.visualY, gP1.y, 1.0f - expf(-20.0f * dt));

            if (gSinglePlayer) {
                UpdateAI(gAI, gP2, gBall, gDifficulty, dt);
                gP2.visualY = Lerp(gP2.visualY, gP2.y, 1.0f - expf(-20.0f * dt));
            } else {
                float p2dy = 0;
                if (IsKeyDown(KEY_UP))   p2dy = -PADDLE_SPD;
                if (IsKeyDown(KEY_DOWN)) p2dy =  PADDLE_SPD;
                gP2.dy = p2dy;
                gP2.y  = Clamp(gP2.y + p2dy * dt, 0, BASE_H - gP2.h);
                gP2.visualY = Lerp(gP2.visualY, gP2.y, 1.0f - expf(-20.0f * dt));
            }

            // Move ball
            float wobble = (gModifier == MOD_WOBBLE) ? sinf(t * 8.0f) * 60.0f : 0.0f;
            gBall.x += gBall.vx * dt;
            gBall.y += (gBall.vy + wobble) * dt;

            PushTrail({gBall.x, gBall.y});

            // Wall bounce
            if (gBall.y - BALL_R <= 0) {
                gBall.y = BALL_R;
                gBall.vy = fabsf(gBall.vy);
                SpawnParticles({gBall.x, 0}, COL_CYAN, 8, 2.0f);
                PlaySfx(sSfxWall, sfx);
            }
            if (gBall.y + BALL_R >= BASE_H) {
                gBall.y = BASE_H - BALL_R;
                gBall.vy = -fabsf(gBall.vy);
                SpawnParticles({gBall.x,(float)BASE_H}, COL_CYAN, 8, 2.0f);
                PlaySfx(sSfxWall, sfx);
            }

            // Paddle collisions
            PaddleBounce(gBall, gP1, 1.0f,  gSave, gRally, sfx, gMode);
            PaddleBounce(gBall, gP2, -1.0f, gSave, gRally, sfx, gMode);

            gLastSpeed = sqrtf(gBall.vx*gBall.vx + gBall.vy*gBall.vy);

            // Check achievements while playing
            CheckAchievements(gSave);

            // Scoring
            auto doScore = [&](int scorer) {
                if (scorer == 1) gP1.score++;
                else             gP2.score++;

                SpawnParticles({(float)(scorer==1?BASE_W-40:40), BASE_H/2.0f},
                               scorer==1?COL_CYAN:COL_MAG, 30, 3.5f);
                TriggerShake(8.0f, 0.2f);
                PlaySfx(sSfxScore, sfx);

                char sbuf[8]; snprintf(sbuf,8,"+1");
                SpawnPop({(float)(scorer==1?BASE_W/2-100:BASE_W/2+80), 100.0f}, sbuf,
                         scorer==1?COL_CYAN:COL_MAG);

                gRally = 0;

                bool p1Wins = gP1.score >= WIN_SCORE;
                bool p2Wins = gP2.score >= WIN_SCORE;

                if ((p1Wins || p2Wins) && gMode != MODE_PRACTICE && gMode != MODE_ENDLESS) {
                    gWinner = p1Wins ? 1 : 2;
                    gSave.wins[gWinner-1]++;
                    if (gSave.highScores[gMode] < std::max(gP1.score,gP2.score))
                        gSave.highScores[gMode] = std::max(gP1.score,gP2.score);
                    CheckAchievements(gSave);
                    WriteSave(gSave);
                    gCelebTimer = 4.0f;
                    gState = STATE_GAME_OVER;
                    PlaySfx(sSfxWin, sfx);
                } else {
                    // New round
                    gModifier = (Modifier)GetRandomValue(0, MOD_COUNT-1);
                    InitPaddles();
                    ResetBall(gBall, scorer==1 ? -1 : 1, gMode);
                    gCountdown = 3.0f;
                    gCountInt  = 3;
                    gState = STATE_COUNTDOWN;
                }
            };

            if (gBall.x + BALL_R < 0)     doScore(2);
            else if (gBall.x - BALL_R > BASE_W) doScore(1);

            break;
        }

        // ── PAUSED ─────────────────────────────────────────
        case STATE_PAUSED: {
            HandleMenuInput();
            gMenuSel = ((gMenuSel % 4) + 4) % 4;
            if (IsKeyPressed(KEY_ESCAPE)) { gState = STATE_PLAYING; }
            if (IsKeyPressed(KEY_ENTER)) {
                switch (gMenuSel) {
                case 0: gState = STATE_PLAYING;   break;
                case 1: StartNewGame();            break;
                case 2: gState = STATE_SETTINGS; gMenuSel = 0; break;
                case 3: gState = STATE_MAIN_MENU; gMenuSel = 0; WriteSave(gSave); break;
                }
            }
            break;
        }

        // ── GAME OVER ──────────────────────────────────────
        case STATE_GAME_OVER: {
            gCelebTimer -= dt;
            if (IsKeyPressed(KEY_R))   { StartNewGame(); }
            if (IsKeyPressed(KEY_ESCAPE)) { gState = STATE_MAIN_MENU; gMenuSel = 0; }
            break;
        }

        default: break;
        }

        // ── DRAW ────────────────────────────────────────────
        // Render to texture for possible post-processing
        BeginTextureMode(canvas);
        ClearBackground(COL_BG);

        if (gSave.neonTheme) {
            DrawRectangleGradientV(0, 0, BASE_W, BASE_H/2, {10,0,30,120}, {0,0,0,0});
            DrawRectangleGradientV(0, BASE_H/2, BASE_W, BASE_H/2, {0,0,0,0}, {0,10,40,120});
        }

        // Apply screen shake offset
        Vector2 shk = GetShakeOffset();
        BeginMode2D({{shk.x, shk.y},{0,0},0,1});

        switch (gState) {
        case STATE_MAIN_MENU:    DrawMainMenu(t);        break;
        case STATE_DIFFICULTY:   DrawDifficultyScreen(); break;
        case STATE_MODE_SELECT:  DrawModeScreen();       break;
        case STATE_CONTROLS:     DrawControlsScreen();   break;
        case STATE_SETTINGS:     DrawSettingsScreen();   break;
        case STATE_STATS:        DrawStatsScreen();      break;
        case STATE_ACHIEVEMENTS: DrawAchievementsScreen(); break;
        case STATE_SKINS:        DrawSkinsScreen();      break;

        case STATE_COUNTDOWN:
            DrawCountdown();
            DrawHUD();
            break;

        case STATE_PLAYING:
        case STATE_PAUSED: {
            DrawStars();
            DrawCentreNet();
            DrawPaddle(gP1);
            DrawPaddle(gP2);
            DrawBall(gBall);
            DrawParticles();
            DrawPops();
            DrawHUD();
            if (gState == STATE_PAUSED) DrawPauseMenu();
            break;
        }

        case STATE_GAME_OVER: {
            DrawStars();
            DrawCentreNet();
            DrawPaddle(gP1);
            DrawPaddle(gP2);
            DrawParticles();
            DrawHUD();
            DrawGameOver();
            DrawPops();
            break;
        }

        default: break;
        }

        EndMode2D();
        EndTextureMode();

        // Blit canvas to window (handles resizing)
        BeginDrawing();
        ClearBackground(BLACK);
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float scale = std::min((float)sw/BASE_W, (float)sh/BASE_H);
        int ox = (sw - (int)(BASE_W*scale))/2;
        int oy = (sh - (int)(BASE_H*scale))/2;
        DrawTexturePro(canvas.texture,
            {0,0,(float)BASE_W,-(float)BASE_H},
            {(float)ox,(float)oy,(float)(BASE_W*scale),(float)(BASE_H*scale)},
            {0,0}, 0, WHITE);
        EndDrawing();
    }

    WriteSave(gSave);
    UnloadRenderTexture(canvas);
    UnloadSound(sSfxPaddle);
    UnloadSound(sSfxWall);
    UnloadSound(sSfxScore);
    UnloadSound(sSfxMenu);
    UnloadSound(sSfxWin);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}