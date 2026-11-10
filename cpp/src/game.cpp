// game.cpp - ROCKS OF RUIN, an Arcade Phantasm game by Cursed Entertainment, ported from the browser's site/game.js
// and site/sfx.js. Fly an enormous asteroid field. Rocks split big > medium > small > dust; some hide alien nests, and
// what crawls out hunts you. Pick up salvage and spend it in the salvage bay (U). The field turns nastier the deeper
// you go from the beacon at its heart, and the longer you last: denser, faster rocks, more nests, debris storms.
#include "game.h"
#include "splash.h"
#include "synth.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr float PI = 3.14159265358979f, TAU = PI * 2;
float rnd() { return Synth::rand01(); }               // Math.random()
float rr(float a, float b) { return a + rnd() * (b - a); }
float clamp(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
float angDiff(float a, float b) { float d = std::fmod(b - a, TAU); if (d > PI) d -= TAU; if (d < -PI) d += TAU; return d; }
std::string fixed1(float v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }

// ---- the sound (site/sfx.js) ------------------------------------------------------------------------------------
namespace Sfx {
using namespace Synth;
bool awake = false, muted = false;
int thrustV = 0;
double lastShot = 0, lastPick = 0;

void wake() {                                       // the browser starts its audio on the first key or tap
    if (awake || !Synth::ready()) return;
    awake = true;
    busGain(0, muted ? 0.f : 0.32f);
    // the engine: a loop of noise through a low band-pass, faded in while thrusting
    Voice v;
    v.wave = NoiseLoop; v.filter = Bandpass; v.cutoff = Param(180); v.q = 0.8f; v.gain = Param(0);
    thrustV = play(v);
}
bool ok() { return awake && !muted; }
void shoot() {                                      // a short laser blip
    if (!ok()) return;
    const double t = now();
    if (t - lastShot < 0.05) return;
    lastShot = t;
    tone(Square, 1100 + rnd() * 120, 260, t, 0.09f, 0.09f);
}
void boom(int size) {                               // rocks break: bigger rocks, deeper booms (size 0 = big)
    if (!ok()) return;
    const double t = now();
    const float ks[4] = {1, 0.7f, 0.45f, 0.25f}, k = size >= 0 && size < 4 ? ks[size] : 0.3f;
    noise(t, 0.25f + k * 0.5f, 0.5f * k + 0.1f, 500 + (1 - k) * 2500);
    tone(Sine, 140 * (1.6f - k), 40, t, 0.3f + k * 0.4f, 0.5f * k);
}
void crack() {                                      // a nest splits open: a dissonant shriek sliding down
    if (!ok()) return;
    const double t = now();
    tone(Sawtooth, 1400, 300, t, 0.6f, 0.12f);
    tone(Sawtooth, 1480, 290, t + 0.03, 0.6f, 0.1f);
    tone(Square, 90, 60, t, 0.7f, 0.2f);
    noise(t, 0.4f, 0.3f, 4000);
}
void spit() {                                       // an alien spits
    if (!ok()) return;
    const double t = now();
    tone(Sawtooth, 300, 900, t, 0.12f, 0.08f);
    noise(t, 0.08f, 0.15f, 3000);
}
void squelch() {                                    // an alien dies
    if (!ok()) return;
    const double t = now();
    tone(Sawtooth, 600, 70, t, 0.35f, 0.18f);
    tone(Square, 220, 50, t, 0.3f, 0.12f);
    noise(t, 0.25f, 0.3f, 1200);
}
void pick() {                                       // salvage collected: a chime, higher on a run
    if (!ok()) return;
    const double t = now();
    const float up = t - lastPick < 0.4 ? 1.15f : 1.f;
    lastPick = t;
    tone(Triangle, 1320 * up, 1320 * up, t, 0.08f, 0.3f);
    tone(Sine, 1980 * up, 1980 * up, t + 0.05, 0.12f, 0.25f);
}
void hit() {                                        // the hull takes damage
    if (!ok()) return;
    const double t = now();
    tone(Square, 160, 40, t, 0.35f, 0.3f);
    noise(t, 0.3f, 0.5f, 900);
}
void shield() {                                     // the shield soaks a hit
    if (!ok()) return;
    const double t = now();
    tone(Sine, 700, 1400, t, 0.18f, 0.25f);
    tone(Triangle, 350, 700, t, 0.18f, 0.2f);
}
void buy() {                                        // an upgrade bought: a rising arpeggio
    if (!ok()) return;
    const double t = now();
    const float fs[4] = {523, 659, 784, 1047};
    for (int i = 0; i < 4; i++) tone(Square, fs[i], fs[i], t + i * 0.06, 0.12f, 0.12f);
}
void deny() {
    if (!ok()) return;
    tone(Square, 150, 140, now(), 0.15f, 0.15f);
}
void storm() {                                      // a debris storm is coming: a two-tone siren
    if (!ok()) return;
    const double t = now();
    for (int i = 0; i < 4; i++) tone(Square, i % 2 ? 520.f : 780.f, i % 2 ? 520.f : 780.f, t + i * 0.22, 0.2f, 0.12f);
}
void over() {
    if (!ok()) return;
    const double t = now();
    tone(Square, 440, 55, t, 1.4f, 0.25f);
    tone(Triangle, 220, 30, t, 1.6f, 0.4f);
    noise(t, 1, 0.5f, 600);
}
void thrust(bool on) {
    if (!awake || !thrustV) return;
    target(thrustV, Gain, on && !muted ? 0.5f : 0.f, 0.05f);
}
bool toggleMute() {
    muted = !muted;
    Platform::store("rocksOfRuinMuted", muted ? "1" : "0");
    if (awake) busGain(0, muted ? 0.f : 0.32f);
    if (muted && thrustV) set(thrustV, Gain, 0);
    return muted;
}
bool isMuted() { return muted; }
}  // namespace Sfx

// ---- the game (site/game.js) ------------------------------------------------------------------------------------
namespace C {
const Color navy("#001725"), deep("#005184"), blue("#007ab7"), sky("#34a9e3"), pink("#f088e9"), acid("#39ff6a"),
    black("#020611"), white("#ffffff");
}

constexpr float FIELD = 7000;                       // the field is a disc this big around the beacon
namespace ROCK {                                    // per size: radius, hits, score, salvage chance and value
const float r[4] = {74, 42, 22, 10};
const int hp[4] = {4, 2, 1, 1}, score[4] = {20, 50, 100, 10};
const float drop[4] = {0.55f, 0.35f, 0.18f, 0.04f};
const int val[4] = {5, 3, 2, 1};
const float hurt[4] = {34, 20, 11, 4};
}
struct AlienKind { float r; int hp; float speed, bite; int score, salvage; Color col; };
const std::map<std::string, AlienKind> ALIEN = {
    {"skitter", {12, 1, 250, 10, 150, 4, C::acid}},
    {"spitter", {16, 3, 170, 12, 250, 6, C::pink}},
    {"leech", {26, 8, 110, 22, 400, 12, C::acid}},
    {"wraith", {18, 2, 210, 16, 300, 8, C::sky}},
};
struct Upgrade { const char* id; const char* name; const char* desc; std::vector<int> cost; bool repeat; };
const Upgrade UPGRADES[7] = {
    {"rate", "Rate of Fire", "Fire faster", {15, 30, 50, 80, 120}, false},
    {"spread", "Spread Shot", "3, then 5 rounds, then a tail gun", {40, 90, 160}, false},
    {"pierce", "Piercing Rounds", "Rounds punch through targets", {35, 80, 140}, false},
    {"homing", "Homing", "Rounds curve onto targets", {50, 100, 170}, false},
    {"shield", "Shields", "A recharging shield over the hull", {25, 55, 95, 150}, false},
    {"engine", "Thrusters", "More thrust, more top speed", {20, 50, 90}, false},
    {"repair", "Hull Repair", "Patch 40 hull", {20}, true},
};

struct Rock {
    int id; float x, y, vx, vy, r; int size; float a, spin;
    std::vector<float> pts, cracks;
    bool iron = false; std::string nest; int hp; float flash = 0, wob; bool storm = false;
};
struct Alien {
    int id; std::string kind; float x, y, vx, vy, r; int hp; float emerge, emergeMax, t, fire, dash, dashing, phase;
    bool phased; float flash, a;
};
struct Bullet { float x, y, vx, vy, life; int pierce; std::vector<int> hits; int target = 0; float seek = 0; };
struct Spit { float x, y, vx, vy, life; };
struct Salvage { float x, y, vx, vy; int val; float life, a; };
struct Part { float x, y, vx, vy, life, max; Color c; float s; };
struct Ring { float x, y, r; Color c; float life, max; };
struct Text { float x, y; std::string t; Color c; float life, max; };
struct StarP { float x, y, r, t; };
struct Nebula { float x, y, r; Color c; };
struct Ship { float x = 0, y = 0, vx = 0, vy = 0, a = -PI / 2, hull = 100, shield = 0, regen = 0, inv = 2, cool = 0; bool thrust = false; float dead = 0; };
struct Storm { float a, t, warn, spawn; };
struct Banner { std::string text; float t; Color c; bool has = false; };

float W = 0, H = 0, Z = 1, VW = 0, VH = 0;          // Z zooms out on small screens; VW x VH is the view in world units
std::vector<std::vector<StarP>> stars;
std::vector<Nebula> nebulae;
Ship ship;
std::vector<Rock> rocks;
std::vector<Alien> aliens;
std::vector<Bullet> bullets;
std::vector<Spit> spits;
std::vector<Salvage> salvage;
std::vector<Part> parts;
std::vector<Ring> rings;
std::vector<Text> texts;
int score = 0, best = 0, salv = 0, nextId = 1;
std::map<std::string, int> lv;
float elapsed = 0, danger = 1, shake = 0, popTimer = 0, nextStorm = 70;
std::string state = "playing";
struct { float x = 0, y = 0; } cam;
bool hasStorm = false, newBest = false, started_ = false, haveShip = false;
Storm storm{};
Banner banner;
std::map<std::string, bool> keys;
double last = 0;
float splashFade = 1;
int hover = -1;                                     // the salvage bay row under the mouse (7: FLY ON)
Texture logo;
Canvas* ctx = nullptr;

bool key(const char* k) { auto it = keys.find(k); return it != keys.end() && it->second; }
float dist2(float ax, float ay, float bx, float by) { const float dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }
template <class T> const T& pick(const std::vector<T>& l) { return l[(size_t)std::floor(rnd() * l.size()) % l.size()]; }

// ---------------------------------------------------------------- setup
void resizeView(float w, float h) {
    W = w; H = h;
    Z = clamp(std::min(W, H) / 720, 0.55f, 1);
    VW = W / Z; VH = H / Z;
}

void makeStars() {                                  // three parallax layers, each a 1024 tile
    stars.clear();
    for (int l = 0; l < 3; l++) {
        std::vector<StarP> layer;
        for (int i = 0; i < 110; i++) layer.push_back({rnd() * 1024, rnd() * 1024, rr(0.4f, 1.2f + l * 0.6f), rnd() * TAU});
        stars.push_back(layer);
    }
    nebulae.clear();
    for (int n = 0; n < 26; n++) {
        const float a = rnd() * TAU, d = std::sqrt(rnd()) * FIELD;
        nebulae.push_back({std::cos(a) * d, std::sin(a) * d, rr(500, 1300), pick(std::vector<Color>{C::deep, C::pink, C::blue, C::navy})});
    }
}

void jagged(float r, std::vector<float>& pts, std::vector<float>& cracks) {   // a ring of points pushed in and out
    const int n = (int)std::floor(rr(9, 15));
    for (int i = 0; i < n; i++) {
        const float a = ((float)i / n) * TAU + rr(-0.18f, 0.18f);
        const float k = rnd() < 0.25f ? rr(0.55f, 0.75f) : rr(0.8f, 1.08f);
        pts.push_back(a); pts.push_back(r * k);
    }
    for (int c = 0; c < 2 + n / 5.f; c++) {                 // a few surface lines, drawn faint
        cracks.push_back(rnd() * TAU); cracks.push_back(rr(0.15f, 0.5f) * r); cracks.push_back(rnd() * TAU); cracks.push_back(rr(0.2f, 0.6f) * r);
    }
}

float depthAt(float x, float y) { return std::sqrt(x * x + y * y) / FIELD; }   // 0 at the beacon, 1 at the edge

Rock& newRock(float x, float y, int size, float vx, float vy, bool iron = false, const std::string& nest = "") {
    Rock rock;
    rock.id = nextId++;
    rock.r = ROCK::r[size] * rr(0.85f, 1.15f);
    jagged(rock.r, rock.pts, rock.cracks);
    rock.x = x; rock.y = y; rock.vx = vx; rock.vy = vy; rock.size = size; rock.a = rnd() * TAU;
    rock.spin = rr(-1, 1) * (0.2f + size * 0.35f);
    rock.iron = iron; rock.nest = nest; rock.hp = ROCK::hp[size]; rock.wob = rnd() * 10;
    if (rock.iron) rock.hp *= 2;
    rocks.push_back(std::move(rock));
    return rocks.back();
}

float nestChance(float d) { return std::min(0.38f, 0.03f + d * 0.22f + danger * 0.02f); }
std::string nestKind() {
    std::vector<std::string> k = {"skitter", "skitter", "spitter"};
    if (danger > 2) { k.push_back("leech"); k.push_back("spitter"); }
    if (danger > 3) { k.push_back("wraith"); k.push_back("leech"); }
    if (danger > 5) { k.push_back("wraith"); k.push_back("wraith"); }
    return pick(k);
}

void seedRock(float x, float y, float spd) {        // a fresh big or medium rock, maybe iron, maybe a nest
    const float d = depthAt(x, y);
    const int size = rnd() < 0.7f ? 0 : 1;
    const float a = rnd() * TAU, v = rr(10, spd);
    const bool iron = rnd() < 0.12f + d * 0.15f;
    const std::string nest = rnd() < nestChance(d) ? nestKind() : "";
    newRock(x, y, size, std::cos(a) * v, std::sin(a) * v, iron, nest);
}

void closeMenu(bool quiet = false) {
    if (!quiet && state == "menu") state = "playing";
}

void reset() {
    ship = Ship();
    haveShip = true;
    rocks.clear(); aliens.clear(); bullets.clear(); spits.clear(); salvage.clear(); parts.clear(); rings.clear(); texts.clear();
    score = 0; salv = 0; elapsed = 0; danger = 1; shake = 0; popTimer = 0; newBest = false;
    lv = {{"rate", 0}, {"spread", 0}, {"pierce", 0}, {"homing", 0}, {"shield", 0}, {"engine", 0}, {"repair", 0}};
    hasStorm = false; nextStorm = 70; banner = {"FIND THE SALVAGE. MIND THE ROCKS.", 3.5f, C::sky, true};
    banner.c = Color(0, 0, 0, 0);
    cam.x = 0; cam.y = 0;
    // the field: sparse round the beacon, thicker towards the edge
    for (int i = 0; i < 1500; i++) {
        const float a = rnd() * TAU, d = 700 + std::sqrt(rnd()) * (FIELD - 700);
        if (rnd() > 0.35f + 0.65f * d / FIELD) continue;
        seedRock(std::cos(a) * d, std::sin(a) * d, 30 + 90 * d / FIELD);
    }
    for (int j = 0; j < 12; j++) {                      // a few easy rocks round the beacon to warm up on
        const float sa = j / 12.f * TAU + rr(-0.2f, 0.2f), sd = rr(330, 800);
        newRock(std::cos(sa) * sd, std::sin(sa) * sd, rnd() < 0.6f ? 0 : 1, rr(-15, 15), rr(-15, 15), false, j == 5 ? "skitter" : "");
    }
    state = "playing";
    closeMenu(true);
}

// ---------------------------------------------------------------- effects
void burst(float x, float y, int n, float speed, const std::vector<Color>& cols, float life = 0.6f, float size = 2.2f) {
    for (int i = 0; i < n; i++) {
        const float a = rnd() * TAU, v = speed * (0.2f + rnd() * 0.8f), l = life * (0.5f + rnd() * 0.8f);
        parts.push_back({x, y, std::cos(a) * v, std::sin(a) * v, l, l, pick(cols), size});
    }
}
void ring(float x, float y, float r, const Color& c, float life = 0.45f) { rings.push_back({x, y, r, c, life, life}); }
void floatText(float x, float y, const std::string& t, const Color& c = C::white) { texts.push_back({x, y, t, c, 1, 1}); }
void addShake(float v) {
    const float d = std::sqrt(dist2(ship.x, ship.y, cam.x, cam.y));
    shake = std::min(18.f, shake + v);
    if (d > 1200) shake *= 0.5f;
}

// ---------------------------------------------------------------- breaking things
void spawnAlien(const std::string& kind, float x, float y, float emerge = 0) {
    const AlienKind& d = ALIEN.at(kind);
    Alien al;
    al.id = nextId++; al.kind = kind; al.x = x; al.y = y; al.vx = rr(-60, 60); al.vy = rr(-60, 60); al.r = d.r;
    al.hp = d.hp + (int)std::floor(danger / 4) * (kind == "leech" ? 2 : 0);
    al.emerge = emerge; al.emergeMax = emerge > 0 ? emerge : 1; al.t = rnd() * 10; al.fire = rr(1, 2.2f); al.dash = rr(2, 4);
    al.dashing = 0; al.phase = rr(0.5f, 2); al.phased = false; al.flash = 0; al.a = rnd() * TAU;
    aliens.push_back(al);
}

void dropSalvage(float x, float y, int val) {
    const int n = std::max(1, (int)std::round(val / 3.f));
    for (int i = 0; i < n; i++) {
        const float a = rnd() * TAU, v = rr(30, 110);
        salvage.push_back({x, y, std::cos(a) * v, std::sin(a) * v, std::max(1, (int)std::round((float)val / n)), 30, rnd() * TAU});
    }
}

void hatch(const Rock& rock) {                      // the rock was a nest: something crawls out
    const std::string kind = rock.nest;
    int n = kind == "skitter" ? 3 + (int)std::floor(danger / 2) : kind == "leech" ? 1 : 1 + (int)std::floor(danger / 3);
    n = std::min(n, 7);
    for (int i = 0; i < n; i++) spawnAlien(kind, rock.x + rr(-15, 15), rock.y + rr(-15, 15), 0.7f + i * 0.12f);
    burst(rock.x, rock.y, 50, 260, {C::acid, C::acid, C::pink, C::white}, 1, 3);
    ring(rock.x, rock.y, rock.r * 2.6f, C::acid, 0.7f);
    Sfx::crack();
    floatText(rock.x, rock.y - rock.r, "SOMETHING CRAWLS OUT", C::acid);
    addShake(6);
}

void breakRock(int idx, float bvx, float bvy) {
    const Rock rock = rocks[idx];
    rocks.erase(rocks.begin() + idx);
    score += ROCK::score[rock.size];
    const std::vector<Color> cols = rock.iron ? std::vector<Color>{C::blue, C::sky, C::white} : std::vector<Color>{C::sky, C::white, C::deep};
    burst(rock.x, rock.y, 10 + (3 - rock.size) * 12, 120.f + (3 - rock.size) * 60, cols, 0.8f, 2.5f);
    ring(rock.x, rock.y, rock.r * 1.6f, rock.iron ? C::blue : C::sky);
    const float sh[4] = {7, 4, 2, 0.5f};
    addShake(sh[rock.size]);
    Sfx::boom(rock.size);
    if (rock.size < 3) {
        const int n = rock.size == 1 ? 2 : (rnd() < 0.5f ? 2 : 3);
        for (int i = 0; i < n; i++) {
            const float a = rnd() * TAU, v = rr(40, 120) * (1 + rock.size * 0.25f) * (1 + danger * 0.05f);
            Rock& child = newRock(rock.x + std::cos(a) * rock.r * 0.35f, rock.y + std::sin(a) * rock.r * 0.35f, rock.size + 1,
                                  rock.vx + std::cos(a) * v + bvx * 0.05f, rock.vy + std::sin(a) * v + bvy * 0.05f, rock.iron);
            child.flash = 0.15f;
        }
    }
    if (rnd() < ROCK::drop[rock.size] * (rock.iron ? 1.6f : 1.f)) dropSalvage(rock.x, rock.y, ROCK::val[rock.size] * (rock.iron ? 2 : 1));
    if (!rock.nest.empty()) hatch(rock);
}

void killAlien(int idx) {
    const Alien al = aliens[idx];
    const AlienKind& d = ALIEN.at(al.kind);
    aliens.erase(aliens.begin() + idx);
    score += d.score;
    floatText(al.x, al.y, "+" + std::to_string(d.score), d.col);
    burst(al.x, al.y, 34, 220, {d.col, C::acid, C::white}, 0.9f, 3);
    ring(al.x, al.y, al.r * 3, d.col);
    dropSalvage(al.x, al.y, d.salvage);
    addShake(3);
    Sfx::squelch();
    if (al.kind == "leech") {                           // a leech splits into skitters
        spawnAlien("skitter", al.x - 10, al.y, 0.25f);
        spawnAlien("skitter", al.x + 10, al.y, 0.25f);
    }
}

void die() {
    ship.hull = 0;
    state = "over";
    ship.dead = 0;
    burst(ship.x, ship.y, 120, 420, {C::pink, C::white, C::sky, C::acid}, 1.6f, 3.5f);
    ring(ship.x, ship.y, 160, C::pink, 1);
    ring(ship.x, ship.y, 90, C::white, 0.7f);
    addShake(18);
    Sfx::thrust(false);
    Sfx::over();
    if (score > best) { best = score; newBest = true; Platform::store("rocksOfRuinBest", std::to_string(best)); }
}

void hurt(float amount, float fromX, float fromY) {
    if (ship.inv > 0 || state != "playing") return;
    ship.regen = 2.5f;
    if (ship.shield > 0) {
        const float soak = std::min(ship.shield, amount);
        ship.shield -= soak; amount -= soak;
        ring(ship.x, ship.y, 40, C::sky, 0.35f);
        Sfx::shield();
    }
    if (amount > 0) {
        ship.hull -= amount;
        burst(ship.x, ship.y, 22, 200, {C::pink, C::white}, 0.6f, 2.5f);
        addShake(8);
        Sfx::hit();
    }
    ship.inv = 0.7f;
    const float dx = ship.x - fromX, dy = ship.y - fromY;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d == 0) d = 1;
    ship.vx += dx / d * 220; ship.vy += dy / d * 220;
    if (ship.hull <= 0) die();
}

// ---------------------------------------------------------------- shooting
void fire() {
    const float interval = 0.24f * std::pow(0.8f, (float)lv["rate"]);
    ship.cool = interval;
    std::vector<float> angles = {0};
    if (lv["spread"] >= 1) angles = {-0.13f, 0, 0.13f};
    if (lv["spread"] >= 2) angles = {-0.26f, -0.13f, 0, 0.13f, 0.26f};
    if (lv["spread"] >= 3) angles.push_back(PI);
    for (float off : angles) {
        const float a = ship.a + off;
        bullets.push_back({ship.x + std::cos(a) * 16, ship.y + std::sin(a) * 16, std::cos(a) * 900 + ship.vx, std::sin(a) * 900 + ship.vy,
                           0.95f, lv["pierce"], {}, 0, 0});
    }
    Sfx::shoot();
}

bool has(const std::vector<int>& v, int id) { return std::find(v.begin(), v.end(), id) != v.end(); }

int nearestTarget(const Bullet& b) {
    int bestT = 0;
    float bd = 520 * 520;
    for (const Alien& al : aliens) {
        if (al.phased || al.emerge > 0) continue;
        const float d = dist2(b.x, b.y, al.x, al.y) * 0.5f;   // aliens first
        if (d < bd && !has(b.hits, al.id)) { bd = d; bestT = al.id; }
    }
    for (const Rock& r : rocks) {
        if (std::fabs(r.x - b.x) > 520 || std::fabs(r.y - b.y) > 520) continue;
        const float d2 = dist2(b.x, b.y, r.x, r.y);
        if (d2 < bd && !has(b.hits, r.id)) { bd = d2; bestT = r.id; }
    }
    return bestT;
}
bool findTarget(int id, float& x, float& y) {
    for (const Alien& a : aliens) if (a.id == id) { x = a.x; y = a.y; return true; }
    for (const Rock& r : rocks) if (r.id == id) { x = r.x; y = r.y; return true; }
    return false;
}

// ---------------------------------------------------------------- the world getting worse
void populate(float dt) {
    popTimer -= dt;
    if (popTimer > 0) return;
    popTimer = 0.8f;
    int near = 0;
    for (const Rock& r : rocks) if (std::fabs(r.x - ship.x) < 2400 && std::fabs(r.y - ship.y) < 2400) near++;
    const float want = std::min(150.f, 34 + danger * 11);
    for (int n = 0; n < 3 && near < want; n++, near++) {
        const float a = rnd() * TAU, d = rr(std::max(VW, VH) * 0.75f + 120, 2300);
        const float x = ship.x + std::cos(a) * d, y = ship.y + std::sin(a) * d;
        if (x * x + y * y > FIELD * FIELD * 0.97f) continue;
        seedRock(x, y, 40 + danger * 14);
    }
    // keep the count sane: drop rocks far from the ship once there are lots
    if (rocks.size() > 2400) {
        for (int k = (int)rocks.size() - 1; k >= 0 && rocks.size() > 2200; k--)
            if (dist2(rocks[k].x, rocks[k].y, ship.x, ship.y) > 5000.f * 5000.f) rocks.erase(rocks.begin() + k);
    }
}

void updateStorm(float dt) {
    if (!hasStorm) {
        if (danger < 2.3f) return;
        nextStorm -= dt;
        if (nextStorm <= 0) {
            storm = {rnd() * TAU, 16, 2.5f, 0};
            hasStorm = true;
            banner = {"DEBRIS STORM INCOMING", 3, C::pink, true};
            Sfx::storm();
        }
        return;
    }
    if (storm.warn > 0) { storm.warn -= dt; return; }
    storm.t -= dt;
    storm.spawn -= dt;
    if (storm.spawn <= 0) {                             // small, fast rocks blown across the view from one side
        storm.spawn = std::max(0.05f, 0.22f - danger * 0.012f);
        const float back = storm.a + PI, R = std::max(VW, VH) * 0.7f + 60;
        const float side = rr(-1, 1) * std::max(VW, VH) * 0.7f;
        const float x = ship.x + std::cos(back) * R + std::cos(storm.a + PI / 2) * side;
        const float y = ship.y + std::sin(back) * R + std::sin(storm.a + PI / 2) * side;
        const float v = rr(380, 560) + danger * 15, aa = storm.a + rr(-0.12f, 0.12f);
        newRock(x, y, rnd() < 0.35f ? 2 : 3, std::cos(aa) * v, std::sin(aa) * v).storm = true;
    }
    if (storm.t <= 0) {
        hasStorm = false;
        nextStorm = rr(45, 75) - std::min(25.f, danger * 2);
        banner = {"THE STORM PASSES", 2, C::sky, true};
    }
}

// ---------------------------------------------------------------- update
void updateFx(float dt) {
    for (int i = (int)parts.size() - 1; i >= 0; i--) {
        Part& p = parts[i];
        p.life -= dt;
        if (p.life <= 0) { parts.erase(parts.begin() + i); continue; }
        p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 1 - 1.8f * dt; p.vy *= 1 - 1.8f * dt;
    }
    if (parts.size() > 1600) parts.erase(parts.begin(), parts.begin() + (parts.size() - 1600));
    for (int j = (int)rings.size() - 1; j >= 0; j--) { rings[j].life -= dt; if (rings[j].life <= 0) rings.erase(rings.begin() + j); }
    for (int k = (int)texts.size() - 1; k >= 0; k--) { texts[k].life -= dt; texts[k].y -= 30 * dt; if (texts[k].life <= 0) texts.erase(texts.begin() + k); }
    shake = std::max(0.f, shake - 30 * dt);
    const float lx = ship.x + ship.vx * 0.3f, ly = ship.y + ship.vy * 0.3f;
    cam.x += (lx - cam.x) * std::min(1.f, 5 * dt); cam.y += (ly - cam.y) * std::min(1.f, 5 * dt);
}

void updateAliens(float dt) {
    const float spd = 1 + danger * 0.05f;
    for (int i = (int)aliens.size() - 1; i >= 0; i--) {
        Alien& al = aliens[i];
        const AlienKind& d = ALIEN.at(al.kind);
        al.t += dt; al.flash = std::max(0.f, al.flash - dt);
        if (al.emerge > 0) { al.emerge -= dt; al.x += al.vx * dt * 0.3f; al.y += al.vy * dt * 0.3f; continue; }
        const float dx = ship.x - al.x, dy = ship.y - al.y;
        float dd = std::sqrt(dx * dx + dy * dy);
        if (dd == 0) dd = 1;
        float ux = dx / dd, uy = dy / dd;
        float tx = 0, ty = 0, steer = 3;
        if (state != "playing") { ux = -ux; uy = -uy; }
        if (al.kind == "skitter") {                     // runs straight at you, zig-zagging
            const float w = std::sin(al.t * 9) * 90;
            tx = ux * d.speed * spd - uy * w; ty = uy * d.speed * spd + ux * w;
        } else if (al.kind == "spitter") {              // keeps its distance, circles, spits
            const float k = dd > 380 ? 1.f : dd < 250 ? -1.f : 0.f;
            tx = ux * d.speed * spd * k - uy * 110; ty = uy * d.speed * spd * k + ux * 110;
            al.fire -= dt;
            if (al.fire <= 0 && dd < 700 && state == "playing") {
                al.fire = std::max(0.8f, 2 - danger * 0.08f);
                const float lead = dd / 340, px = ship.x + ship.vx * lead * 0.6f, py = ship.y + ship.vy * lead * 0.6f;
                const float la = std::atan2(py - al.y, px - al.x);
                spits.push_back({al.x, al.y, std::cos(la) * 340, std::sin(la) * 340, 2.4f});
                Sfx::spit();
            }
        } else if (al.kind == "leech") {                // lumbers after you, then lunges
            al.dash -= dt;
            if (al.dashing > 0) {
                al.dashing -= dt; steer = 0.3f;
                if (al.dashing <= 0) al.dash = rr(2.2f, 3.6f);
            } else if (al.dash <= 0 && dd < 600) {
                al.dashing = 0.65f; al.vx = ux * 520 * spd; al.vy = uy * 520 * spd;
                burst(al.x, al.y, 10, 100, {C::acid}, 0.4f);
            }
            tx = ux * d.speed * spd; ty = uy * d.speed * spd;
            if (al.dash < 0.5f && al.dashing <= 0) { tx *= 0.1f; ty *= 0.1f; }   // the tell: it stops and swells before a lunge
        } else if (al.kind == "wraith") {               // fades out of reach, circles, then comes back solid and dashes
            al.phase -= dt;
            if (al.phase <= 0) {
                al.phased = !al.phased;
                al.phase = al.phased ? rr(1.6f, 2.4f) : rr(1.2f, 1.8f);
                if (!al.phased) { al.vx = ux * 460 * spd; al.vy = uy * 460 * spd; ring(al.x, al.y, 50, C::sky, 0.3f); }
            }
            if (al.phased) { tx = (ux * 0.6f - uy) * d.speed * spd * 1.4f; ty = (uy * 0.6f + ux) * d.speed * spd * 1.4f; }
            else { tx = ux * d.speed * spd; ty = uy * d.speed * spd; steer = 0.8f; }
        }
        al.vx += (tx - al.vx) * std::min(1.f, steer * dt); al.vy += (ty - al.vy) * std::min(1.f, steer * dt);
        al.x += al.vx * dt; al.y += al.vy * dt;
        al.a = std::atan2(al.vy, al.vx);
        if (!al.phased && state == "playing" && dd < al.r + 11) {
            const float ax = al.x, ay = al.y;
            al.vx = -ux * 300; al.vy = -uy * 300;
            if (al.kind == "leech") al.dashing = 0;
            hurt(d.bite, ax, ay);
        }
        if (dd > 4200) aliens.erase(aliens.begin() + i);   // lost track of you
    }
}

void update(float dt) {
    if (state == "over") { ship.dead += dt; updateFx(dt); return; }
    if (state != "playing") return;
    elapsed += dt;
    const float deep = depthAt(ship.x, ship.y);
    danger = 1 + elapsed / 55 + deep * 5;

    // the ship: rotation, thrust and inertia
    float turn = 0;
    bool thrust = false, brake = false;
    if (key("arrowleft") || key("a")) turn -= 1;
    if (key("arrowright") || key("d")) turn += 1;
    if (key("arrowup") || key("w")) thrust = true;
    if (key("arrowdown") || key("s")) brake = true;
    const float power = 1 + lv["engine"] * 0.2f;
    ship.a += turn * 4.4f * dt;
    if (thrust) {
        ship.vx += std::cos(ship.a) * 430 * power * dt;
        ship.vy += std::sin(ship.a) * 430 * power * dt;
        if (rnd() < 0.8f) {
            const float ba = ship.a + PI + rr(-0.3f, 0.3f);
            parts.push_back({ship.x - std::cos(ship.a) * 12, ship.y - std::sin(ship.a) * 12,
                             std::cos(ba) * rr(120, 260) + ship.vx, std::sin(ba) * rr(120, 260) + ship.vy,
                             0.35f, 0.35f, pick(std::vector<Color>{C::pink, C::pink, C::white, C::sky}), 2});
        }
    }
    if (thrust != ship.thrust) { ship.thrust = thrust; Sfx::thrust(thrust); }
    const float drag = brake ? 2.2f : 0.18f;
    ship.vx -= ship.vx * drag * dt; ship.vy -= ship.vy * drag * dt;
    const float sp = std::sqrt(ship.vx * ship.vx + ship.vy * ship.vy), top = 560 * (1 + lv["engine"] * 0.12f);
    if (sp > top) { ship.vx *= top / sp; ship.vy *= top / sp; }
    ship.x += ship.vx * dt; ship.y += ship.vy * dt;
    // the edge of the field pushes back
    const float rd = std::sqrt(ship.x * ship.x + ship.y * ship.y);
    if (rd > FIELD) {
        const float nx = ship.x / rd, ny = ship.y / rd, out = ship.vx * nx + ship.vy * ny;
        if (out > 0) { ship.vx -= nx * out * 1.6f; ship.vy -= ny * out * 1.6f; }
        ship.x = nx * FIELD; ship.y = ny * FIELD;
    }
    ship.inv = std::max(0.f, ship.inv - dt);
    ship.cool -= dt;
    if (key(" ") && ship.cool <= 0) fire();
    const float smax = lv["shield"] * 25.f;
    ship.regen -= dt;
    if (ship.regen <= 0 && ship.shield < smax) ship.shield = std::min(smax, ship.shield + 9 * dt);

    // rocks
    for (int i = (int)rocks.size() - 1; i >= 0; i--) {
        if (i >= (int)rocks.size()) continue;
        Rock& r = rocks[i];
        r.x += r.vx * dt; r.y += r.vy * dt; r.a += r.spin * dt; r.flash = std::max(0.f, r.flash - dt);
        if (r.storm && dist2(r.x, r.y, ship.x, ship.y) > 2600.f * 2600.f) { rocks.erase(rocks.begin() + i); continue; }
        const float rr2 = r.x * r.x + r.y * r.y;
        if (rr2 > FIELD * FIELD * 1.1f) {               // rocks drifting off the edge come back round
            if (r.storm) { rocks.erase(rocks.begin() + i); continue; }
            r.vx = -r.vx; r.vy = -r.vy;
        }
        const float dx = ship.x - r.x, dy = ship.y - r.y, rad = r.r * 0.85f + 11;
        if (state == "playing" && dx * dx + dy * dy < rad * rad) {
            float dd = std::sqrt(dx * dx + dy * dy);
            if (dd == 0) dd = 1;
            const float nx2 = dx / dd, ny2 = dy / dd;
            const float rel = (ship.vx - r.vx) * nx2 + (ship.vy - r.vy) * ny2;
            if (rel < 0) { ship.vx -= nx2 * rel * 1.5f; ship.vy -= ny2 * rel * 1.5f; }
            ship.x = r.x + nx2 * rad; ship.y = r.y + ny2 * rad;
            if (ship.inv <= 0) {
                const int id = r.id, size = r.size;
                hurt(ROCK::hurt[size] * (r.storm ? 1.3f : 1.f), r.x, r.y);
                if (size >= 2) {
                    for (int k = 0; k < (int)rocks.size(); k++) if (rocks[k].id == id) { breakRock(k, 0, 0); break; }
                }
            }
        }
    }

    // bullets
    for (int b = (int)bullets.size() - 1; b >= 0; b--) {
        Bullet& bu = bullets[b];
        bu.life -= dt;
        if (lv["homing"] > 0) {
            bu.seek -= dt;
            if (bu.seek <= 0) { bu.seek = 0.12f; bu.target = nearestTarget(bu); }
            float tx, ty;
            if (bu.target && findTarget(bu.target, tx, ty)) {
                const float cur = std::atan2(bu.vy, bu.vx), want2 = std::atan2(ty - bu.y, tx - bu.x);
                const float na = cur + clamp(angDiff(cur, want2), -1, 1) * (2.2f + lv["homing"] * 2.2f) * dt;
                const float bs = std::sqrt(bu.vx * bu.vx + bu.vy * bu.vy);
                bu.vx = std::cos(na) * bs; bu.vy = std::sin(na) * bs;
            }
        }
        bu.x += bu.vx * dt; bu.y += bu.vy * dt;
        bool gone = bu.life <= 0;
        for (int a = (int)aliens.size() - 1; a >= 0 && !gone; a--) {
            Alien& al = aliens[a];
            if (al.phased || has(bu.hits, al.id)) continue;
            if (dist2(bu.x, bu.y, al.x, al.y) < (al.r + 4) * (al.r + 4)) {
                al.hp--; al.flash = 0.1f; bu.hits.push_back(al.id);
                burst(bu.x, bu.y, 6, 140, {ALIEN.at(al.kind).col, C::white}, 0.3f, 2);
                if (al.hp <= 0) killAlien(a);
                if (bu.pierce-- <= 0) gone = true;
            }
        }
        for (int k = (int)rocks.size() - 1; k >= 0 && !gone; k--) {
            Rock& ro = rocks[k];
            if (std::fabs(ro.x - bu.x) > ro.r + 4 || std::fabs(ro.y - bu.y) > ro.r + 4 || has(bu.hits, ro.id)) continue;
            if (dist2(bu.x, bu.y, ro.x, ro.y) < (ro.r * 0.9f + 3) * (ro.r * 0.9f + 3)) {
                ro.hp--; ro.flash = 0.08f; bu.hits.push_back(ro.id);
                burst(bu.x, bu.y, 4, 120, {ro.iron ? C::blue : C::sky, C::white}, 0.3f, 1.8f);
                if (ro.hp <= 0) breakRock(k, bu.vx, bu.vy);
                if (bu.pierce-- <= 0) gone = true;
            }
        }
        if (gone) bullets.erase(bullets.begin() + b);
    }

    updateAliens(dt);

    // alien spit
    for (int s = (int)spits.size() - 1; s >= 0; s--) {
        Spit& sp2 = spits[s];
        sp2.x += sp2.vx * dt; sp2.y += sp2.vy * dt; sp2.life -= dt;
        if (dist2(sp2.x, sp2.y, ship.x, ship.y) < 15 * 15) {
            const float x = sp2.x, y = sp2.y;
            spits.erase(spits.begin() + s);
            hurt(10, x, y); burst(x, y, 10, 120, {C::pink, C::acid}, 0.4f);
            continue;
        }
        if (sp2.life <= 0) spits.erase(spits.begin() + s);
    }

    // salvage: drifts, slows, and is pulled in when close
    for (int g = (int)salvage.size() - 1; g >= 0; g--) {
        Salvage& sv = salvage[g];
        sv.life -= dt; sv.a += dt * 3;
        const float gx = ship.x - sv.x, gy = ship.y - sv.y;
        float gd = std::sqrt(gx * gx + gy * gy);
        if (gd == 0) gd = 1;
        if (gd < 170) { sv.vx += gx / gd * 1600 * dt; sv.vy += gy / gd * 1600 * dt; }
        sv.vx -= sv.vx * 1.5f * dt; sv.vy -= sv.vy * 1.5f * dt;
        sv.x += sv.vx * dt; sv.y += sv.vy * dt;
        if (gd < 20) {
            salv += sv.val; score += sv.val * 5;
            floatText(sv.x, sv.y, "+" + std::to_string(sv.val), C::acid);
            Sfx::pick();
            salvage.erase(salvage.begin() + g);
            continue;
        }
        if (sv.life <= 0) salvage.erase(salvage.begin() + g);
    }

    populate(dt);
    updateStorm(dt);
    updateFx(dt);
    if (banner.has && banner.t > 0) banner.t -= dt;
}

// ---------------------------------------------------------------- drawing
void glow(const Color& col, float blur) { ctx->shadowColor = col; ctx->shadowBlur = blur; }
void noGlow() { ctx->shadowBlur = 0; }

void drawBack(double now) {
    Canvas& c = *ctx;
    c.fillStyle = C::black;
    c.fillRect(0, 0, W, H);
    if (!started_) { cam.x += 0.25f; cam.y += 0.08f; }
    // nebulae, far behind: soft clouds of the Cursed blues and pink
    for (const Nebula& nb : nebulae) {
        const float sx = (nb.x - cam.x) * 0.35f + W / 2, sy = (nb.y - cam.y) * 0.35f + H / 2, r = nb.r * 0.6f;
        if (sx < -r || sx > W + r || sy < -r || sy > H + r) continue;
        const Color inner = nb.c == C::pink ? Color("rgba(240,136,233,0.10)") : nb.c == C::navy ? Color("rgba(0,23,37,0.5)") : Color("rgba(0,122,183,0.13)");
        c.radialDisc(sx, sy, r, inner, Color("rgba(2,6,17,0)"));
    }
    for (size_t l = 0; l < stars.size(); l++) {
        const float f = 0.1f + l * 0.22f, ox = -cam.x * f, oy = -cam.y * f;
        c.fillStyle = l == 2 ? C::white : l == 1 ? Color("#a9d8f0") : Color("#6f8fa8");
        for (const StarP& s : stars[l]) {
            const float x = std::fmod(std::fmod(s.x + ox, 1024.f) + 1024, 1024.f), y = std::fmod(std::fmod(s.y + oy, 1024.f) + 1024, 1024.f);
            c.globalAlpha = 0.45f + 0.55f * std::fabs(std::sin(s.t + (float)(now / 1400)));
            for (float tx = x; tx < W; tx += 1024) for (float ty = y; ty < H; ty += 1024) c.fillRect(tx, ty, s.r, s.r);
        }
    }
    c.globalAlpha = 1;
}

bool onScreen(float x, float y, float r) {
    return x + r > cam.x - VW / 2 - 40 && x - r < cam.x + VW / 2 + 40 && y + r > cam.y - VH / 2 - 40 && y - r < cam.y + VH / 2 + 40;
}

void drawRock(const Rock& r, double now) {
    Canvas& c = *ctx;
    c.save();
    c.translate(r.x, r.y);
    if (!r.nest.empty()) { const float tw = (float)std::sin(now / 90 + r.wob) * (std::sin(now / 900 + r.wob) > 0.7 ? 2.f : 0.f); c.translate(tw, 0); }
    c.rotate(r.a);
    c.beginPath();
    for (size_t i = 0; i < r.pts.size(); i += 2) {
        const float x = std::cos(r.pts[i]) * r.pts[i + 1], y = std::sin(r.pts[i]) * r.pts[i + 1];
        if (i == 0) c.moveTo(x, y); else c.lineTo(x, y);
    }
    c.closePath();
    c.fillStyle = r.iron ? Color("#001a2e") : C::navy;
    c.fill();
    const Color col = r.flash > 0 ? C::white : r.iron ? C::blue : r.storm ? C::pink : C::sky;
    glow(col, r.size == 3 ? 6.f : 14.f);
    c.strokeStyle = col;
    c.lineWidth = r.size == 0 ? 2.4f : r.size == 3 ? 1.4f : 2.f;
    c.stroke();
    noGlow();
    c.globalAlpha = 0.35f;
    c.lineWidth = 1;
    c.beginPath();
    for (size_t k = 0; k + 3 < r.cracks.size(); k += 4) {
        c.moveTo(std::cos(r.cracks[k]) * r.cracks[k + 1], std::sin(r.cracks[k]) * r.cracks[k + 1]);
        c.lineTo(std::cos(r.cracks[k + 2]) * r.cracks[k + 3], std::sin(r.cracks[k + 2]) * r.cracks[k + 3]);
    }
    c.stroke();
    c.globalAlpha = 1;
    if (!r.nest.empty()) {                              // a nest: acid veins pulsing faintly under the surface
        const float p = 0.25f + 0.35f * std::max(0.f, (float)std::sin(now / 420 + r.wob));
        c.globalAlpha = p;
        glow(C::acid, 10);
        c.strokeStyle = C::acid;
        c.lineWidth = 1.3f;
        c.beginPath();
        for (size_t v = 0; v + 3 < r.cracks.size(); v += 4) {
            c.moveTo(0, 0);
            c.quadraticCurveTo(std::cos(r.cracks[v]) * r.cracks[v + 1], std::sin(r.cracks[v]) * r.cracks[v + 1],
                               std::cos(r.cracks[v + 2]) * r.cracks[v + 3], std::sin(r.cracks[v + 2]) * r.cracks[v + 3]);
        }
        c.stroke();
        c.fillStyle = C::acid;
        c.beginPath(); c.arc(0, 0, r.r * 0.09f, 0, TAU); c.fill();
        noGlow();
        c.globalAlpha = 1;
    }
    c.restore();
}

void drawAlien(const Alien& al, double now) {
    Canvas& c = *ctx;
    const AlienKind& d = ALIEN.at(al.kind);
    const Color col = al.flash > 0 ? C::white : d.col;
    const float k = al.emerge > 0 ? 1 - al.emerge / al.emergeMax : 1;
    if (k <= 0) return;
    c.save();
    c.translate(al.x, al.y);
    c.globalAlpha = al.phased ? 0.18f + 0.1f * (float)std::sin(now / 80) : 1.f;
    c.scale(k, k);
    glow(col, 16);
    c.strokeStyle = col; c.fillStyle = col; c.lineWidth = 2;
    const float t = al.t, r = al.r;
    if (al.kind == "skitter") {                         // a tick: a body and six twitching legs
        c.rotate(al.a);
        c.beginPath();
        for (int i = 0; i < 6; i++) {
            const float side = i < 3 ? 1.f : -1.f, j = (float)(i % 3), ba = (j - 1) * 0.7f, tw = std::sin(t * 22 + i) * 0.35f;
            c.moveTo(std::cos(ba) * r * 0.4f, side * r * 0.4f);
            c.lineTo(std::cos(ba + tw) * r * 1.3f, side * (r * 1.1f + std::sin(ba + tw) * 4));
        }
        c.stroke();
        c.beginPath(); c.ellipse(0, 0, r * 0.85f, r * 0.6f, 0, 0, TAU); c.fillStyle = C::black; c.fill(); c.stroke();
        c.fillStyle = C::pink; c.beginPath(); c.arc(r * 0.5f, -3, 2, 0, TAU); c.arc(r * 0.5f, 3, 2, 0, TAU); c.fill();
    } else if (al.kind == "spitter") {                  // a floating eye with a frill
        c.beginPath();
        for (int f = 0; f < 14; f++) {
            const float fa = f / 14.f * TAU + t * 0.6f, fr = r * (1.25f + 0.2f * std::sin(t * 6 + f));
            c.moveTo(std::cos(fa) * r * 0.9f, std::sin(fa) * r * 0.9f); c.lineTo(std::cos(fa) * fr, std::sin(fa) * fr);
        }
        c.stroke();
        c.beginPath(); c.arc(0, 0, r, 0, TAU); c.fillStyle = C::black; c.fill(); c.stroke();
        const float ea = std::atan2(ship.y - al.y, ship.x - al.x);
        c.fillStyle = C::white; c.beginPath(); c.arc(std::cos(ea) * r * 0.35f, std::sin(ea) * r * 0.35f, r * 0.45f, 0, TAU); c.fill();
        c.fillStyle = C::black; c.beginPath(); c.ellipse(std::cos(ea) * r * 0.5f, std::sin(ea) * r * 0.5f, r * 0.1f, r * 0.3f, ea, 0, TAU); c.fill();
    } else if (al.kind == "leech") {                    // a fat, ribbed slug with tendrils
        c.rotate(al.a);
        const float swell = al.dash < 0.5f && al.dashing <= 0 ? 1.18f : 1.f;
        c.beginPath();
        for (int tn = 0; tn < 5; tn++) {
            const float ty = (tn - 2) * r * 0.3f;
            c.moveTo(-r * 0.8f, ty);
            c.quadraticCurveTo(-r * 1.4f, ty + std::sin(t * 5 + tn) * 10, -r * 1.9f, ty + std::sin(t * 4 + tn * 2) * 14);
        }
        c.stroke();
        c.beginPath(); c.ellipse(0, 0, r * swell, r * 0.7f * swell, 0, 0, TAU); c.fillStyle = C::black; c.fill(); c.stroke();
        c.beginPath();
        for (int rb = -2; rb <= 2; rb++) { c.moveTo(rb * r * 0.3f, -r * 0.6f * swell); c.quadraticCurveTo(rb * r * 0.3f + 5, 0, rb * r * 0.3f, r * 0.6f * swell); }
        c.globalAlpha *= 0.6f; c.stroke(); c.globalAlpha = al.phased ? 0.2f : 1.f;
        c.fillStyle = C::pink; c.beginPath(); c.arc(r * 0.85f, 0, r * 0.28f, 0, TAU); c.fill();
        c.fillStyle = C::black; c.beginPath(); c.arc(r * 0.9f, 0, r * 0.14f, 0, TAU); c.fill();
    } else {                                            // the wraith: a hooded shape trailing ribbons
        c.rotate(al.a + PI / 2);
        c.beginPath();
        c.moveTo(0, -r * 1.2f);
        c.quadraticCurveTo(r * 1.1f, -r * 0.4f, r * 0.8f, r * 0.9f);
        for (int wv = 0; wv < 5; wv++) c.lineTo(r * (0.6f - wv * 0.35f), r * (1.2f + std::sin(t * 10 + wv) * 0.35f + (wv % 2) * 0.4f));
        c.quadraticCurveTo(-r * 1.1f, -r * 0.4f, 0, -r * 1.2f);
        c.fillStyle = C::black; c.fill(); c.stroke();
        c.fillStyle = C::white;
        c.beginPath(); c.arc(-r * 0.3f, -r * 0.35f, 2.5f, 0, TAU); c.arc(r * 0.3f, -r * 0.35f, 2.5f, 0, TAU); c.fill();
    }
    noGlow();
    c.restore();
}

void drawShip(double now) {
    Canvas& c = *ctx;
    if (state == "over") return;
    c.save();
    c.translate(ship.x, ship.y);
    if (lv["shield"] > 0 && ship.shield > 0.5f) {
        c.globalAlpha = 0.25f + 0.5f * ship.shield / (lv["shield"] * 25.f);
        glow(C::sky, 14); c.strokeStyle = C::sky; c.lineWidth = 1.5f;
        c.beginPath(); c.arc(0, 0, 24 + (float)std::sin(now / 150) * 1.5f, 0, TAU); c.stroke();
        c.globalAlpha = 1;
    }
    c.rotate(ship.a);
    if (ship.inv > 0 && (long long)std::floor(now / 70) % 2) c.globalAlpha = 0.4f;
    if (ship.thrust) {
        glow(C::pink, 20); c.fillStyle = C::pink;
        c.beginPath(); c.moveTo(-9, -5); c.lineTo(-18 - rnd() * 12, 0); c.lineTo(-9, 5); c.fill();
    }
    glow(C::pink, 18);
    c.beginPath();
    c.moveTo(17, 0); c.lineTo(-11, -11); c.lineTo(-6, 0); c.lineTo(-11, 11); c.closePath();
    c.fillStyle = C::black; c.fill();
    c.strokeStyle = C::pink; c.lineWidth = 2.4f; c.stroke();
    c.fillStyle = C::white; c.beginPath(); c.arc(4, 0, 2.5f, 0, TAU); c.fill();
    noGlow();
    c.restore();
    c.globalAlpha = 1;
}

void drawWorld(double now) {
    Canvas& c = *ctx;
    const float sx = shake ? (rnd() - 0.5f) * shake : 0, sy = shake ? (rnd() - 0.5f) * shake : 0;
    c.save();
    c.translate(std::round(W / 2 + sx), std::round(H / 2 + sy));
    c.scale(Z, Z);
    c.translate(-cam.x, -cam.y);

    // the beacon at the heart of the field, and the edge of the field
    if (onScreen(0, 0, 200)) {
        const float bp = 0.5f + 0.5f * (float)std::sin(now / 500);
        glow(C::acid, 20); c.strokeStyle = C::acid; c.lineWidth = 2;
        c.beginPath(); c.arc(0, 0, 30 + bp * 6, 0, TAU); c.stroke();
        c.globalAlpha = 0.3f; c.beginPath(); c.arc(0, 0, 120 + bp * 40, 0, TAU); c.stroke(); c.globalAlpha = 1;
        noGlow();
    }
    const float rd = std::sqrt(cam.x * cam.x + cam.y * cam.y);
    if (rd > FIELD - std::max(VW, VH)) {
        glow(C::pink, 16); c.strokeStyle = C::pink; c.lineWidth = 3; c.setLineDash({24, 18});
        c.beginPath(); c.arc(0, 0, FIELD, 0, TAU); c.stroke(); c.setLineDash({}); noGlow();
    }

    for (const Salvage& sv : salvage) {
        if (!onScreen(sv.x, sv.y, 10)) continue;
        c.save(); c.translate(sv.x, sv.y); c.rotate(sv.a);
        c.globalAlpha = sv.life < 4 ? ((long long)std::floor(now / 120) % 2 ? 0.3f : 1.f) : 1.f;
        glow(C::acid, 12); c.fillStyle = C::acid;
        c.beginPath(); c.moveTo(0, -6); c.lineTo(5, 0); c.lineTo(0, 6); c.lineTo(-5, 0); c.fill();
        c.restore();
    }
    noGlow();
    for (const Rock& r : rocks) if (onScreen(r.x, r.y, r.r)) drawRock(r, now);
    for (const Alien& a : aliens) if (onScreen(a.x, a.y, 60)) drawAlien(a, now);

    glow(C::pink, 12); c.fillStyle = C::pink;
    for (size_t s = 0; s < spits.size(); s++) {
        c.beginPath(); c.arc(spits[s].x, spits[s].y, 5 + (float)std::sin(now / 50 + s) * 1.5f, 0, TAU); c.fill();
    }
    glow(C::white, 10); c.strokeStyle = C::white; c.lineWidth = 2.5f; c.lineCap = Cap::Round;
    c.beginPath();
    for (const Bullet& bu : bullets) {
        float bl = std::sqrt(bu.vx * bu.vx + bu.vy * bu.vy);
        if (bl == 0) bl = 1;
        c.moveTo(bu.x, bu.y); c.lineTo(bu.x - bu.vx / bl * 12, bu.y - bu.vy / bl * 12);
    }
    c.stroke();
    if (lv["pierce"] > 0 || lv["homing"] > 0) {
        c.strokeStyle = lv["homing"] > 0 ? C::acid : C::sky; c.lineWidth = 1; glow(c.strokeStyle, 8);
        c.beginPath();
        for (const Bullet& u : bullets) { c.moveTo(u.x + 4, u.y); c.arc(u.x, u.y, 4, 0, TAU); }
        c.stroke();
    }
    noGlow();
    drawShip(now);

    c.lighter(true);
    for (const Part& pt : parts) {
        const float k = pt.life / pt.max;
        c.globalAlpha = k; c.fillStyle = pt.c;
        c.fillRect(pt.x - pt.s * k, pt.y - pt.s * k, pt.s * 2 * k + 0.5f, pt.s * 2 * k + 0.5f);
    }
    c.lighter(false);
    for (const Ring& ri : rings) {
        const float kk = ri.life / ri.max;
        c.globalAlpha = kk; glow(ri.c, 14); c.strokeStyle = ri.c; c.lineWidth = 3 * kk + 0.5f;
        c.beginPath(); c.arc(ri.x, ri.y, ri.r * (1 - kk * 0.8f), 0, TAU); c.stroke();
    }
    noGlow();
    c.font(Font::Bold, 14); c.textAlign = Align::Center;
    for (const Text& t : texts) {
        c.globalAlpha = std::min(1.f, t.life * 2); c.fillStyle = t.c;
        c.fillText(t.t, t.x, t.y);
    }
    c.globalAlpha = 1;
    c.restore();

    if (hasStorm && storm.warn <= 0) {                  // storm streaks across the screen
        c.save();
        c.strokeStyle = C::pink; c.lineWidth = 1;
        c.globalAlpha = 0.18f;
        const float ca = std::cos(storm.a), sa = std::sin(storm.a);
        c.beginPath();
        for (int st = 0; st < 40; st++) {
            const float px = std::fmod(std::fmod(st * 397 + (float)now * 1.1f * ca, W) + W, W), py = std::fmod(std::fmod(st * 211 + (float)now * 1.1f * sa, H) + H, H);
            c.moveTo(px, py); c.lineTo(px - ca * 60, py - sa * 60);
        }
        c.stroke();
        c.restore();
    }
}

void bar(float x, float y, float w, float h, float k, const Color& col, const std::string& label) {
    Canvas& c = *ctx;
    c.strokeStyle = col; c.lineWidth = 1.5f; c.strokeRect(x, y, w, h);
    glow(col, 10); c.fillStyle = col; c.fillRect(x + 2, y + 2, std::max(0.f, (w - 4) * k), h - 4); noGlow();
    c.fillStyle = C::white; c.font(Font::Bold, 11); c.textAlign = Align::Left;
    c.fillText(label, x, y - 5);
}

void drawHud(double now) {
    Canvas& c = *ctx;
    const bool small = W < 600;
    c.textAlign = Align::Left; c.font(Font::Bold, small ? 16.f : 22.f);
    glow(C::pink, 10); c.fillStyle = C::pink;
    c.fillText("SCORE " + std::to_string(score), 14, 30);
    noGlow();
    c.font(Font::Regular, small ? 12.f : 14.f); c.fillStyle = Color("#c2c2c2");
    c.fillText("BEST " + std::to_string(std::max(best, score)), 14, 50);
    glow(C::acid, 10); c.fillStyle = C::acid; c.font(Font::Bold, small ? 14.f : 18.f);
    c.fillText("SALVAGE " + std::to_string(salv) + (small ? "" : "   U: SALVAGE BAY"), 14, 74);
    noGlow();
    c.textAlign = Align::Center; c.font(Font::Bold, small ? 14.f : 18.f);
    c.fillStyle = danger > 5 ? C::pink : C::sky;
    c.fillText("DEPTH " + fixed1(danger), W / 2, 28);
    if (!small) {
        c.textAlign = Align::Right; c.font(Font::Regular, 13); c.fillStyle = Color("#c2c2c2");
        c.fillText(std::string("Arrows/WASD fly \xC2\xB7 Space fire \xC2\xB7 U upgrades \xC2\xB7 P pause \xC2\xB7 M ") + (Sfx::isMuted() ? "unmute" : "mute") +
                       " \xC2\xB7 F11 fullscreen", W - 14, 24);
    }
    // hull and shield
    const float bw = small ? 120.f : 180.f, by = H - (small ? 190.f : 46.f);
    bar(14, by, bw, 12, ship.hull / 100, C::pink, "HULL");
    if (lv["shield"] > 0) bar(24 + bw, by, bw * 0.7f, 12, ship.shield / (lv["shield"] * 25.f), C::sky, "SHIELD");
    // the minimap
    const float ms = small ? 110.f : 170.f, mx = W - ms - 14, my = small ? H - ms - 200 : H - ms - 14, mr = ms / 2, sc = mr / FIELD;
    const float cx = mx + mr, cy = my + mr;
    c.fillStyle = Color("rgba(0,23,37,0.75)");
    c.beginPath(); c.arc(cx, cy, mr, 0, TAU); c.fill();
    c.strokeStyle = C::deep; c.lineWidth = 1;
    c.beginPath(); c.arc(cx, cy, mr * 0.33f, 0, TAU); c.stroke();
    c.beginPath(); c.arc(cx, cy, mr * 0.66f, 0, TAU); c.stroke();
    c.strokeStyle = C::pink; c.beginPath(); c.arc(cx, cy, mr, 0, TAU); c.stroke();
    c.fillStyle = Color("#6f8fa8");
    for (const Rock& r : rocks) {
        if (r.size > 1) continue;
        c.fillRect(cx + r.x * sc - 0.6f, cy + r.y * sc - 0.6f, r.size ? 1.f : 1.6f, r.size ? 1.f : 1.6f);
    }
    c.fillStyle = C::acid;
    for (const Alien& a : aliens) c.fillRect(cx + a.x * sc - 1.5f, cy + a.y * sc - 1.5f, 3, 3);
    c.fillRect(cx - 1.5f, cy - 1.5f, 3, 3);
    // the view, and you
    c.strokeStyle = C::sky; c.globalAlpha = 0.6f;
    c.strokeRect(cx + (cam.x - VW / 2) * sc, cy + (cam.y - VH / 2) * sc, VW * sc, VH * sc);
    c.globalAlpha = 1;
    c.save(); c.translate(cx + ship.x * sc, cy + ship.y * sc); c.rotate(ship.a);
    glow(C::pink, 8); c.fillStyle = C::pink;
    c.beginPath(); c.moveTo(6, 0); c.lineTo(-4, -4); c.lineTo(-4, 4); c.fill(); noGlow();
    c.restore();

    // threat arrows: aliens hunting you from off screen
    for (const Alien& al : aliens) {
        const float dx = al.x - cam.x, dy = al.y - cam.y;
        if (std::fabs(dx) < VW / 2 && std::fabs(dy) < VH / 2) continue;
        const float ang = std::atan2(dy, dx);
        const float ex = clamp(W / 2 + std::cos(ang) * (W / 2 - 30), 20, W - 20), ey = clamp(H / 2 + std::sin(ang) * (H / 2 - 30), 90, H - 20);
        c.save(); c.translate(ex, ey); c.rotate(ang);
        c.fillStyle = ALIEN.at(al.kind).col; c.globalAlpha = 0.5f + 0.5f * (float)std::sin(now / 120);
        c.beginPath(); c.moveTo(10, 0); c.lineTo(-6, -7); c.lineTo(-6, 7); c.fill();
        c.restore();
    }
    c.globalAlpha = 1;

    if (banner.has && banner.t > 0) {
        const Color bc = banner.c.a > 0 ? banner.c : C::sky;
        c.globalAlpha = std::min(1.f, banner.t);
        c.textAlign = Align::Center; c.font(Font::Black, std::min(34.f, W / 16));
        const float bwid = c.measureText(banner.text);
        if (bwid > W - 24) c.font(Font::Black, std::floor(std::min(34.f, W / 16) * (W - 24) / bwid));
        glow(bc, 18); c.fillStyle = bc;
        c.fillText(banner.text, W / 2, H * 0.28f);
        noGlow(); c.globalAlpha = 1;
    }
}

void drawOverlay() {
    Canvas& c = *ctx;
    if (state != "paused" && !(state == "over" && ship.dead > 1)) return;
    c.fillStyle = Color("rgba(2, 6, 17, 0.6)");
    c.fillRect(0, 0, W, H);
    c.textAlign = Align::Center;
    glow(C::pink, 24); c.fillStyle = C::pink;
    c.font(Font::Black, std::min(72.f, W / 14));
    c.fillText(state == "over" ? "LOST IN THE ROCKS" : "PAUSED", W / 2, H / 2 - 20);
    noGlow();
    c.fillStyle = C::white; c.font(Font::Regular, W < 600 ? 14.f : 20.f);
    if (state == "over") {
        c.fillText("Score " + std::to_string(score) + "  \xC2\xB7  Depth " + fixed1(danger) +
                       (newBest ? "  \xC2\xB7  NEW BEST!" : "  \xC2\xB7  Best " + std::to_string(best)), W / 2, H / 2 + 24);
        c.fillStyle = C::sky; c.fillText("Space, R or click to fly again", W / 2, H / 2 + 56);
    } else {
        c.fillText("P to carry on", W / 2, H / 2 + 24);
    }
}

// ---------------------------------------------------------------- the salvage bay (an HTML panel in the browser)
int costOf(const Upgrade& u) { const int l = lv[u.id]; return u.repeat ? u.cost[0] : l < (int)u.cost.size() ? u.cost[l] : -1; }

struct MenuLayout { float x, y, w, h, rowY[7], rowH, closeX, closeY, closeW, closeH; };
MenuLayout menuLayout() {
    MenuLayout m;
    m.w = std::min(520.f, W * 0.92f);
    m.rowH = 51;
    const float inner = 28 + 4 + 21 + 12 + 7 * (m.rowH + 6) + 10 + 15 + 8 + 33;
    m.h = std::min(H * 0.9f, inner + 36);
    m.x = W / 2 - m.w / 2; m.y = H / 2 - m.h / 2;
    float y = m.y + 18 + 28 + 4 + 21 + 12;
    for (int i = 0; i < 7; i++) { m.rowY[i] = y; y += m.rowH + 6; }
    y += 10 + 15 + 8;
    ctx->font(Font::Bold, 15); ctx->letterSpacing = 1.5f;
    m.closeW = ctx->measureText("FLY ON") + 56; ctx->letterSpacing = 0;
    m.closeH = 33; m.closeX = W / 2 - m.closeW / 2; m.closeY = y;
    return m;
}

void drawMenu() {
    Canvas& c = *ctx;
    if (state != "menu") return;
    const MenuLayout m = menuLayout();
    c.save();
    c.fillStyle = C::navy; c.fillRect(m.x, m.y, m.w, m.h);
    c.strokeStyle = C::pink; c.lineWidth = 2; c.strokeRect(m.x + 1, m.y + 1, m.w - 2, m.h - 2);
    c.textAlign = Align::Center; c.textBaseline = Baseline::Middle;
    c.font(Font::Black, 24); c.letterSpacing = 2.4f; c.fillStyle = C::pink;
    c.fillText("SALVAGE BAY", W / 2 + 1.2f, m.y + 18 + 14);
    c.letterSpacing = 0;
    c.font(Font::Bold, 18); c.fillStyle = C::acid;
    c.fillText("Salvage " + std::to_string(salv), W / 2, m.y + 18 + 32 + 10);
    for (int i = 0; i < 7; i++) {
        const Upgrade& u = UPGRADES[i];
        const int cost = costOf(u);
        const bool maxed = cost < 0, full = std::string(u.id) == "repair" && ship.hull >= 100;
        const bool disabled = maxed || full || salv < cost;
        const float bx = m.x + 22, bw = m.w - 44, by = m.rowY[i], bh = m.rowH;
        const bool hot = hover == i && !disabled;
        c.fillStyle = hot ? C::deep : C::black; c.fillRect(bx, by, bw, bh);
        c.strokeStyle = hot ? C::sky : C::deep; c.lineWidth = 1; c.strokeRect(bx + 0.5f, by + 0.5f, bw - 1, bh - 1);
        const Color text = disabled ? Color("#6f8797") : C::white;
        c.textAlign = Align::Left;
        c.font(Font::Bold, 15); c.fillStyle = disabled ? text : C::pink;
        c.fillText(std::to_string(i + 1), bx + 13, by + bh / 2);
        c.font(Font::Regular, 15); c.fillStyle = text;
        c.fillText(u.name, bx + 13 + 18 + 10, by + 18);
        c.font(Font::Regular, 12); c.fillStyle = disabled ? text : C::sky;
        c.fillText(u.desc, bx + 13 + 18 + 10, by + 35);
        std::string pips;
        if (!u.repeat) for (size_t p = 0; p < u.cost.size(); p++) pips += (int)p < lv[u.id] ? "\xE2\x97\x86" : "\xE2\x97\x87";
        c.textAlign = Align::Right;
        c.font(Font::Regular, 15); c.fillStyle = disabled ? text : C::acid;
        const std::string cs = maxed ? "MAX" : full ? "FULL" : std::to_string(cost);
        c.fillText(cs, bx + bw - 12, by + bh / 2);
        c.letterSpacing = 2;
        c.fillText(pips, bx + bw - 12 - 52 - 10, by + bh / 2);
        c.letterSpacing = 0;
    }
    c.textAlign = Align::Center;
    c.font(Font::Regular, 13); c.fillStyle = Color("#c2c2c2");
    c.fillText("1-7 or click to buy \xC2\xB7 U or Esc to fly on", W / 2, m.rowY[6] + m.rowH + 6 + 10 + 7);
    c.fillStyle = C::pink; c.fillRect(m.closeX, m.closeY, m.closeW, m.closeH);
    c.font(Font::Bold, 15); c.letterSpacing = 1.5f; c.fillStyle = C::black;
    c.fillText("FLY ON", W / 2 + 0.75f, m.closeY + m.closeH / 2);
    c.restore();
}

void buy(int i) {
    const Upgrade& u = UPGRADES[i];
    const int cost = costOf(u);
    if (cost < 0 || salv < cost || (std::string(u.id) == "repair" && ship.hull >= 100)) { Sfx::deny(); return; }
    salv -= cost;
    if (std::string(u.id) == "repair") ship.hull = std::min(100.f, ship.hull + 40);
    else { lv[u.id]++; if (std::string(u.id) == "shield") ship.shield = lv["shield"] * 25.f; }
    Sfx::buy();
}

void openMenu() {
    if (state != "playing") return;
    state = "menu";
    keys.clear();
    Sfx::thrust(false);
}

void drawTheSplash(double nowMs) {
    if (splashFade <= 0) return;
    const float vw = W / 100;
    const float tpx = std::min(96.f, 13 * vw);
    SplashItem title; title.px = tpx; title.lineH = 1.15f;
    SplashItem a; a.text = "ROCKS "; a.font = Font::Black; a.px = tpx; a.spacing = tpx * 0.06f; a.color = C::sky; a.stroke = C::pink; a.strokeW = 2;
    SplashItem o; o.text = "OF "; o.font = Font::Black; o.px = tpx * 0.5f; o.spacing = tpx * 0.03f; o.color = C::pink;
    SplashItem b = a; b.text = "RUIN";
    title.inline_ = {a, o, b};
    SplashItem tag; tag.text = "AN ARCADE PHANTASM GAME"; tag.font = Font::Bold; tag.px = 18; tag.spacing = 3.6f; tag.color = C::acid; tag.marginBottom = 10;
    SplashItem img; img.image = &logo; img.imageW = std::min(260.f, 50 * vw);
    SplashItem copy; copy.text = "\xC2\xA9 Cursed Entertainment 2026"; copy.px = 15; copy.color = Color("#c2c2c2");
    SplashItem press; press.text = "Press any key or click to start"; press.font = Font::Bold; press.px = 20; press.spacing = 1.6f;
    press.color = C::pink; press.marginTop = 18;
    const float ph = (float)std::fmod(nowMs / 1200.0, 2.0), k = ph < 1 ? ph : 2 - ph;   // 1.2 s ease-in-out, alternating
    press.alpha = 1 - 0.7f * (0.5f - 0.5f * std::cos(k * PI));
    SplashItem keysLine; keysLine.text = "Arrows / WASD fly \xC2\xB7 Space fire \xC2\xB7 U upgrades \xC2\xB7 P pause \xC2\xB7 M mute";
    keysLine.px = 13; keysLine.color = C::sky;
    drawSplash(*ctx, {title, tag, img, copy, press, keysLine}, 14, Color("rgba(2, 6, 17, 0.55)"), splashFade);
}

void draw(double now) {
    drawBack(now);
    if (started_ && haveShip) {
        drawWorld(now);
        drawHud(now);
        drawOverlay();
        drawMenu();
    }
    drawTheSplash(now);
}

void start() {
    if (started_) return;
    started_ = true;
    reset();
}

void togglePause() {
    if (state == "playing") { state = "paused"; Sfx::thrust(false); }
    else if (state == "paused") state = "playing";
}

std::string lower(std::string k) { for (char& ch : k) ch = (char)std::tolower((unsigned char)ch); return k; }
}  // namespace

// ---- what main.cpp calls ----------------------------------------------------------------------------------------
const char* Game::title() { return "ROCKS OF RUIN"; }
bool Game::started() { return started_; }

void Game::init(float w, float h) {
    best = std::atoi(Platform::load("rocksOfRuinBest", "0").c_str());
    Sfx::muted = Platform::load("rocksOfRuinMuted", "0") == "1";
    logo = Platform::texture("cursed-logo.png");
    resizeView(w, h);
    makeStars();
}

void Game::resize(float w, float h) { resizeView(w, h); }

void Game::frame(Canvas& c, double now) {
    ctx = &c;
    const float dt = last ? std::min(0.033f, (float)((now - last) / 1000)) : 0.016f;
    last = now;
    if (started_) update(dt);
    if (started_) splashFade = std::max(0.f, splashFade - dt / 0.5f);
    draw(now);
}

void Game::keyDown(const std::string& key0, bool) {
    Sfx::wake();
    const std::string k = lower(key0);
    if (!started_) { start(); return; }
    if (state == "menu") {
        if (k.size() == 1 && k[0] >= '1' && k[0] <= '7') buy(k[0] - '1');
        else if (k == "u" || k == "escape" || k == "enter") closeMenu();
        else if (k == "m") Sfx::toggleMute();
        return;
    }
    if (k == "p") togglePause();
    else if (k == "m") Sfx::toggleMute();
    else if (k == "u" || k == "tab") openMenu();
    else if ((k == " " || k == "r") && state == "over" && ship.dead > 1) reset();
    else if (k == "escape" && state == "paused") togglePause();
    keys[k] = true;
}

void Game::keyUp(const std::string& k) { keys[lower(k)] = false; }

void Game::pointerDown(float x, float y) {
    Sfx::wake();
    if (!started_) { start(); return; }
    if (state == "over" && ship.dead > 1) { reset(); return; }
    if (state == "menu" && ctx) {
        const MenuLayout m = menuLayout();
        for (int i = 0; i < 7; i++)
            if (x >= m.x + 22 && x <= m.x + m.w - 22 && y >= m.rowY[i] && y <= m.rowY[i] + m.rowH) { buy(i); return; }
        if (x >= m.closeX && x <= m.closeX + m.closeW && y >= m.closeY && y <= m.closeY + m.closeH) closeMenu();
    }
}

void Game::pointerMove(float x, float y) {
    hover = -1;
    if (state != "menu" || !ctx) return;
    const MenuLayout m = menuLayout();
    for (int i = 0; i < 7; i++)
        if (x >= m.x + 22 && x <= m.x + m.w - 22 && y >= m.rowY[i] && y <= m.rowY[i] + m.rowH) hover = i;
}
void Game::pointerUp(float, float) {}

void Game::blur() {
    keys.clear();
    if (state == "playing") togglePause();
}
