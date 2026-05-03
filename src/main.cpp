#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#error "This sample uses the platform OpenGL 3.3 headers shipped with macOS."
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int WorldSize = 72;
constexpr int WorldHeight = WorldSize;
constexpr int FaceRelief = 42;
constexpr int FaceBase = 8;
constexpr float PlayerHeight = 1.72f;
constexpr float PlayerRadius = 0.32f;
constexpr float Pi = 3.1415926535f;
constexpr float WalkSpeed = 5.4f;
constexpr float SprintSpeed = 8.2f;
constexpr float GroundAcceleration = 36.0f;
constexpr float AirAcceleration = 12.0f;
constexpr float GroundFriction = 18.0f;
constexpr float Gravity = 24.0f;
constexpr float JumpSpeed = 8.4f;
constexpr float CoyoteTime = 0.11f;
constexpr float JumpBufferTime = 0.13f;
constexpr float NormalBuildTime = 0.28f;
constexpr float HardBuildTime = 0.62f;
constexpr std::size_t MaxUiVertices = 100000;
constexpr const char* GameTitle = "InterPlanetary 3D";
constexpr const char* GameTitleCaps = "INTERPLANETARY 3D";

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    float m[16]{};
};

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float length(Vec3 v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(Vec3 v) {
    const float len = length(v);
    return len > 0.00001f ? v * (1.0f / len) : Vec3{};
}

Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 translation(Vec3 v) {
    Mat4 r = identity();
    r.m[12] = v.x;
    r.m[13] = v.y;
    r.m[14] = v.z;
    return r;
}

Mat4 transformFromBasis(Vec3 position, Vec3 forward, Vec3 up) {
    forward = normalize(forward);
    Vec3 right = normalize(cross(up, forward));
    if (length(right) < 0.001f) right = {1.0f, 0.0f, 0.0f};
    up = normalize(cross(forward, right));
    Mat4 r = identity();
    r.m[0] = right.x;
    r.m[1] = right.y;
    r.m[2] = right.z;
    r.m[4] = up.x;
    r.m[5] = up.y;
    r.m[6] = up.z;
    r.m[8] = forward.x;
    r.m[9] = forward.y;
    r.m[10] = forward.z;
    r.m[12] = position.x;
    r.m[13] = position.y;
    r.m[14] = position.z;
    return r;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

Mat4 perspective(float fovyRadians, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    Mat4 r{};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return r;
}

Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r = identity();
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

enum class Block : std::uint8_t {
    Air,
    Ash,
    BloodSoil,
    Basalt,
    PaleStone,
    DeadCrystal,
    Bone,
    ObsidianGlass,
    Fuel,
    Plutonium,
    Sand,
    Sandstone,
    DesertRock,
    Grass,
    Dirt,
    Wood,
    Leaves,
    Concrete,
    DarkConcrete,
    Asphalt,
    WindowGlass
};

enum class BuildType {
    Normal,
    Hard
};

enum class WorldTheme {
    Dead,
    Desert,
    Lush,
    Brutalist
};

enum class SatelliteOrbit {
    CurrentFace,
    OppositeFace,
    Polar
};

enum class ToolMode {
    MineBuild,
    Missile,
    AtomicMissile
};

enum class GameType {
    FirstToThree,
    PhasedTurns,
    AlternatingHunter
};

enum class P2Control {
    Keyboard,
    Gamepad
};

enum class GameScreen {
    Title,
    Playing
};

struct MatchState {
    GameType type = GameType::FirstToThree;
    double startedAt = 0.0;
    bool suddenDeath = false;
    bool over = false;
    int winner = 0;
    int firstHunter = 1;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    float r;
    float g;
    float b;
    float a;
    float kind;
};

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
};

struct UiVertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

struct SatelliteCamera {
    GLuint framebuffer = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int size = 0;
    int satelliteSize = 45;
    int missileSize = 240;
};

struct VoxelUniforms {
    GLuint program = 0;
    GLint mvp = -1;
    GLint model = -1;
    GLint camera = -1;
    GLint headlampDir = -1;
    GLint sunDir = -1;
    GLint fogColor = -1;
    GLint time = -1;
    GLint feedClarity = -1;
};

struct LineUniforms {
    GLuint program = 0;
    GLint mvp = -1;
    GLint color = -1;
};

struct TextureUniforms {
    GLuint program = 0;
    GLint rect = -1;
    GLint texture = -1;
    GLint time = -1;
};

struct MissileFeedUniforms {
    GLuint program = 0;
    GLint rect = -1;
    GLint texture = -1;
};

struct ForcefieldUniforms {
    GLuint program = 0;
    GLint mvp = -1;
    GLint time = -1;
    GLint feedClarity = -1;
};

struct SatelliteView {
    Vec3 position{};
    Vec3 target{};
    Vec3 down{};
    Vec3 up{};
    Mat4 vp{};
};

struct World {
    std::vector<Block> blocks;
    int seed = 0;
    WorldTheme theme = WorldTheme::Dead;

    World() : blocks(WorldSize * WorldHeight * WorldSize, Block::Air) {}

    int index(int x, int y, int z) const {
        return (y * WorldSize + z) * WorldSize + x;
    }

    bool inBounds(int x, int y, int z) const {
        return x >= 0 && x < WorldSize && y >= 0 && y < WorldHeight && z >= 0 && z < WorldSize;
    }

    Block get(int x, int y, int z) const {
        if (!inBounds(x, y, z)) return Block::Air;
        return blocks[index(x, y, z)];
    }

    void set(int x, int y, int z, Block block) {
        if (inBounds(x, y, z)) blocks[index(x, y, z)] = block;
    }
};

struct Player {
    Vec3 position{WorldSize / 2.0f, WorldHeight + 4.0f, WorldSize / 2.0f};
    Vec3 velocity{};
    float upSign = 1.0f;
    float yaw = 0.0f;
    float pitch = -0.12f;
    float lookVelocityX = 0.0f;
    float lookVelocityY = 0.0f;
    bool grounded = false;
    bool jumpWasDown = false;
    float coyoteTimer = 0.0f;
    float jumpBufferTimer = 0.0f;
    int health = 100;
    int fuel = 3;
    int plutonium = 3;
    int score = 0;
    float rocketCooldown = 0.0f;
    float hurtTimer = 0.0f;
};

struct Rocket {
    bool active = false;
    int owner = 1;
    bool atomic = false;
    float age = 0.0f;
    Vec3 position{};
    Vec3 direction{0.0f, 0.0f, 1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct Blast {
    bool active = false;
    bool atomic = false;
    float age = 0.0f;
    Vec3 position{};
};

struct Cell {
    int x = 0;
    int y = 0;
    int z = 0;
};

GLFWwindow* window = nullptr;
bool mouseCaptured = false;
bool firstMouse = true;
Vec2 lastMouse{};
Vec2 pendingMouseLook{};

int randomPlanetSeed() {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::random_device rd;
    return static_cast<int>((static_cast<std::uint64_t>(now) ^ (static_cast<std::uint64_t>(rd()) << 1)) & 0x7fffffff);
}

float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float hash2(int x, int z, int seed) {
    std::uint32_t n = static_cast<std::uint32_t>(x * 374761393 + z * 668265263 + seed * 1442695041);
    n = (n ^ (n >> 13)) * 1274126177u;
    return static_cast<float>(n ^ (n >> 16)) / static_cast<float>(0xffffffffu);
}

float valueNoise(float x, float z, int seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float tx = fade(x - static_cast<float>(x0));
    const float tz = fade(z - static_cast<float>(z0));
    const float a = hash2(x0, z0, seed);
    const float b = hash2(x0 + 1, z0, seed);
    const float c = hash2(x0, z0 + 1, seed);
    const float d = hash2(x0 + 1, z0 + 1, seed);
    return std::lerp(std::lerp(a, b, tx), std::lerp(c, d, tx), tz);
}

float fractalNoise(float x, float z, int seed) {
    float total = 0.0f;
    float amplitude = 0.55f;
    float frequency = 1.0f;
    float norm = 0.0f;
    for (int octave = 0; octave < 5; ++octave) {
        total += valueNoise(x * frequency, z * frequency, seed + octave * 97) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return total / norm;
}

bool solid(Block block) {
    return block != Block::Air && block != Block::DeadCrystal && block != Block::ObsidianGlass;
}

bool transparent(Block block) {
    return block == Block::Air || block == Block::DeadCrystal || block == Block::ObsidianGlass || block == Block::WindowGlass;
}

bool spawnSurface(Block block) {
    return solid(block) && block != Block::Wood;
}

float blockKind(Block block) {
    switch (block) {
        case Block::Ash: return 1.0f;
        case Block::BloodSoil: return 2.0f;
        case Block::Basalt: return 3.0f;
        case Block::PaleStone: return 4.0f;
        case Block::DeadCrystal: return 7.0f;
        case Block::Bone: return 8.0f;
        case Block::ObsidianGlass: return 9.0f;
        case Block::Fuel: return 10.0f;
        case Block::Plutonium: return 11.0f;
        case Block::Sand: return 12.0f;
        case Block::Sandstone: return 13.0f;
        case Block::DesertRock: return 14.0f;
        case Block::Grass: return 15.0f;
        case Block::Dirt: return 16.0f;
        case Block::Wood: return 17.0f;
        case Block::Leaves: return 18.0f;
        case Block::Concrete: return 19.0f;
        case Block::DarkConcrete: return 20.0f;
        case Block::Asphalt: return 21.0f;
        case Block::WindowGlass: return 22.0f;
        case Block::Air: break;
    }
    return 0.0f;
}

std::array<float, 4> colorOf(Block block, int x, int y, int z) {
    float speckle = hash2(x + y * 17, z - y * 9, 181) * 0.07f - 0.035f;
    std::array<float, 4> c{};
    switch (block) {
        case Block::Ash: c = {0.18f, 0.175f, 0.17f, 1.0f}; break;
        case Block::BloodSoil: c = {0.17f, 0.145f, 0.135f, 1.0f}; break;
        case Block::Basalt: c = {0.055f, 0.055f, 0.060f, 1.0f}; break;
        case Block::PaleStone: c = {0.39f, 0.38f, 0.35f, 1.0f}; break;
        case Block::DeadCrystal: c = {0.20f, 0.22f, 0.23f, 0.62f}; speckle = 0.0f; break;
        case Block::Bone: c = {0.58f, 0.56f, 0.49f, 1.0f}; break;
        case Block::ObsidianGlass: c = {0.050f, 0.052f, 0.060f, 0.55f}; speckle = 0.0f; break;
        case Block::Fuel: c = {0.03f, 0.45f, 1.0f, 1.0f}; speckle = 0.0f; break;
        case Block::Plutonium: c = {0.88f, 1.0f, 0.05f, 1.0f}; speckle = 0.0f; break;
        case Block::Sand: c = {0.64f, 0.52f, 0.32f, 1.0f}; break;
        case Block::Sandstone: c = {0.48f, 0.38f, 0.22f, 1.0f}; break;
        case Block::DesertRock: c = {0.28f, 0.22f, 0.15f, 1.0f}; break;
        case Block::Grass: c = {0.13f, 0.46f, 0.12f, 1.0f}; break;
        case Block::Dirt: c = {0.25f, 0.17f, 0.10f, 1.0f}; break;
        case Block::Wood: c = {0.34f, 0.20f, 0.10f, 1.0f}; break;
        case Block::Leaves: c = {0.08f, 0.42f, 0.10f, 1.0f}; speckle = 0.0f; break;
        case Block::Concrete: c = {0.44f, 0.44f, 0.41f, 1.0f}; break;
        case Block::DarkConcrete: c = {0.18f, 0.18f, 0.17f, 1.0f}; break;
        case Block::Asphalt: c = {0.055f, 0.055f, 0.052f, 1.0f}; break;
        case Block::WindowGlass: c = {0.12f, 0.17f, 0.19f, 0.58f}; speckle = 0.0f; break;
        case Block::Air: c = {0.0f, 0.0f, 0.0f, 0.0f}; break;
    }
    c[0] = std::clamp(c[0] + speckle, 0.0f, 1.0f);
    c[1] = std::clamp(c[1] + speckle, 0.0f, 1.0f);
    c[2] = std::clamp(c[2] + speckle, 0.0f, 1.0f);
    return c;
}

Block blockForBuildType(BuildType type, WorldTheme theme) {
    if (theme == WorldTheme::Desert) return type == BuildType::Normal ? Block::Sand : Block::DesertRock;
    if (theme == WorldTheme::Lush) return type == BuildType::Normal ? Block::Dirt : Block::Wood;
    if (theme == WorldTheme::Brutalist) return type == BuildType::Normal ? Block::Concrete : Block::DarkConcrete;
    return type == BuildType::Normal ? Block::Ash : Block::Basalt;
}

const char* buildTypeName(BuildType type) {
    return type == BuildType::Normal ? "normal" : "hard";
}

const char* themeName(WorldTheme theme) {
    switch (theme) {
        case WorldTheme::Dead: return "dead";
        case WorldTheme::Desert: return "desert";
        case WorldTheme::Lush: return "lush";
        case WorldTheme::Brutalist: return "brutalist city";
    }
    return "dead";
}

WorldTheme nextTheme(WorldTheme theme) {
    switch (theme) {
        case WorldTheme::Dead: return WorldTheme::Desert;
        case WorldTheme::Desert: return WorldTheme::Lush;
        case WorldTheme::Lush: return WorldTheme::Brutalist;
        case WorldTheme::Brutalist: return WorldTheme::Dead;
    }
    return WorldTheme::Dead;
}

WorldTheme previousTheme(WorldTheme theme) {
    switch (theme) {
        case WorldTheme::Dead: return WorldTheme::Brutalist;
        case WorldTheme::Desert: return WorldTheme::Dead;
        case WorldTheme::Lush: return WorldTheme::Desert;
        case WorldTheme::Brutalist: return WorldTheme::Lush;
    }
    return WorldTheme::Dead;
}

int themeShaderValue(WorldTheme theme) {
    switch (theme) {
        case WorldTheme::Dead: return 0;
        case WorldTheme::Desert: return 1;
        case WorldTheme::Lush: return 2;
        case WorldTheme::Brutalist: return 3;
    }
    return 0;
}

const char* orbitName(SatelliteOrbit orbit) {
    switch (orbit) {
        case SatelliteOrbit::CurrentFace: return "current face";
        case SatelliteOrbit::OppositeFace: return "opposite face";
        case SatelliteOrbit::Polar: return "polar";
    }
    return "current face";
}

SatelliteOrbit nextOrbit(SatelliteOrbit orbit) {
    switch (orbit) {
        case SatelliteOrbit::CurrentFace: return SatelliteOrbit::OppositeFace;
        case SatelliteOrbit::OppositeFace: return SatelliteOrbit::Polar;
        case SatelliteOrbit::Polar: return SatelliteOrbit::CurrentFace;
    }
    return SatelliteOrbit::CurrentFace;
}

ToolMode nextToolMode(ToolMode mode) {
    switch (mode) {
        case ToolMode::MineBuild: return ToolMode::Missile;
        case ToolMode::Missile: return ToolMode::AtomicMissile;
        case ToolMode::AtomicMissile: return ToolMode::MineBuild;
    }
    return ToolMode::MineBuild;
}

const char* toolModeName(ToolMode mode) {
    switch (mode) {
        case ToolMode::MineBuild: return "mine/build";
        case ToolMode::Missile: return "missile";
        case ToolMode::AtomicMissile: return "atomic";
    }
    return "mine/build";
}

GameType nextGameType(GameType type) {
    switch (type) {
        case GameType::FirstToThree: return GameType::PhasedTurns;
        case GameType::PhasedTurns: return GameType::AlternatingHunter;
        case GameType::AlternatingHunter: return GameType::FirstToThree;
    }
    return GameType::FirstToThree;
}

const char* gameTypeName(GameType type) {
    switch (type) {
        case GameType::FirstToThree: return "first to 3";
        case GameType::PhasedTurns: return "10-turn phases";
        case GameType::AlternatingHunter: return "alternating hunter";
    }
    return "first to 3";
}

GameType previousGameType(GameType type) {
    switch (type) {
        case GameType::FirstToThree: return GameType::AlternatingHunter;
        case GameType::PhasedTurns: return GameType::FirstToThree;
        case GameType::AlternatingHunter: return GameType::PhasedTurns;
    }
    return GameType::FirstToThree;
}

const char* p2ControlName(P2Control control) {
    return control == P2Control::Keyboard ? "keys" : "gamepad";
}

const char* turnPhaseName(const MatchState& match, double now) {
    if (match.over) return match.winner == 1 ? "P1 wins" : "P2 wins";
    if (match.suddenDeath) return "sudden death";
    if (match.type == GameType::PhasedTurns) {
        const int turn = std::clamp(static_cast<int>((now - match.startedAt) / 60.0), 0, 9);
        return (turn % 2) == 0 ? "build/mine" : "attack/hide";
    }
    if (match.type == GameType::AlternatingHunter) {
        const int turn = std::max(0, static_cast<int>((now - match.startedAt) / 60.0));
        const int active = (turn % 2) == 0 ? match.firstHunter : 3 - match.firstHunter;
        return active == 1 ? "P1 hunts" : "P2 hunts";
    }
    return "duel";
}

float buildDuration(BuildType type) {
    return type == BuildType::Normal ? NormalBuildTime : HardBuildTime;
}

float mineDuration(Block block) {
    constexpr float MiningDifficulty = 2.0f;
    float duration = 0.5f;
    switch (block) {
        case Block::Ash: duration = 0.42f; break;
        case Block::BloodSoil: duration = 0.50f; break;
        case Block::Basalt: duration = 1.15f; break;
        case Block::PaleStone: duration = 0.85f; break;
        case Block::DeadCrystal: duration = 0.70f; break;
        case Block::Bone: duration = 0.62f; break;
        case Block::ObsidianGlass: duration = 1.35f; break;
        case Block::Fuel: duration = 1.05f; break;
        case Block::Plutonium: duration = 1.45f; break;
        case Block::Sand: duration = 0.38f; break;
        case Block::Sandstone: duration = 0.78f; break;
        case Block::DesertRock: duration = 1.10f; break;
        case Block::Grass: duration = 0.35f; break;
        case Block::Dirt: duration = 0.45f; break;
        case Block::Wood: duration = 0.95f; break;
        case Block::Leaves: duration = 0.32f; break;
        case Block::Concrete: duration = 0.95f; break;
        case Block::DarkConcrete: duration = 1.20f; break;
        case Block::Asphalt: duration = 0.70f; break;
        case Block::WindowGlass: duration = 0.55f; break;
        case Block::Air: break;
    }
    return duration * MiningDifficulty;
}

bool sameBlockCell(Vec3 a, Vec3 b) {
    return static_cast<int>(a.x) == static_cast<int>(b.x)
        && static_cast<int>(a.y) == static_cast<int>(b.y)
        && static_cast<int>(a.z) == static_cast<int>(b.z);
}

int faceProfile(int face) {
    (void)face;
    return 0;
}

int mirrorSourceFace(int face) {
    return face == 1 ? 0 : face;
}

int faceTerrainHeight(int u, int v, int face, int seed, WorldTheme theme) {
    face = mirrorSourceFace(face);
    const int profile = faceProfile(face);
    const float nx = static_cast<float>(u) / 22.0f;
    const float nz = static_cast<float>(v) / 22.0f;
    if (theme == WorldTheme::Desert) {
        const float broad = fractalNoise(nx * 0.82f + 11.0f, nz * 0.82f - 5.0f, seed + profile * 503 + 1201);
        const float dunes = std::abs(fractalNoise(nx * 2.8f - 17.0f, nz * 1.55f + 23.0f, seed + profile * 503 + 1229) - 0.5f) * 2.0f;
        const float mesas = fractalNoise(nx * 1.35f + 41.0f, nz * 1.35f - 31.0f, seed + profile * 503 + 1291);
        const float windScars = fractalNoise(nx * 7.5f + 3.0f, nz * 2.2f - 9.0f, seed + profile * 503 + 1327);
        int height = 5 + static_cast<int>(broad * 14.0f + dunes * 12.0f);
        if (mesas > 0.72f) height += 9;
        if (windScars > 0.72f) height += 3;
        if (windScars < 0.20f) height -= 3;
        height = (height / 2) * 2;
        return std::clamp(height, 2, FaceRelief - 2);
    }
    if (theme == WorldTheme::Lush) {
        const float rolling = fractalNoise(nx * 0.80f - 6.0f, nz * 0.80f + 14.0f, seed + profile * 503 + 2201);
        const float hills = fractalNoise(nx * 1.80f + 27.0f, nz * 1.80f - 19.0f, seed + profile * 503 + 2237);
        const float ridges = std::abs(fractalNoise(nx * 3.1f - 11.0f, nz * 3.1f + 5.0f, seed + profile * 503 + 2293) - 0.5f) * 2.0f;
        int height = 8 + static_cast<int>(rolling * 15.0f + hills * 11.0f + ridges * 4.0f);
        height = (height / 2) * 2;
        return std::clamp(height, 5, FaceRelief - 5);
    }
    if (theme == WorldTheme::Brutalist) {
        const float slab = fractalNoise(nx * 0.55f + 8.0f, nz * 0.55f - 12.0f, seed + profile * 503 + 4201);
        const float broken = fractalNoise(nx * 3.5f - 21.0f, nz * 3.5f + 9.0f, seed + profile * 503 + 4219);
        int height = 8 + static_cast<int>(slab * 5.0f + broken * 3.0f);
        height = (height / 2) * 2;
        return std::clamp(height, 5, 18);
    }
    const float continent = fractalNoise(nx, nz, seed + profile * 503);
    const float ridges = std::abs(fractalNoise(nx * 2.45f + 32.0f, nz * 2.45f - 8.0f, seed + profile * 503 + 7) - 0.5f) * 2.0f;
    const float pits = fractalNoise(nx * 3.0f - 11.0f, nz * 3.0f + 19.0f, seed + profile * 503 + 31);
    const float crags = std::abs(fractalNoise(nx * 5.0f - 17.0f, nz * 5.0f + 29.0f, seed + profile * 503 + 83) - 0.5f) * 2.0f;
    const float scars = fractalNoise(nx * 9.0f + 3.0f, nz * 9.0f - 6.0f, seed + profile * 503 + 151);
    int height = 2 + static_cast<int>(continent * 18.0f + ridges * 18.0f + crags * 10.0f - pits * 9.0f);
    if (scars > 0.68f) height += 5;
    if (scars < 0.24f) height -= 4;
    height = (height / 2) * 2;
    return std::clamp(height, 2, FaceRelief);
}

Block cubeFaceMaterial(int depth, int height, int u, int v, int face, int seed, WorldTheme theme) {
    face = mirrorSourceFace(face);
    const int profile = faceProfile(face);
    const float vein = fractalNoise((u + depth * 3) / 7.0f, (v - depth * 2) / 7.0f, seed + profile * 503 + 99);
    const float resourceA = fractalNoise((u + profile * 19) / 5.5f, (v + depth * 2) / 5.5f, seed + profile * 701 + 211);
    const float resourceB = fractalNoise((u - depth * 3) / 6.5f, (v + profile * 23) / 6.5f, seed + profile * 701 + 409);
    if (depth <= 2) {
        const float surfaceFuel = fractalNoise(u / 6.2f + 13.0f, v / 6.2f - 9.0f, seed + profile * 839 + 17);
        const float surfacePlutonium = fractalNoise(u / 7.0f - 21.0f, v / 7.0f + 5.0f, seed + profile * 839 + 71);
        const float fleck = hash2(u + depth * 31, v - depth * 19, seed + profile * 1181);
        if (surfacePlutonium > 0.855f && fleck > 0.62f) return Block::Plutonium;
        if (surfaceFuel > 0.875f && fleck > 0.68f) return Block::Fuel;
    }
    if (depth >= 5 && depth <= 11) {
        const float fleck = hash2(u + depth * 13, v - depth * 7, seed + profile * 997);
        if (resourceB > 0.845f && fleck > 0.68f) return Block::Plutonium;
        if (resourceA > 0.855f && fleck > 0.72f) return Block::Fuel;
    }
    if (theme == WorldTheme::Desert) {
        const float strata = fractalNoise((u + depth) / 9.0f, (v - depth) / 9.0f, seed + profile * 503 + 1601);
        if (depth == 0) {
            if (height > FaceRelief - 12 && strata > 0.46f) return Block::DesertRock;
            return Block::Sand;
        }
        if (depth < 5) return strata > 0.64f ? Block::Sandstone : Block::Sand;
        return vein > 0.76f ? Block::DesertRock : Block::Sandstone;
    }
    if (theme == WorldTheme::Lush) {
        const float loam = fractalNoise((u + depth * 2) / 8.0f, (v - depth) / 8.0f, seed + profile * 503 + 2601);
        if (depth == 0) return Block::Grass;
        if (depth < 5) return Block::Dirt;
        return loam > 0.72f ? Block::PaleStone : Block::Dirt;
    }
    if (theme == WorldTheme::Brutalist) {
        if (depth == 0) {
            const bool roadX = (u % 16) < 3;
            const bool roadV = (v % 16) < 3;
            if (roadX || roadV) return Block::Asphalt;
            return Block::Concrete;
        }
        if (depth < 4) return Block::Concrete;
        return vein > 0.70f ? Block::DarkConcrete : Block::Concrete;
    }
    if (depth == 0) {
        if (height < 10) return hash2(u, v, seed + profile * 17 + 91) > 0.68f ? Block::ObsidianGlass : Block::Ash;
        if (height > FaceRelief - 9) return Block::Bone;
        return hash2(u, v, seed + profile * 17 + 144) > 0.66f ? Block::Ash : Block::BloodSoil;
    }
    if (depth < 4) return height < 10 ? Block::Ash : Block::BloodSoil;
    return vein > 0.80f ? Block::PaleStone : Block::Basalt;
}

void setCubeFaceColumn(World& world, int face, int u, int v, int height) {
    const int lowSurface = FaceBase + FaceRelief - height;
    const int highSurface = WorldSize - 1 - lowSurface;
    const int shell = 12;
    switch (face) {
        case 0:
            {
                const int surface = highSurface;
                for (int y = surface + 1; y < WorldHeight; ++y) world.set(u, y, v, Block::Air);
                for (int y = std::max(0, surface - shell); y <= surface; ++y) world.set(u, y, v, cubeFaceMaterial(surface - y, height, u, v, face, world.seed, world.theme));
            }
            break;
        case 1:
            {
                const int surface = lowSurface;
                for (int y = 0; y < surface; ++y) world.set(u, y, v, Block::Air);
                for (int y = surface; y <= std::min(WorldHeight - 1, surface + shell); ++y) world.set(u, y, v, cubeFaceMaterial(y - surface, height, u, v, face, world.seed, world.theme));
            }
            break;
        case 2:
            {
                const int surface = highSurface;
                for (int x = surface + 1; x < WorldSize; ++x) world.set(x, u, v, Block::Air);
                for (int x = std::max(0, surface - shell); x <= surface; ++x) world.set(x, u, v, cubeFaceMaterial(surface - x, height, u, v, face, world.seed, world.theme));
            }
            break;
        case 3:
            {
                const int surface = lowSurface;
                for (int x = 0; x < surface; ++x) world.set(x, u, v, Block::Air);
                for (int x = surface; x <= std::min(WorldSize - 1, surface + shell); ++x) world.set(x, u, v, cubeFaceMaterial(x - surface, height, u, v, face, world.seed, world.theme));
            }
            break;
        case 4:
            {
                const int surface = highSurface;
                for (int z = surface + 1; z < WorldSize; ++z) world.set(u, v, z, Block::Air);
                for (int z = std::max(0, surface - shell); z <= surface; ++z) world.set(u, v, z, cubeFaceMaterial(surface - z, height, u, v, face, world.seed, world.theme));
            }
            break;
        case 5:
            {
                const int surface = lowSurface;
                for (int z = 0; z < surface; ++z) world.set(u, v, z, Block::Air);
                for (int z = surface; z <= std::min(WorldSize - 1, surface + shell); ++z) world.set(u, v, z, cubeFaceMaterial(z - surface, height, u, v, face, world.seed, world.theme));
            }
            break;
    }
}

Cell addCell(Cell a, Cell b, int scale = 1) {
    return {a.x + b.x * scale, a.y + b.y * scale, a.z + b.z * scale};
}

bool faceBasis(int face, Cell& normal, Cell& tangentU, Cell& tangentV) {
    switch (face) {
        case 0: normal = {0, 1, 0}; tangentU = {1, 0, 0}; tangentV = {0, 0, 1}; return true;
        case 1: normal = {0, -1, 0}; tangentU = {1, 0, 0}; tangentV = {0, 0, 1}; return true;
        case 2: normal = {1, 0, 0}; tangentU = {0, 1, 0}; tangentV = {0, 0, 1}; return true;
        case 3: normal = {-1, 0, 0}; tangentU = {0, 1, 0}; tangentV = {0, 0, 1}; return true;
        case 4: normal = {0, 0, 1}; tangentU = {1, 0, 0}; tangentV = {0, 1, 0}; return true;
        case 5: normal = {0, 0, -1}; tangentU = {1, 0, 0}; tangentV = {0, 1, 0}; return true;
    }
    return false;
}

bool faceSurface(const World& world, int face, int u, int v, Cell& surface, Cell& normal, Cell& tangentU, Cell& tangentV) {
    if (!faceBasis(face, normal, tangentU, tangentV)) return false;
    switch (face) {
        case 0:
            for (int y = WorldHeight - 1; y >= 0; --y) if (solid(world.get(u, y, v))) { surface = {u, y, v}; return true; }
            break;
        case 1:
            for (int y = 0; y < WorldHeight; ++y) if (solid(world.get(u, y, v))) { surface = {u, y, v}; return true; }
            break;
        case 2:
            for (int x = WorldSize - 1; x >= 0; --x) if (solid(world.get(x, u, v))) { surface = {x, u, v}; return true; }
            break;
        case 3:
            for (int x = 0; x < WorldSize; ++x) if (solid(world.get(x, u, v))) { surface = {x, u, v}; return true; }
            break;
        case 4:
            for (int z = WorldSize - 1; z >= 0; --z) if (solid(world.get(u, v, z))) { surface = {u, v, z}; return true; }
            break;
        case 5:
            for (int z = 0; z < WorldSize; ++z) if (solid(world.get(u, v, z))) { surface = {u, v, z}; return true; }
            break;
    }
    return false;
}

void setCell(World& world, Cell cell, Block block) {
    if (world.inBounds(cell.x, cell.y, cell.z)) world.set(cell.x, cell.y, cell.z, block);
}

void addLushTree(World& world, int face, int u, int v) {
    Cell surface{}, normal{}, tangentU{}, tangentV{};
    if (!faceSurface(world, face, u, v, surface, normal, tangentU, tangentV)) return;
    const int featureFace = face == 1 ? 0 : face;
    const int trunkHeight = 3 + static_cast<int>(hash2(u, v, world.seed + featureFace * 97 + 3301) * 2.0f);
    for (int i = 1; i <= trunkHeight; ++i) {
        setCell(world, addCell(surface, normal, i), Block::Wood);
    }
    const Cell crown = addCell(surface, normal, trunkHeight + 1);
    for (int dn = -1; dn <= 1; ++dn) {
        const int spread = dn == 1 ? 1 : 2;
        for (int dv = -spread; dv <= spread; ++dv) {
            for (int du = -spread; du <= spread; ++du) {
                if (std::abs(du) + std::abs(dv) + std::max(0, dn) > 3) continue;
                Cell leaf = addCell(addCell(addCell(crown, normal, dn), tangentU, du), tangentV, dv);
                if (world.inBounds(leaf.x, leaf.y, leaf.z) && world.get(leaf.x, leaf.y, leaf.z) == Block::Air) {
                    world.set(leaf.x, leaf.y, leaf.z, Block::Leaves);
                }
            }
        }
    }
}

void addLushFeatures(World& world) {
    constexpr std::array<int, 6> faces{{0, 1, 2, 3, 4, 5}};
    for (int face : faces) {
        const int featureFace = face == 1 ? 0 : face;
        for (int v = 8; v < WorldSize - 8; v += 11) {
            for (int u = 8; u < WorldSize - 8; u += 11) {
                const float grove = hash2(u + featureFace * 43, v - featureFace * 29, world.seed + 3203);
                if (grove > 0.42f) addLushTree(world, face, u, v);
            }
        }
    }
}

void addBrutalistTower(World& world, int face, int centerU, int centerV) {
    Cell surface{}, normal{}, tangentU{}, tangentV{};
    if (!faceSurface(world, face, centerU, centerV, surface, normal, tangentU, tangentV)) return;
    const int featureFace = face == 1 ? 0 : face;
    const int widthU = 3 + static_cast<int>(hash2(centerU, centerV, world.seed + featureFace * 71 + 5101) * 3.0f);
    const int widthV = 3 + static_cast<int>(hash2(centerU, centerV, world.seed + featureFace * 73 + 5107) * 3.0f);
    const int height = 10 + static_cast<int>(hash2(centerU, centerV, world.seed + featureFace * 79 + 5113) * 22.0f);
    for (int dv = -widthV; dv <= widthV; ++dv) {
        for (int du = -widthU; du <= widthU; ++du) {
            const bool edge = std::abs(du) == widthU || std::abs(dv) == widthV;
            for (int h = 1; h <= height; ++h) {
                Cell cell = addCell(addCell(addCell(surface, normal, h), tangentU, du), tangentV, dv);
                if (!world.inBounds(cell.x, cell.y, cell.z)) continue;
                const bool windowBand = edge && h > 2 && (h % 3 == 1) && ((du + dv + featureFace) & 1) == 0;
                world.set(cell.x, cell.y, cell.z, windowBand ? Block::WindowGlass : (edge ? Block::Concrete : Block::DarkConcrete));
            }
        }
    }
}

void addBrutalistFeatures(World& world) {
    constexpr std::array<int, 6> faces{{0, 1, 2, 3, 4, 5}};
    for (int face : faces) {
        const int featureFace = face == 1 ? 0 : face;
        for (int v = 10; v < WorldSize - 10; v += 14) {
            for (int u = 10; u < WorldSize - 10; u += 14) {
                const float towerChance = hash2(u + featureFace * 61, v - featureFace * 37, world.seed + 5201);
                if (towerChance > 0.48f) addBrutalistTower(world, face, u, v);
            }
        }
    }
}

void mirrorP2HalfFromP1Half(World& world) {
    for (int y = 0; y < WorldHeight / 2; ++y) {
        const int sourceY = WorldHeight - 1 - y;
        for (int z = 0; z < WorldSize; ++z) {
            for (int x = 0; x < WorldSize; ++x) {
                world.set(x, y, z, world.get(x, sourceY, z));
            }
        }
    }
}

void generateWorld(World& world) {
    std::fill(world.blocks.begin(), world.blocks.end(), Block::Air);

    constexpr std::array<int, 6> faceOrder{{2, 3, 4, 5, 0, 1}};
    for (int face : faceOrder) {
        for (int v = 0; v < WorldSize; ++v) {
            for (int u = 0; u < WorldSize; ++u) {
                setCubeFaceColumn(world, face, u, v, faceTerrainHeight(u, v, face, world.seed, world.theme));
            }
        }
    }

    if (world.theme == WorldTheme::Desert) {
        mirrorP2HalfFromP1Half(world);
        return;
    }
    if (world.theme == WorldTheme::Lush) {
        addLushFeatures(world);
        mirrorP2HalfFromP1Half(world);
        return;
    }
    if (world.theme == WorldTheme::Brutalist) {
        addBrutalistFeatures(world);
        mirrorP2HalfFromP1Half(world);
        return;
    }

    for (int z = 4; z < WorldSize - 4; ++z) {
        for (int x = 4; x < WorldSize - 4; ++x) {
            int y = WorldHeight - 1;
            while (y > 1 && world.get(x, y, z) == Block::Air) --y;
            const Block surface = world.get(x, y, z);
            Block decoration = Block::Air;
            if ((surface == Block::Ash || surface == Block::ObsidianGlass) && hash2(x, z, world.seed + 70) > 0.996f && world.inBounds(x, y + 1, z)) {
                decoration = Block::DeadCrystal;
            } else if ((surface == Block::Ash || surface == Block::BloodSoil) && hash2(x, z, world.seed + 64) > 0.988f && world.inBounds(x, y + 1, z)) {
                decoration = Block::Bone;
            }
            if (decoration != Block::Air) {
                world.set(x, y + 1, z, decoration);
                int bottomY = 0;
                while (bottomY < WorldHeight - 2 && world.get(x, bottomY, z) == Block::Air) ++bottomY;
                if (world.inBounds(x, bottomY - 1, z)) world.set(x, bottomY - 1, z, decoration);
            }
        }
    }
    mirrorP2HalfFromP1Half(world);
}

Vec3 spawnPosition(const World& world) {
    constexpr int margin = 4;
    int bestX = WorldSize / 2;
    int bestY = -1;
    int bestZ = WorldSize / 2;
    int bestCenterDistance = WorldSize * WorldSize;
    for (int z = margin; z < WorldSize - margin; ++z) {
        for (int x = margin; x < WorldSize - margin; ++x) {
            for (int y = WorldHeight - 1; y >= 0; --y) {
                if (!spawnSurface(world.get(x, y, z))) continue;
                const int dx = x - WorldSize / 2;
                const int dz = z - WorldSize / 2;
                const int centerDistance = dx * dx + dz * dz;
                if (y > bestY || (y == bestY && centerDistance < bestCenterDistance)) {
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                    bestCenterDistance = centerDistance;
                }
                break;
            }
        }
    }
    if (bestY >= 0) return {bestX + 0.5f, bestY + 1.2f, bestZ + 0.5f};
    return {WorldSize / 2.0f, WorldHeight + 4.0f, WorldSize / 2.0f};
}

Vec3 spawnPositionOppositeFace(const World& world) {
    constexpr int margin = 4;
    int bestX = WorldSize / 2;
    int bestY = WorldHeight;
    int bestZ = WorldSize / 2;
    int bestCenterDistance = WorldSize * WorldSize;
    for (int z = margin; z < WorldSize - margin; ++z) {
        for (int x = margin; x < WorldSize - margin; ++x) {
            for (int y = 0; y < WorldHeight; ++y) {
                if (!spawnSurface(world.get(x, y, z))) continue;
                const int dx = x - WorldSize / 2;
                const int dz = z - WorldSize / 2;
                const int centerDistance = dx * dx + dz * dz;
                if (y < bestY || (y == bestY && centerDistance < bestCenterDistance)) {
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                    bestCenterDistance = centerDistance;
                }
                break;
            }
        }
    }
    if (bestY < WorldHeight) return {bestX + 0.5f, bestY - PlayerHeight - 0.2f, bestZ + 0.5f};
    return {WorldSize / 2.0f, -PlayerHeight - 4.0f, WorldSize / 2.0f};
}

void respawnPlayer(const World& world, Player& player, bool oppositeFace = false) {
    player.upSign = oppositeFace ? -1.0f : 1.0f;
    player.position = oppositeFace ? spawnPositionOppositeFace(world) : spawnPosition(world);
    player.velocity = {};
    player.grounded = false;
    player.jumpWasDown = false;
    player.coyoteTimer = 0.0f;
    player.jumpBufferTimer = 0.0f;
    player.health = 100;
    player.hurtTimer = 0.0f;
    if (oppositeFace) {
        player.yaw = Pi;
        player.pitch = 0.10f;
    }
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        std::exit(1);
    }
    return shader;
}

GLuint makeProgram(const char* vertex, const char* fragment) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertex);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragment);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
        std::exit(1);
    }
    return program;
}

VoxelUniforms voxelUniforms(GLuint program) {
    return {
        program,
        glGetUniformLocation(program, "uMVP"),
        glGetUniformLocation(program, "uModel"),
        glGetUniformLocation(program, "uCamera"),
        glGetUniformLocation(program, "uHeadlampDir"),
        glGetUniformLocation(program, "uSunDir"),
        glGetUniformLocation(program, "uFogColor"),
        glGetUniformLocation(program, "uTime"),
        glGetUniformLocation(program, "uFeedClarity")
    };
}

LineUniforms lineUniforms(GLuint program) {
    return {
        program,
        glGetUniformLocation(program, "uMVP"),
        glGetUniformLocation(program, "uColor")
    };
}

TextureUniforms textureUniforms(GLuint program) {
    return {
        program,
        glGetUniformLocation(program, "uRect"),
        glGetUniformLocation(program, "uTexture"),
        glGetUniformLocation(program, "uTime")
    };
}

MissileFeedUniforms missileFeedUniforms(GLuint program) {
    return {
        program,
        glGetUniformLocation(program, "uRect"),
        glGetUniformLocation(program, "uTexture")
    };
}

ForcefieldUniforms forcefieldUniforms(GLuint program) {
    return {
        program,
        glGetUniformLocation(program, "uMVP"),
        glGetUniformLocation(program, "uTime"),
        glGetUniformLocation(program, "uFeedClarity")
    };
}

void uploadMesh(Mesh& mesh, const std::vector<Vertex>& vertices) {
    if (mesh.vao == 0) {
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
    }
    mesh.count = static_cast<GLsizei>(vertices.size());
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, kind)));
    glBindVertexArray(0);
}

void ensureSatelliteCamera(SatelliteCamera& camera) {
    if (camera.framebuffer != 0) return;
    camera.size = camera.satelliteSize;

    glGenFramebuffers(1, &camera.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, camera.framebuffer);

    glGenTextures(1, &camera.color);
    glBindTexture(GL_TEXTURE_2D, camera.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, camera.size, camera.size, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, camera.color, 0);

    glGenRenderbuffers(1, &camera.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, camera.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, camera.size, camera.size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, camera.depth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Satellite camera framebuffer is incomplete.\n");
        std::exit(1);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void resizeSatelliteCamera(SatelliteCamera& camera, int size) {
    if (camera.size == size) return;
    camera.size = size;
    glBindTexture(GL_TEXTURE_2D, camera.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, camera.size, camera.size, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindRenderbuffer(GL_RENDERBUFFER, camera.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, camera.size, camera.size);
}

void addFace(std::vector<Vertex>& vertices, int x, int y, int z, Block block, const std::array<Vec3, 4>& corners, Vec3 normal) {
    const auto c = colorOf(block, x, y, z);
    const float kind = blockKind(block);
    const std::array<int, 6> order{0, 1, 2, 0, 2, 3};
    for (int i : order) {
        vertices.push_back({corners[i], normal, c[0], c[1], c[2], c[3], kind});
    }
}

void appendCube(std::vector<Vertex>& vertices, const World& world, int x, int y, int z, Block block) {
    const float x0 = static_cast<float>(x);
    const float y0 = static_cast<float>(y);
    const float z0 = static_cast<float>(z);
    const float x1 = x0 + 1.0f;
    const float y1 = y0 + 1.0f;
    const float z1 = z0 + 1.0f;

    if (transparent(world.get(x + 1, y, z)) && world.get(x + 1, y, z) != block)
        addFace(vertices, x, y, z, block, {{{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}}}, {1, 0, 0});
    if (transparent(world.get(x - 1, y, z)) && world.get(x - 1, y, z) != block)
        addFace(vertices, x, y, z, block, {{{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}}}, {-1, 0, 0});
    if (transparent(world.get(x, y + 1, z)) && world.get(x, y + 1, z) != block)
        addFace(vertices, x, y, z, block, {{{x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}}}, {0, 1, 0});
    if (transparent(world.get(x, y - 1, z)) && world.get(x, y - 1, z) != block)
        addFace(vertices, x, y, z, block, {{{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}}}, {0, -1, 0});
    if (transparent(world.get(x, y, z + 1)) && world.get(x, y, z + 1) != block)
        addFace(vertices, x, y, z, block, {{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}}, {0, 0, 1});
    if (transparent(world.get(x, y, z - 1)) && world.get(x, y, z - 1) != block)
        addFace(vertices, x, y, z, block, {{{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}}}, {0, 0, -1});
}

void addColoredFace(std::vector<Vertex>& vertices, const std::array<Vec3, 4>& corners, Vec3 normal, std::array<float, 4> color) {
    const std::array<int, 6> order{0, 1, 2, 0, 2, 3};
    for (int i : order) {
        vertices.push_back({corners[i], normal, color[0], color[1], color[2], color[3], 0.0f});
    }
}

void addColoredBox(std::vector<Vertex>& vertices, Vec3 min, Vec3 max, std::array<float, 4> color) {
    addColoredFace(vertices, {{{max.x, min.y, max.z}, {max.x, min.y, min.z}, {max.x, max.y, min.z}, {max.x, max.y, max.z}}}, {1, 0, 0}, color);
    addColoredFace(vertices, {{{min.x, min.y, min.z}, {min.x, min.y, max.z}, {min.x, max.y, max.z}, {min.x, max.y, min.z}}}, {-1, 0, 0}, color);
    addColoredFace(vertices, {{{min.x, max.y, max.z}, {max.x, max.y, max.z}, {max.x, max.y, min.z}, {min.x, max.y, min.z}}}, {0, 1, 0}, color);
    addColoredFace(vertices, {{{min.x, min.y, min.z}, {max.x, min.y, min.z}, {max.x, min.y, max.z}, {min.x, min.y, max.z}}}, {0, -1, 0}, color);
    addColoredFace(vertices, {{{min.x, min.y, max.z}, {max.x, min.y, max.z}, {max.x, max.y, max.z}, {min.x, max.y, max.z}}}, {0, 0, 1}, color);
    addColoredFace(vertices, {{{max.x, min.y, min.z}, {min.x, min.y, min.z}, {min.x, max.y, min.z}, {max.x, max.y, min.z}}}, {0, 0, -1}, color);
}

void rebuildSatelliteMesh(Mesh& satellite) {
    std::vector<Vertex> vertices;
    vertices.reserve(180);
    addColoredBox(vertices, {-0.42f, -0.24f, -0.32f}, {0.42f, 0.24f, 0.32f}, {0.42f, 0.43f, 0.41f, 1.0f});
    addColoredBox(vertices, {-0.12f, -0.38f, -0.12f}, {0.12f, -0.24f, 0.12f}, {0.05f, 0.08f, 0.09f, 1.0f});
    addColoredBox(vertices, {-1.38f, -0.04f, -0.36f}, {-0.48f, 0.04f, 0.36f}, {0.03f, 0.12f, 0.19f, 1.0f});
    addColoredBox(vertices, {0.48f, -0.04f, -0.36f}, {1.38f, 0.04f, 0.36f}, {0.03f, 0.12f, 0.19f, 1.0f});
    addColoredBox(vertices, {-0.06f, -0.08f, -0.56f}, {0.06f, 0.08f, -0.34f}, {0.58f, 0.08f, 0.04f, 1.0f});
    uploadMesh(satellite, vertices);
}

void rebuildRocketMesh(Mesh& rocket) {
    std::vector<Vertex> vertices;
    vertices.reserve(120);
    addColoredBox(vertices, {-0.14f, -0.14f, -0.42f}, {0.14f, 0.14f, 0.34f}, {0.30f, 0.32f, 0.30f, 1.0f});
    addColoredBox(vertices, {-0.10f, -0.10f, 0.34f}, {0.10f, 0.10f, 0.55f}, {0.62f, 0.10f, 0.04f, 1.0f});
    addColoredBox(vertices, {-0.20f, -0.035f, -0.48f}, {0.20f, 0.035f, -0.34f}, {0.10f, 0.095f, 0.085f, 1.0f});
    addColoredBox(vertices, {-0.035f, -0.20f, -0.48f}, {0.035f, 0.20f, -0.34f}, {0.10f, 0.095f, 0.085f, 1.0f});
    uploadMesh(rocket, vertices);
}

float forcefieldPlaneY(const World& world) {
    const int x = WorldSize / 2;
    const int z = WorldSize / 2;
    float topFace = static_cast<float>(WorldHeight);
    float bottomFace = 0.0f;
    for (int y = WorldHeight - 1; y >= 0; --y) {
        if (solid(world.get(x, y, z))) {
            topFace = static_cast<float>(y) + 1.0f;
            break;
        }
    }
    for (int y = 0; y < WorldHeight; ++y) {
        if (solid(world.get(x, y, z))) {
            bottomFace = static_cast<float>(y);
            break;
        }
    }
    return (topFace + bottomFace) * 0.5f;
}

void rebuildForcefieldMesh(const World& world, Mesh& forcefield) {
    std::vector<Vertex> vertices;
    vertices.reserve(12);
    constexpr float overhang = 5.0f;
    const float y = forcefieldPlaneY(world);
    const float min = -overhang;
    const float max = WorldSize + overhang;
    const std::array<Vec3, 4> top{{{min, y, min}, {max, y, min}, {max, y, max}, {min, y, max}}};
    const std::array<Vec3, 4> bottom{{{min, y, max}, {max, y, max}, {max, y, min}, {min, y, min}}};
    addColoredFace(vertices, top, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.24f, 0.62f});
    addColoredFace(vertices, bottom, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.24f, 0.62f});
    uploadMesh(forcefield, vertices);
}

void rebuildMeshes(const World& world, Mesh& opaque, Mesh& transparentMesh) {
    std::vector<Vertex> opaqueVertices;
    std::vector<Vertex> transparentVertices;
    opaqueVertices.reserve(180000);
    transparentVertices.reserve(32000);

    for (int y = 0; y < WorldHeight; ++y) {
        for (int z = 0; z < WorldSize; ++z) {
            for (int x = 0; x < WorldSize; ++x) {
                const Block block = world.get(x, y, z);
                if (block == Block::Air) continue;
                if (block == Block::DeadCrystal || block == Block::ObsidianGlass || block == Block::WindowGlass) appendCube(transparentVertices, world, x, y, z, block);
                else appendCube(opaqueVertices, world, x, y, z, block);
            }
        }
    }

    uploadMesh(opaque, opaqueVertices);
    uploadMesh(transparentMesh, transparentVertices);
}

Vec3 forwardFromAngles(float yaw, float pitch) {
    return normalize({std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)});
}

bool playerCollides(const World& world, Vec3 position) {
    const int minX = static_cast<int>(std::floor(position.x - PlayerRadius));
    const int maxX = static_cast<int>(std::floor(position.x + PlayerRadius));
    const int minY = static_cast<int>(std::floor(position.y));
    const int maxY = static_cast<int>(std::floor(position.y + PlayerHeight));
    const int minZ = static_cast<int>(std::floor(position.z - PlayerRadius));
    const int maxZ = static_cast<int>(std::floor(position.z + PlayerRadius));

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                if (solid(world.get(x, y, z))) return true;
            }
        }
    }
    return false;
}

bool playerCollides(const World& world, const Player& player, Vec3 position) {
    const int minX = static_cast<int>(std::floor(position.x - PlayerRadius));
    const int maxX = static_cast<int>(std::floor(position.x + PlayerRadius));
    const float headY = position.y + player.upSign * PlayerHeight;
    const int minY = static_cast<int>(std::floor(std::min(position.y, headY)));
    const int maxY = static_cast<int>(std::floor(std::max(position.y, headY)));
    const int minZ = static_cast<int>(std::floor(position.z - PlayerRadius));
    const int maxZ = static_cast<int>(std::floor(position.z + PlayerRadius));

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                if (solid(world.get(x, y, z))) return true;
            }
        }
    }
    return false;
}

float approach(float value, float target, float maxDelta) {
    if (value < target) return std::min(value + maxDelta, target);
    return std::max(value - maxDelta, target);
}

bool moveAxis(const World& world, Player& player, float& coord, float delta) {
    coord += delta;
    if (playerCollides(world, player, player.position)) {
        coord -= delta;
        if (&coord == &player.position.y) player.velocity.y = 0.0f;
        return true;
    }
    return false;
}

bool enforceForcefield(Player& player, float forcefieldY) {
    constexpr float barrierSkin = 0.08f;
    if (player.upSign > 0.0f && player.position.y < forcefieldY + barrierSkin) {
        player.position.y = forcefieldY + barrierSkin;
        if (player.velocity.y < 0.0f) player.velocity.y = 0.0f;
        player.grounded = true;
        return true;
    }
    if (player.upSign < 0.0f && player.position.y > forcefieldY - barrierSkin) {
        player.position.y = forcefieldY - barrierSkin;
        if (player.velocity.y > 0.0f) player.velocity.y = 0.0f;
        player.grounded = true;
        return true;
    }
    return false;
}

Vec3 rotateAroundAxis(Vec3 v, Vec3 axis, float angle) {
    axis = normalize(axis);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}

bool gamepadButtonDown(int button);
float gamepadAxis(int axis);

void updatePlayer(const World& world, Player& player, float dt, float forcefieldY, bool allowLook = true, bool allowMove = true) {
    if (allowLook) {
        player.lookVelocityX = player.lookVelocityX * 0.42f + pendingMouseLook.x * 0.58f;
        player.lookVelocityY = player.lookVelocityY * 0.42f + pendingMouseLook.y * 0.58f;
        pendingMouseLook = {};
        player.yaw -= player.lookVelocityX * 0.0021f;
        player.pitch -= player.lookVelocityY * 0.0021f;
        player.pitch = std::clamp(player.pitch, -1.45f, 1.45f);
    } else {
        player.lookVelocityX = 0.0f;
        player.lookVelocityY = 0.0f;
    }

    if (!allowMove) {
        player.velocity = {};
        player.jumpWasDown = false;
        player.jumpBufferTimer = 0.0f;
        player.coyoteTimer = 0.0f;
        pendingMouseLook = {};
        return;
    }

    Vec3 forward = normalize(forwardFromAngles(player.yaw, 0.0f));
    Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    Vec3 input{};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) input = input + forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) input = input - forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) input = input + right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) input = input - right;
    if (length(input) > 0.0f) input = normalize(input);

    const bool jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (jumpDown && !player.jumpWasDown) {
        player.jumpBufferTimer = JumpBufferTime;
    } else {
        player.jumpBufferTimer = std::max(0.0f, player.jumpBufferTimer - dt);
    }
    player.jumpWasDown = jumpDown;

    const bool sprinting = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    const float targetSpeed = sprinting ? SprintSpeed : WalkSpeed;
    const Vec3 desiredVelocity{input.x * targetSpeed, 0.0f, input.z * targetSpeed};
    const float accel = player.grounded ? GroundAcceleration : AirAcceleration;
    player.velocity.x = approach(player.velocity.x, desiredVelocity.x, accel * dt);
    player.velocity.z = approach(player.velocity.z, desiredVelocity.z, accel * dt);

    if (length(input) < 0.001f && player.grounded) {
        player.velocity.x = approach(player.velocity.x, 0.0f, GroundFriction * dt);
        player.velocity.z = approach(player.velocity.z, 0.0f, GroundFriction * dt);
    }

    player.coyoteTimer = player.grounded ? CoyoteTime : std::max(0.0f, player.coyoteTimer - dt);
    if (player.jumpBufferTimer > 0.0f && player.coyoteTimer > 0.0f) {
        player.velocity.y = JumpSpeed * player.upSign;
        player.grounded = false;
        player.coyoteTimer = 0.0f;
        player.jumpBufferTimer = 0.0f;
    }

    player.velocity.y -= Gravity * player.upSign * dt;
    if (player.upSign > 0.0f) player.velocity.y = std::max(player.velocity.y, -32.0f);
    else player.velocity.y = std::min(player.velocity.y, 32.0f);

    moveAxis(world, player, player.position.x, player.velocity.x * dt);
    moveAxis(world, player, player.position.z, player.velocity.z * dt);
    const float beforeY = player.position.y;
    const float verticalVelocity = player.velocity.y;
    const bool hitVertical = moveAxis(world, player, player.position.y, player.velocity.y * dt);
    player.grounded = hitVertical && verticalVelocity * player.upSign <= 0.0f && beforeY * player.upSign > player.position.y * player.upSign - 0.0001f;
    enforceForcefield(player, forcefieldY);

    if ((player.upSign > 0.0f && player.position.y < -4.0f) || (player.upSign < 0.0f && player.position.y > WorldHeight + 4.0f)) {
        respawnPlayer(world, player, player.upSign < 0.0f);
    }
}

void updatePlayerTwo(const World& world, Player& player, float dt, float forcefieldY, bool allowInput = true, P2Control control = P2Control::Keyboard) {
    if (!allowInput) {
        player.velocity = {};
        player.jumpWasDown = false;
        player.jumpBufferTimer = 0.0f;
        player.coyoteTimer = 0.0f;
        return;
    }

    const float lookSpeed = 1.9f * dt;
    if (control == P2Control::Gamepad) {
        player.yaw -= gamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X) * lookSpeed * 2.1f;
        player.pitch -= gamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y) * lookSpeed * 2.1f;
    } else {
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) player.yaw += lookSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) player.yaw -= lookSpeed;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) player.pitch += lookSpeed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) player.pitch -= lookSpeed;
    }
    player.pitch = std::clamp(player.pitch, -1.35f, 1.35f);

    Vec3 forward = normalize(forwardFromAngles(player.yaw, 0.0f));
    Vec3 right = normalize(cross(forward, {0.0f, player.upSign, 0.0f}));
    Vec3 input{};
    if (control == P2Control::Gamepad) {
        input = input + forward * (-gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y));
        input = input + right * gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X);
    } else {
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) input = input + forward;
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) input = input - forward;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) input = input + right;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) input = input - right;
    }
    if (length(input) > 0.0f) input = normalize(input);

    const bool jumpDown = control == P2Control::Gamepad
        ? gamepadButtonDown(GLFW_GAMEPAD_BUTTON_A)
        : (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS);
    if (jumpDown && !player.jumpWasDown) player.jumpBufferTimer = JumpBufferTime;
    else player.jumpBufferTimer = std::max(0.0f, player.jumpBufferTimer - dt);
    player.jumpWasDown = jumpDown;

    const bool sprinting = control == P2Control::Gamepad
        ? gamepadButtonDown(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)
        : glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const float targetSpeed = sprinting ? SprintSpeed : WalkSpeed;
    const Vec3 desiredVelocity{input.x * targetSpeed, 0.0f, input.z * targetSpeed};
    const float accel = player.grounded ? GroundAcceleration : AirAcceleration;
    player.velocity.x = approach(player.velocity.x, desiredVelocity.x, accel * dt);
    player.velocity.z = approach(player.velocity.z, desiredVelocity.z, accel * dt);
    if (length(input) < 0.001f && player.grounded) {
        player.velocity.x = approach(player.velocity.x, 0.0f, GroundFriction * dt);
        player.velocity.z = approach(player.velocity.z, 0.0f, GroundFriction * dt);
    }

    player.coyoteTimer = player.grounded ? CoyoteTime : std::max(0.0f, player.coyoteTimer - dt);
    if (player.jumpBufferTimer > 0.0f && player.coyoteTimer > 0.0f) {
        player.velocity.y = JumpSpeed * player.upSign;
        player.grounded = false;
        player.coyoteTimer = 0.0f;
        player.jumpBufferTimer = 0.0f;
    }

    player.velocity.y -= Gravity * player.upSign * dt;
    if (player.upSign > 0.0f) player.velocity.y = std::max(player.velocity.y, -32.0f);
    else player.velocity.y = std::min(player.velocity.y, 32.0f);

    moveAxis(world, player, player.position.x, player.velocity.x * dt);
    moveAxis(world, player, player.position.z, player.velocity.z * dt);
    const float beforeY = player.position.y;
    const float verticalVelocity = player.velocity.y;
    const bool hitVertical = moveAxis(world, player, player.position.y, player.velocity.y * dt);
    player.grounded = hitVertical && verticalVelocity * player.upSign <= 0.0f && beforeY * player.upSign > player.position.y * player.upSign - 0.0001f;
    enforceForcefield(player, forcefieldY);

    if ((player.upSign > 0.0f && player.position.y < -4.0f) || (player.upSign < 0.0f && player.position.y > WorldHeight + 4.0f)) {
        respawnPlayer(world, player, player.upSign < 0.0f);
    }
}

bool canFireRocket(const Player& player, bool atomic) {
    const int cost = atomic ? 5 : 1;
    return player.fuel >= cost && player.plutonium >= cost;
}

void spendRocketResources(Player& player, bool atomic) {
    const int cost = atomic ? 5 : 1;
    player.fuel = std::max(0, player.fuel - cost);
    player.plutonium = std::max(0, player.plutonium - cost);
}

void fireRocket(Rocket& rocket, int owner, bool atomic, Vec3 eye, Vec3 look, float yaw, float pitch) {
    rocket.active = true;
    rocket.owner = owner;
    rocket.atomic = atomic;
    rocket.age = 0.0f;
    rocket.position = eye + look * 1.1f;
    rocket.direction = look;
    Vec3 right = normalize(cross(rocket.direction, {0.0f, 1.0f, 0.0f}));
    if (length(right) < 0.001f) right = {1.0f, 0.0f, 0.0f};
    rocket.up = normalize(cross(right, rocket.direction));
    rocket.yaw = yaw;
    rocket.pitch = pitch;
}

void triggerBlast(Blast& blast, Vec3 position, bool atomic) {
    blast.active = true;
    blast.atomic = atomic;
    blast.age = 0.0f;
    blast.position = position;
}

void updateRocket(const World& world, Rocket& rocket, Blast& blast, float dt, bool guided) {
    if (!rocket.active) return;
    rocket.age += dt;
    if (guided) {
        Vec3 right = normalize(cross(rocket.direction, rocket.up));
        if (length(right) < 0.001f) right = {1.0f, 0.0f, 0.0f};
        const float yawDelta = -pendingMouseLook.x * 0.0075f;
        const float pitchDelta = -pendingMouseLook.y * 0.0075f;
        rocket.direction = normalize(rotateAroundAxis(rocket.direction, rocket.up, yawDelta));
        right = normalize(rotateAroundAxis(right, rocket.up, yawDelta));
        rocket.direction = normalize(rotateAroundAxis(rocket.direction, right, pitchDelta));
        rocket.up = normalize(cross(right, rocket.direction));
        pendingMouseLook = {};
    }
    const float speed = guided ? 8.5f : 7.5f;
    rocket.position = rocket.position + rocket.direction * (speed * dt);
    const int x = static_cast<int>(std::floor(rocket.position.x));
    const int y = static_cast<int>(std::floor(rocket.position.y));
    const int z = static_cast<int>(std::floor(rocket.position.z));
    const Vec3 worldCenter{WorldSize * 0.5f, WorldHeight * 0.5f, WorldSize * 0.5f};
    const bool hitBlock = world.inBounds(x, y, z) && solid(world.get(x, y, z));
    const bool tooFar = length(rocket.position - worldCenter) > 260.0f;
    if (hitBlock) {
        triggerBlast(blast, rocket.position, rocket.atomic);
        rocket.active = false;
    } else if (tooFar || rocket.age > 60.0f) {
        rocket.active = false;
    }
}

void updateBlast(Blast& blast, float dt) {
    if (!blast.active) return;
    blast.age += dt;
    if (blast.age > (blast.atomic ? 1.9f : 1.35f)) blast.active = false;
}

bool destroyBlocksInBlast(World& world, Vec3 center, float radius) {
    bool changed = false;
    const int minX = std::max(0, static_cast<int>(std::floor(center.x - radius)));
    const int maxX = std::min(WorldSize - 1, static_cast<int>(std::ceil(center.x + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(center.y - radius)));
    const int maxY = std::min(WorldHeight - 1, static_cast<int>(std::ceil(center.y + radius)));
    const int minZ = std::max(0, static_cast<int>(std::floor(center.z - radius)));
    const int maxZ = std::min(WorldSize - 1, static_cast<int>(std::ceil(center.z + radius)));
    const float r2 = radius * radius;
    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const Vec3 blockCenter{x + 0.5f, y + 0.5f, z + 0.5f};
                if (dot(blockCenter - center, blockCenter - center) <= r2 && world.get(x, y, z) != Block::Air) {
                    world.set(x, y, z, Block::Air);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

void collectResource(Player& player, Block block) {
    if (block == Block::Fuel) player.fuel = std::min(player.fuel + 1, 20);
    if (block == Block::Plutonium) {
        player.plutonium = std::min(player.plutonium + 1, 20);
        player.health = std::min(player.health + 15, 100);
    }
}

bool collectResourcesInBlast(World& world, Player& owner, Vec3 center, float radius) {
    bool found = false;
    const int minX = std::max(0, static_cast<int>(std::floor(center.x - radius)));
    const int maxX = std::min(WorldSize - 1, static_cast<int>(std::ceil(center.x + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(center.y - radius)));
    const int maxY = std::min(WorldHeight - 1, static_cast<int>(std::ceil(center.y + radius)));
    const int minZ = std::max(0, static_cast<int>(std::floor(center.z - radius)));
    const int maxZ = std::min(WorldSize - 1, static_cast<int>(std::ceil(center.z + radius)));
    const float r2 = radius * radius;
    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const Block block = world.get(x, y, z);
                if (block != Block::Fuel && block != Block::Plutonium) continue;
                const Vec3 blockCenter{x + 0.5f, y + 0.5f, z + 0.5f};
                if (dot(blockCenter - center, blockCenter - center) <= r2) {
                    collectResource(owner, block);
                    found = true;
                }
            }
        }
    }
    return found;
}

bool applyBlastDamage(const World& world, Player& player, bool oppositeFace, Player& attacker, Vec3 center, float radius) {
    const Vec3 torso = player.position + Vec3{0.0f, player.upSign * PlayerHeight * 0.5f, 0.0f};
    const float distance = length(torso - center);
    if (distance > radius) return false;
    const int damage = static_cast<int>((1.0f - distance / radius) * 95.0f) + 18;
    player.health = std::max(0, player.health - damage);
    player.hurtTimer = 0.75f;
    if (player.health == 0) {
        attacker.score += 1;
        attacker.fuel = std::min(attacker.fuel + 1, 20);
        attacker.plutonium = std::min(attacker.plutonium + 1, 20);
        respawnPlayer(world, player, oppositeFace);
        return true;
    }
    return false;
}

bool applyToolDamage(const World& world, Player& target, bool targetOppositeFace, Player& attacker, int damage) {
    target.health = std::max(0, target.health - damage);
    target.hurtTimer = 0.45f;
    if (target.health == 0) {
        attacker.score += 1;
        attacker.fuel = std::min(attacker.fuel + 1, 20);
        attacker.plutonium = std::min(attacker.plutonium + 1, 20);
        respawnPlayer(world, target, targetOppositeFace);
        return true;
    }
    return false;
}

bool rayHitsPlayer(Vec3 origin, Vec3 direction, const Player& target, float maxDistance) {
    const Vec3 torso = target.position + Vec3{0.0f, target.upSign * PlayerHeight * 0.55f, 0.0f};
    const Vec3 toTarget = torso - origin;
    const float along = dot(toTarget, direction);
    if (along < 0.0f || along > maxDistance) return false;
    const Vec3 closest = origin + direction * along;
    return length(closest - torso) < 0.95f;
}

bool raycastBlock(const World& world, Vec3 origin, Vec3 direction, float maxDistance, Vec3& hit, Vec3& previous) {
    Vec3 last = origin;
    for (float t = 0.0f; t < maxDistance; t += 0.04f) {
        const Vec3 point = origin + direction * t;
        const int x = static_cast<int>(std::floor(point.x));
        const int y = static_cast<int>(std::floor(point.y));
        const int z = static_cast<int>(std::floor(point.z));
        if (solid(world.get(x, y, z))) {
            hit = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
            previous = {std::floor(last.x), std::floor(last.y), std::floor(last.z)};
            return true;
        }
        last = point;
    }
    return false;
}

Vec3 satellitePositionAt(const World& world, float time, SatelliteOrbit orbit, float orbitHeight) {
    (void)world;
    const Vec3 worldCenter{WorldSize * 0.5f, WorldHeight * 0.5f, WorldSize * 0.5f};
    const float angle = time * 0.22f;
    if (orbit == SatelliteOrbit::OppositeFace) {
        return {
            worldCenter.x + std::cos(angle) * 25.0f,
            worldCenter.y - ((WorldHeight + orbitHeight) - worldCenter.y) - std::sin(angle * 0.67f) * 4.0f,
            worldCenter.z + std::sin(angle) * 25.0f
        };
    }
    if (orbit == SatelliteOrbit::Polar) {
        constexpr float HalfWorld = WorldSize * 0.5f;
        const float PolarAltitude = orbitHeight;
        const float outer = HalfWorld + PolarAltitude;
        const float side = HalfWorld * 2.0f;
        const float corner = PolarAltitude * Pi * 0.5f;
        const float lap = side * 4.0f + corner * 4.0f;
        float d = std::fmod(angle / (Pi * 2.0f) * lap, lap);
        if (d < 0.0f) d += lap;

        auto cornerPoint = [PolarAltitude](float centerZ, float centerY, float start, float t) {
            const float a = start - t * Pi * 0.5f;
            return Vec2{centerZ + std::cos(a) * PolarAltitude, centerY + std::sin(a) * PolarAltitude};
        };

        float z = -HalfWorld;
        float y = outer;
        if (d < side) {
            z = -HalfWorld + d;
        } else if ((d -= side) < corner) {
            const Vec2 p = cornerPoint(HalfWorld, HalfWorld, Pi * 0.5f, d / corner);
            z = p.x;
            y = p.y;
        } else if ((d -= corner) < side) {
            y = HalfWorld - d;
            z = outer;
        } else if ((d -= side) < corner) {
            const Vec2 p = cornerPoint(HalfWorld, -HalfWorld, 0.0f, d / corner);
            z = p.x;
            y = p.y;
        } else if ((d -= corner) < side) {
            y = -outer;
            z = HalfWorld - d;
        } else if ((d -= side) < corner) {
            const Vec2 p = cornerPoint(-HalfWorld, -HalfWorld, -Pi * 0.5f, d / corner);
            z = p.x;
            y = p.y;
        } else if ((d -= corner) < side) {
            y = -HalfWorld + d;
            z = -outer;
        } else {
            d -= side;
            const Vec2 p = cornerPoint(-HalfWorld, HalfWorld, Pi, d / corner);
            z = p.x;
            y = p.y;
        }
        return {worldCenter.x, worldCenter.y + y, worldCenter.z + z};
    }
    return {
        worldCenter.x + std::cos(angle) * 25.0f,
        WorldHeight + orbitHeight + std::sin(angle * 0.67f) * 4.0f,
        worldCenter.z + std::sin(angle) * 25.0f
    };
}

Vec3 closestPointOnPlanetCube(Vec3 point) {
    return {
        std::clamp(point.x, 0.0f, static_cast<float>(WorldSize)),
        std::clamp(point.y, 0.0f, static_cast<float>(WorldHeight)),
        std::clamp(point.z, 0.0f, static_cast<float>(WorldSize))
    };
}

SatelliteView makeSatelliteView(Vec3 position) {
    SatelliteView view{};
    view.position = position;
    view.target = closestPointOnPlanetCube(position);
    view.down = normalize(view.target - view.position);
    view.up = {1.0f, 0.0f, 0.0f};
    const Mat4 proj = perspective(46.0f * Pi / 180.0f, 1.0f, 0.2f, 180.0f);
    const Mat4 camera = lookAt(view.position, view.target, view.up);
    view.vp = multiply(proj, camera);
    return view;
}

GLuint makeLineVao(GLuint& vbo) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 320, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
    glBindVertexArray(0);
    return vao;
}

GLuint makeUiVao(GLuint& vbo) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(UiVertex) * MaxUiVertices, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), reinterpret_cast<void*>(offsetof(UiVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex), reinterpret_cast<void*>(offsetof(UiVertex, r)));
    glBindVertexArray(0);
    return vao;
}

void addUiRect(std::vector<UiVertex>& vertices, float x, float y, float w, float h, std::array<float, 4> color) {
    const float x0 = x * 2.0f - 1.0f;
    const float y0 = 1.0f - y * 2.0f;
    const float x1 = (x + w) * 2.0f - 1.0f;
    const float y1 = 1.0f - (y + h) * 2.0f;
    const std::array<UiVertex, 6> quad{{
        {x0, y0, color[0], color[1], color[2], color[3]},
        {x1, y0, color[0], color[1], color[2], color[3]},
        {x1, y1, color[0], color[1], color[2], color[3]},
        {x0, y0, color[0], color[1], color[2], color[3]},
        {x1, y1, color[0], color[1], color[2], color[3]},
        {x0, y1, color[0], color[1], color[2], color[3]}
    }};
    vertices.insert(vertices.end(), quad.begin(), quad.end());
}

Vec2 rotateUi(Vec2 p, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {p.x * c - p.y * s, p.x * s + p.y * c};
}

void addUiQuad(std::vector<UiVertex>& vertices, std::array<Vec2, 4> points, std::array<float, 4> color) {
    auto toVertex = [color](Vec2 p) {
        return UiVertex{p.x * 2.0f - 1.0f, 1.0f - p.y * 2.0f, color[0], color[1], color[2], color[3]};
    };
    const std::array<UiVertex, 6> quad{{
        toVertex(points[0]), toVertex(points[1]), toVertex(points[2]),
        toVertex(points[0]), toVertex(points[2]), toVertex(points[3])
    }};
    vertices.insert(vertices.end(), quad.begin(), quad.end());
}

void addUiRectRotated(std::vector<UiVertex>& vertices, Vec2 origin, float angle, float x, float y, float w, float h, std::array<float, 4> color) {
    std::array<Vec2, 4> points{{
        origin + rotateUi({x, y}, angle),
        origin + rotateUi({x + w, y}, angle),
        origin + rotateUi({x + w, y + h}, angle),
        origin + rotateUi({x, y + h}, angle)
    }};
    addUiQuad(vertices, points, color);
}

const std::array<const char*, 7>& glyphRows(char ch) {
    static const std::array<const char*, 7> blank{{"00000", "00000", "00000", "00000", "00000", "00000", "00000"}};
    static const std::array<const char*, 7> unknown{{"11110", "00010", "00100", "01000", "01000", "00000", "01000"}};
    static const std::array<std::array<const char*, 7>, 36> glyphs{{
        {{"01110", "10001", "10011", "10101", "11001", "10001", "01110"}},
        {{"00100", "01100", "00100", "00100", "00100", "00100", "01110"}},
        {{"01110", "10001", "00001", "00010", "00100", "01000", "11111"}},
        {{"11110", "00001", "00001", "01110", "00001", "00001", "11110"}},
        {{"00010", "00110", "01010", "10010", "11111", "00010", "00010"}},
        {{"11111", "10000", "10000", "11110", "00001", "00001", "11110"}},
        {{"01110", "10000", "10000", "11110", "10001", "10001", "01110"}},
        {{"11111", "00001", "00010", "00100", "01000", "01000", "01000"}},
        {{"01110", "10001", "10001", "01110", "10001", "10001", "01110"}},
        {{"01110", "10001", "10001", "01111", "00001", "00001", "01110"}},
        {{"01110", "10001", "10001", "11111", "10001", "10001", "10001"}},
        {{"11110", "10001", "10001", "11110", "10001", "10001", "11110"}},
        {{"01111", "10000", "10000", "10000", "10000", "10000", "01111"}},
        {{"11110", "10001", "10001", "10001", "10001", "10001", "11110"}},
        {{"11111", "10000", "10000", "11110", "10000", "10000", "11111"}},
        {{"11111", "10000", "10000", "11110", "10000", "10000", "10000"}},
        {{"01111", "10000", "10000", "10011", "10001", "10001", "01111"}},
        {{"10001", "10001", "10001", "11111", "10001", "10001", "10001"}},
        {{"01110", "00100", "00100", "00100", "00100", "00100", "01110"}},
        {{"00001", "00001", "00001", "00001", "10001", "10001", "01110"}},
        {{"10001", "10010", "10100", "11000", "10100", "10010", "10001"}},
        {{"10000", "10000", "10000", "10000", "10000", "10000", "11111"}},
        {{"10001", "11011", "10101", "10101", "10001", "10001", "10001"}},
        {{"10001", "11001", "10101", "10011", "10001", "10001", "10001"}},
        {{"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
        {{"11110", "10001", "10001", "11110", "10000", "10000", "10000"}},
        {{"01110", "10001", "10001", "10001", "10101", "10010", "01101"}},
        {{"11110", "10001", "10001", "11110", "10100", "10010", "10001"}},
        {{"01111", "10000", "10000", "01110", "00001", "00001", "11110"}},
        {{"11111", "00100", "00100", "00100", "00100", "00100", "00100"}},
        {{"10001", "10001", "10001", "10001", "10001", "10001", "01110"}},
        {{"10001", "10001", "10001", "10001", "10001", "01010", "00100"}},
        {{"10001", "10001", "10001", "10101", "10101", "11011", "10001"}},
        {{"10001", "10001", "01010", "00100", "01010", "10001", "10001"}},
        {{"10001", "10001", "01010", "00100", "00100", "00100", "00100"}},
        {{"11111", "00001", "00010", "00100", "01000", "10000", "11111"}}
    }};
    if (ch == ' ') return blank;
    if (ch >= '0' && ch <= '9') return glyphs[static_cast<std::size_t>(ch - '0')];
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') return glyphs[static_cast<std::size_t>(10 + ch - 'A')];
    return unknown;
}

void addUiText(std::vector<UiVertex>& vertices, float x, float y, float size, const std::string& text, std::array<float, 4> color) {
    const float pixel = size;
    const float gap = pixel;
    float cursor = x;
    for (char ch : text) {
        if (ch == '-') {
            addUiRect(vertices, cursor, y + pixel * 3.0f, pixel * 4.0f, pixel, color);
            cursor += pixel * 6.0f;
            continue;
        }
        if (ch == '/') {
            for (int row = 0; row < 5; ++row) addUiRect(vertices, cursor + pixel * static_cast<float>(4 - row), y + pixel * static_cast<float>(row + 1), pixel, pixel, color);
            cursor += pixel * 6.0f;
            continue;
        }
        if (ch == ':') {
            addUiRect(vertices, cursor + pixel * 2.0f, y + pixel * 2.0f, pixel, pixel, color);
            addUiRect(vertices, cursor + pixel * 2.0f, y + pixel * 5.0f, pixel, pixel, color);
            cursor += pixel * 4.0f;
            continue;
        }
        const auto& rows = glyphRows(ch);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[static_cast<std::size_t>(row)][col] == '1') {
                    addUiRect(vertices, cursor + static_cast<float>(col) * pixel, y + static_cast<float>(row) * pixel, pixel, pixel, color);
                }
            }
        }
        cursor += pixel * 5.0f + gap;
    }
}

float uiTextWidth(float size, const std::string& text) {
    float width = 0.0f;
    for (char ch : text) {
        if (ch == ':') width += size * 4.0f;
        else width += size * 6.0f;
    }
    return width;
}

void addUiTextCentered(std::vector<UiVertex>& vertices, float centerX, float y, float size, const std::string& text, std::array<float, 4> color) {
    addUiText(vertices, centerX - uiTextWidth(size, text) * 0.5f, y, size, text, color);
}

void drawHand(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, bool mining, bool building, bool rocketLauncher, float progress, float time, bool alternatePalette = false) {
    const float action = (mining || building || rocketLauncher) ? std::sin(std::clamp(progress, 0.0f, 1.0f) * Pi) : 0.0f;
    const float idle = std::sin(time * 1.7f) * 0.006f;
    const float sx = -0.015f - action * 0.030f;
    const float sy = idle + action * 0.030f;
    const float swing = -0.50f - action * 0.28f;
    std::vector<UiVertex> vertices;
    vertices.reserve(108);

    const Vec2 pivot{0.73f + sx, 0.73f + sy};
    if (rocketLauncher) {
        const Vec2 tail{0.825f + sx, 0.800f + sy};
        const Vec2 muzzle{0.555f, 0.545f};
        const Vec2 aim{muzzle.x - tail.x, muzzle.y - tail.y};
        const float launcherAngle = std::atan2(aim.y, aim.x);
        const float launcherLength = std::sqrt(aim.x * aim.x + aim.y * aim.y);
        const std::array<float, 4> launcherBody = alternatePalette ? std::array<float, 4>{0.050f, 0.075f, 0.105f, 1.0f} : std::array<float, 4>{0.055f, 0.064f, 0.060f, 1.0f};
        const std::array<float, 4> launcherBand = alternatePalette ? std::array<float, 4>{0.72f, 0.86f, 0.08f, 1.0f} : std::array<float, 4>{0.42f, 0.12f, 0.055f, 1.0f};
        const std::array<float, 4> launcherTip = alternatePalette ? std::array<float, 4>{0.12f, 0.64f, 1.0f, 1.0f} : std::array<float, 4>{0.70f, 0.18f, 0.06f, 1.0f};
        addUiRectRotated(vertices, tail, launcherAngle, 0.000f, -0.034f, launcherLength, 0.068f, launcherBody);
        addUiRectRotated(vertices, tail, launcherAngle, -0.020f, -0.052f, 0.075f, 0.104f, {0.18f, 0.20f, 0.18f, 1.0f});
        addUiRectRotated(vertices, tail, launcherAngle, launcherLength - 0.046f, -0.060f, 0.052f, 0.120f, launcherBand);
        addUiRectRotated(vertices, tail, launcherAngle, launcherLength - 0.035f, -0.082f, 0.034f, 0.044f, launcherTip);
        addUiRectRotated(vertices, tail, launcherAngle, 0.080f, 0.020f, 0.052f, 0.086f, {0.11f, 0.115f, 0.105f, 1.0f});
        addUiRectRotated(vertices, tail, launcherAngle, 0.108f, 0.088f, 0.035f, 0.058f, {0.035f, 0.032f, 0.030f, 1.0f});
    } else {
        addUiRectRotated(vertices, pivot, swing, -0.018f, -0.015f, 0.036f, 0.250f, {0.32f, 0.21f, 0.11f, 1.0f});
        addUiRectRotated(vertices, pivot, swing, -0.026f, 0.070f, 0.052f, 0.030f, {0.20f, 0.13f, 0.07f, 1.0f});
        const std::array<float, 4> toolColor = alternatePalette
            ? (building ? std::array<float, 4>{0.72f, 0.76f, 0.16f, 1.0f} : std::array<float, 4>{0.10f, 0.40f, 1.0f, 1.0f})
            : (building ? std::array<float, 4>{0.38f, 0.40f, 0.38f, 1.0f} : std::array<float, 4>{0.05f, 0.60f, 0.58f, 1.0f});
        const std::array<float, 4> toolShadow = alternatePalette
            ? (building ? std::array<float, 4>{0.30f, 0.32f, 0.08f, 1.0f} : std::array<float, 4>{0.03f, 0.14f, 0.38f, 1.0f})
            : (building ? std::array<float, 4>{0.18f, 0.19f, 0.19f, 1.0f} : std::array<float, 4>{0.02f, 0.28f, 0.29f, 1.0f});
        addUiRectRotated(vertices, pivot, swing, -0.105f, -0.060f, 0.180f, 0.050f, toolShadow);
        addUiRectRotated(vertices, pivot, swing, -0.085f, -0.080f, 0.115f, 0.060f, toolColor);
    }

    addUiRect(vertices, 0.75f + sx, 0.76f + sy, 0.24f, 0.14f, {0.055f, 0.048f, 0.044f, 0.94f});
    const std::array<float, 4> sleeve = alternatePalette ? std::array<float, 4>{0.18f, 0.23f, 0.34f, 1.0f} : std::array<float, 4>{0.34f, 0.27f, 0.22f, 1.0f};
    const std::array<float, 4> skin = alternatePalette ? std::array<float, 4>{0.50f, 0.58f, 0.66f, 1.0f} : std::array<float, 4>{0.62f, 0.50f, 0.40f, 1.0f};
    const std::array<float, 4> knuckle = alternatePalette ? std::array<float, 4>{0.58f, 0.68f, 0.76f, 1.0f} : std::array<float, 4>{0.70f, 0.56f, 0.44f, 1.0f};
    addUiRect(vertices, 0.72f + sx, 0.70f + sy, 0.21f, 0.14f, sleeve);
    addUiRect(vertices, 0.79f + sx, 0.66f + sy, 0.12f, 0.10f, skin);
    addUiRect(vertices, 0.89f + sx, 0.66f + sy, 0.052f, 0.074f, knuckle);

    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void drawReticle(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, bool guided, float time) {
    const float pulse = guided ? 0.18f + std::sin(time * 12.0f) * 0.05f : 0.0f;
    const std::array<float, 4> color = guided
        ? std::array<float, 4>{0.90f, 0.12f, 0.06f, 0.92f}
        : std::array<float, 4>{0.55f, 0.92f, 0.78f, 0.82f};
    std::vector<UiVertex> vertices;
    vertices.reserve(48);
    const float cx = 0.5f;
    const float cy = 0.5f;
    const float gap = 0.014f + pulse * 0.015f;
    const float len = 0.030f;
    const float thick = 0.0035f;
    addUiRect(vertices, cx - thick * 0.5f, cy - gap - len, thick, len, color);
    addUiRect(vertices, cx - thick * 0.5f, cy + gap, thick, len, color);
    addUiRect(vertices, cx - gap - len, cy - thick * 0.5f, len, thick, color);
    addUiRect(vertices, cx + gap, cy - thick * 0.5f, len, thick, color);
    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void drawSmallReticle(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, bool alternatePalette = false) {
    const std::array<float, 4> color = alternatePalette
        ? std::array<float, 4>{0.25f, 0.62f, 1.0f, 0.82f}
        : std::array<float, 4>{0.78f, 0.92f, 0.82f, 0.78f};
    std::vector<UiVertex> vertices;
    vertices.reserve(30);
    const float cx = 0.5f;
    const float cy = 0.5f;
    const float gap = 0.008f;
    const float len = 0.015f;
    const float thick = 0.0025f;
    addUiRect(vertices, cx - thick * 0.5f, cy - gap - len, thick, len, color);
    addUiRect(vertices, cx - thick * 0.5f, cy + gap, thick, len, color);
    addUiRect(vertices, cx - gap - len, cy - thick * 0.5f, len, thick, color);
    addUiRect(vertices, cx + gap, cy - thick * 0.5f, len, thick, color);
    addUiRect(vertices, cx - thick * 0.5f, cy - thick * 0.5f, thick, thick, {color[0], color[1], color[2], 0.55f});

    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void drawPlayerStatus(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, const Player& player, bool alternatePalette = false) {
    std::vector<UiVertex> vertices;
    vertices.reserve(96);
    const float health = std::clamp(static_cast<float>(player.health) / 100.0f, 0.0f, 1.0f);
    const float fuel = std::clamp(static_cast<float>(player.fuel) / 20.0f, 0.0f, 1.0f);
    const float plutonium = std::clamp(static_cast<float>(player.plutonium) / 20.0f, 0.0f, 1.0f);
    const std::array<float, 4> accent = alternatePalette
        ? std::array<float, 4>{0.12f, 0.55f, 1.0f, 0.92f}
        : std::array<float, 4>{1.0f, 0.32f, 0.08f, 0.92f};
    const std::array<float, 4> hurt = player.hurtTimer > 0.0f
        ? std::array<float, 4>{1.0f, 0.02f, 0.01f, 0.18f}
        : std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
    if (player.hurtTimer > 0.0f) addUiRect(vertices, 0.0f, 0.0f, 1.0f, 1.0f, hurt);
    addUiRect(vertices, 0.035f, 0.035f, 0.260f, 0.022f, {0.0f, 0.0f, 0.0f, 0.64f});
    addUiRect(vertices, 0.039f, 0.039f, 0.252f * health, 0.014f, {0.80f, 0.06f, 0.035f, 0.92f});
    addUiRect(vertices, 0.035f, 0.066f, 0.220f, 0.018f, {0.0f, 0.0f, 0.0f, 0.58f});
    addUiRect(vertices, 0.039f, 0.070f, 0.102f * fuel, 0.010f, {0.00f, 0.76f, 1.0f, 0.92f});
    addUiRect(vertices, 0.149f, 0.070f, 0.102f * plutonium, 0.010f, {0.92f, 1.0f, 0.02f, 0.92f});
    for (int i = 0; i < std::min(player.score, 8); ++i) {
        addUiRect(vertices, 0.035f + static_cast<float>(i) * 0.018f, 0.094f, 0.012f, 0.012f, accent);
    }
    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void addHudDigit(std::vector<UiVertex>& vertices, float x, float y, float size, int digit, std::array<float, 4> color);

void drawTitleScreen(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, WorldTheme theme, GameType gameType, P2Control p2Control, int selectedRow, float time) {
    std::vector<UiVertex> vertices;
    vertices.reserve(8000);
    const std::array<float, 4> glow{0.25f, 1.0f, 0.55f, 0.85f};
    const std::array<float, 4> ember{0.95f, 0.18f, 0.055f, 0.90f};
    const std::array<float, 4> blue{0.06f, 0.52f, 1.0f, 0.90f};
    const std::array<float, 4> yellow{0.86f, 1.0f, 0.03f, 0.90f};
    const std::array<float, 4> text{0.80f, 1.0f, 0.86f, 0.96f};
    const std::array<float, 4> muted{0.36f, 0.62f, 0.50f, 0.92f};

    addUiRect(vertices, 0.0f, 0.0f, 1.0f, 1.0f, {0.0f, 0.0f, 0.0f, 0.48f});
    addUiTextCentered(vertices, 0.5f, 0.145f, 0.0054f, GameTitleCaps, {0.95f, 1.0f, 0.92f, 0.98f});
    addUiRect(vertices, 0.32f, 0.205f, 0.36f, 0.006f, ember);

    auto row = [&](int index, float y, std::array<float, 4> accent, const std::string& label, const std::string& value) {
        const bool selected = selectedRow == index;
        addUiRect(vertices, 0.23f, y, 0.54f, 0.080f, selected ? std::array<float, 4>{0.035f, 0.12f, 0.085f, 0.92f} : std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.58f});
        addUiRect(vertices, 0.23f, y, 0.006f, 0.080f, selected ? glow : std::array<float, 4>{0.04f, 0.14f, 0.10f, 0.75f});
        addUiText(vertices, 0.255f, y + 0.027f, 0.0039f, label, selected ? text : muted);
        addUiText(vertices, 0.475f, y + 0.027f, 0.0039f, value, accent);
    };

    std::string mode = "FIRST TO 3";
    if (gameType == GameType::PhasedTurns) mode = "10 TURNS";
    if (gameType == GameType::AlternatingHunter) mode = "HUNTER";
    std::string world = "DEAD";
    if (theme == WorldTheme::Desert) world = "DESERT";
    if (theme == WorldTheme::Lush) world = "LUSH";
    if (theme == WorldTheme::Brutalist) world = "CITY";
    row(0, 0.325f, ember, "GAME", mode);
    row(1, 0.445f, blue, "WORLD", world);
    row(2, 0.565f, yellow, "P2", p2Control == P2Control::Keyboard ? "KEYS" : "PAD");

    const float pulse = 0.75f + std::sin(time * 4.0f) * 0.20f;
    addUiTextCentered(vertices, 0.5f, 0.735f, 0.0041f, "ENTER TO START", {0.25f, 1.0f, 0.55f, pulse});
    addUiTextCentered(vertices, 0.5f, 0.790f, 0.0032f, "UP DOWN   LEFT RIGHT", muted);

    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    if (vertices.size() > MaxUiVertices) vertices.resize(MaxUiVertices);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void addHudDigit(std::vector<UiVertex>& vertices, float x, float y, float size, int digit, std::array<float, 4> color) {
    const float t = size * 0.16f;
    const float w = size * 0.62f;
    const float h = size;
    const std::array<std::array<bool, 7>, 10> segments{{
        {{true, true, true, false, true, true, true}},
        {{false, false, true, false, false, true, false}},
        {{true, false, true, true, true, false, true}},
        {{true, false, true, true, false, true, true}},
        {{false, true, true, true, false, true, false}},
        {{true, true, false, true, false, true, true}},
        {{true, true, false, true, true, true, true}},
        {{true, false, true, false, false, true, false}},
        {{true, true, true, true, true, true, true}},
        {{true, true, true, true, false, true, true}}
    }};
    digit = std::clamp(digit, 0, 9);
    const auto& s = segments[digit];
    if (s[0]) addUiRect(vertices, x, y, w, t, color);
    if (s[1]) addUiRect(vertices, x, y, t, h * 0.50f, color);
    if (s[2]) addUiRect(vertices, x + w - t, y, t, h * 0.50f, color);
    if (s[3]) addUiRect(vertices, x, y + h * 0.42f, w, t, color);
    if (s[4]) addUiRect(vertices, x, y + h * 0.42f, t, h * 0.50f, color);
    if (s[5]) addUiRect(vertices, x + w - t, y + h * 0.42f, t, h * 0.50f, color);
    if (s[6]) addUiRect(vertices, x, y + h - t, w, t, color);
}

bool projectToUi(const Mat4& m, Vec3 p, Vec2& out) {
    const float x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    const float y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    const float w = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    if (std::abs(w) < 0.0001f) return false;
    const float nx = x / w;
    const float ny = y / w;
    out = {nx * 0.5f + 0.5f, 0.5f - ny * 0.5f};
    return w > 0.0f;
}

void drawSatellitePositionLabels(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, const Mat4& satelliteVp, const Player& playerOne, const Player& playerTwo, int width, int height) {
    std::vector<UiVertex> vertices;
    vertices.reserve(120);
    const float pixels = std::clamp(static_cast<float>(height) * 0.58f, 360.0f, 520.0f);
    const float margin = 18.0f;
    const float panelX = (static_cast<float>(width) - pixels - margin) / static_cast<float>(width);
    const float panelY = margin / static_cast<float>(height);
    const float panelW = pixels / static_cast<float>(width);
    const float panelH = pixels / static_cast<float>(height);
    auto addLabel = [&vertices, &satelliteVp, panelX, panelY, panelW, panelH](const Player& player, int digit, std::array<float, 4> color) {
        const Vec3 head = player.position + Vec3{0.0f, player.upSign * (PlayerHeight + 3.8f), 0.0f};
        Vec2 pos{};
        if (!projectToUi(satelliteVp, head, pos)) return;
        pos.x = panelX + std::clamp(pos.x, 0.06f, 0.88f) * panelW;
        pos.y = panelY + std::clamp(pos.y, 0.06f, 0.84f) * panelH;
        const float size = 0.043f;
        addUiRect(vertices, pos.x - 0.011f, pos.y - 0.009f, 0.040f, 0.054f, {0.0f, 0.0f, 0.0f, 0.54f});
        addHudDigit(vertices, pos.x, pos.y, size, digit, color);
        addUiRect(vertices, pos.x - 0.008f, pos.y + 0.048f, 0.037f, 0.003f, {color[0], color[1], color[2], 0.82f});
    };
    addLabel(playerOne, 1, {1.0f, 0.30f, 0.08f, 0.98f});
    addLabel(playerTwo, 2, {0.15f, 0.55f, 1.0f, 0.98f});
    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void drawMissileHud(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, const Rocket& rocket, int width, int height, float time) {
    const float pixels = std::clamp(static_cast<float>(height) * 0.58f, 360.0f, 520.0f);
    const float margin = 18.0f;
    const float x = (static_cast<float>(width) - pixels - margin) / static_cast<float>(width);
    const float y = margin / static_cast<float>(height);
    const float s = pixels / static_cast<float>(height);
    const float aspectFix = static_cast<float>(height) / static_cast<float>(width);
    const float cx = x + s * 0.5f * aspectFix;
    const float cy = y + s * 0.5f;
    const float panelW = pixels / static_cast<float>(width);
    const float panelH = pixels / static_cast<float>(height);
    const float roll = std::atan2(rocket.up.x, rocket.up.y) * 0.35f;
    const float pitch = std::clamp(rocket.direction.y, -1.0f, 1.0f) * panelH * 0.24f;
    const std::array<float, 4> green{0.42f, 1.0f, 0.62f, 0.78f};
    const std::array<float, 4> dim{0.18f, 0.62f, 0.34f, 0.48f};
    std::vector<UiVertex> vertices;
    vertices.reserve(180);

    for (int i = -2; i <= 2; ++i) {
        const float lineY = pitch + static_cast<float>(i) * panelH * 0.075f;
        const float lineW = panelW * (i == 0 ? 0.46f : 0.24f);
        addUiRectRotated(vertices, {cx, cy}, roll, -lineW * 0.5f, lineY, lineW, 0.0032f, i == 0 ? green : dim);
    }

    addUiRect(vertices, cx - panelW * 0.055f, cy - 0.002f, panelW * 0.040f, 0.004f, green);
    addUiRect(vertices, cx + panelW * 0.015f, cy - 0.002f, panelW * 0.040f, 0.004f, green);
    addUiRect(vertices, cx - 0.002f, cy - panelH * 0.055f, 0.004f, panelH * 0.035f, green);
    addUiRect(vertices, cx - 0.002f, cy + panelH * 0.020f, 0.004f, panelH * 0.035f, green);

    const float tickY = y + panelH * 0.10f;
    for (int i = 0; i < 7; ++i) {
        const float tx = x + panelW * (0.20f + static_cast<float>(i) * 0.10f);
        addUiRect(vertices, tx, tickY, 0.0035f, i == 3 ? 0.024f : 0.014f, dim);
    }

    const float blink = 0.55f + 0.45f * std::sin(time * 8.0f);
    addUiRect(vertices, x + panelW * 0.07f, y + panelH * 0.08f, panelW * 0.070f, 0.004f, {0.95f, 0.18f, 0.08f, 0.45f + blink * 0.35f});
    addUiRect(vertices, x + panelW * 0.07f, y + panelH * 0.08f, 0.004f, panelH * 0.070f, {0.95f, 0.18f, 0.08f, 0.45f + blink * 0.35f});
    addUiRect(vertices, x + panelW * 0.86f, y + panelH * 0.08f, panelW * 0.070f, 0.004f, green);
    addUiRect(vertices, x + panelW * 0.926f, y + panelH * 0.08f, 0.004f, panelH * 0.070f, green);
    addUiRect(vertices, x + panelW * 0.07f, y + panelH * 0.916f, panelW * 0.070f, 0.004f, green);
    addUiRect(vertices, x + panelW * 0.07f, y + panelH * 0.850f, 0.004f, panelH * 0.070f, green);
    addUiRect(vertices, x + panelW * 0.86f, y + panelH * 0.916f, panelW * 0.070f, 0.004f, green);
    addUiRect(vertices, x + panelW * 0.926f, y + panelH * 0.850f, 0.004f, panelH * 0.070f, green);

    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void drawSelection(const LineUniforms& lineUniforms, GLuint lineVao, GLuint lineVbo, const Mat4& vp, Vec3 hit, Vec3 previous, float progress, bool building, float time) {
    const float x0 = hit.x - 0.015f;
    const float y0 = hit.y - 0.015f;
    const float z0 = hit.z - 0.015f;
    const float x1 = hit.x + 1.015f;
    const float y1 = hit.y + 1.015f;
    const float z1 = hit.z + 1.015f;
    const std::array<Vec3, 8> p{{{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}};
    const std::array<int, 24> e{{0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7}};
    std::vector<float> data;
    data.reserve(3 * 96);
    auto addLine = [&data](Vec3 a, Vec3 b) {
        data.push_back(a.x); data.push_back(a.y); data.push_back(a.z);
        data.push_back(b.x); data.push_back(b.y); data.push_back(b.z);
    };
    for (int i = 0; i < 24; i += 2) addLine(p[e[i]], p[e[i + 1]]);

    Vec3 normal{
        std::clamp(previous.x - hit.x, -1.0f, 1.0f),
        std::clamp(previous.y - hit.y, -1.0f, 1.0f),
        std::clamp(previous.z - hit.z, -1.0f, 1.0f)
    };
    if (length(normal) < 0.1f) normal = {0.0f, 1.0f, 0.0f};

    const float inset = 0.12f;
    Vec3 f0{}, f1{}, f2{}, f3{};
    if (std::abs(normal.x) > 0.5f) {
        const float x = normal.x > 0.0f ? x1 + 0.012f : x0 - 0.012f;
        f0 = {x, y0 + inset, z0 + inset};
        f1 = {x, y1 - inset, z0 + inset};
        f2 = {x, y1 - inset, z1 - inset};
        f3 = {x, y0 + inset, z1 - inset};
    } else if (std::abs(normal.y) > 0.5f) {
        const float y = normal.y > 0.0f ? y1 + 0.012f : y0 - 0.012f;
        f0 = {x0 + inset, y, z0 + inset};
        f1 = {x1 - inset, y, z0 + inset};
        f2 = {x1 - inset, y, z1 - inset};
        f3 = {x0 + inset, y, z1 - inset};
    } else {
        const float z = normal.z > 0.0f ? z1 + 0.012f : z0 - 0.012f;
        f0 = {x0 + inset, y0 + inset, z};
        f1 = {x1 - inset, y0 + inset, z};
        f2 = {x1 - inset, y1 - inset, z};
        f3 = {x0 + inset, y1 - inset, z};
    }
    addLine(f0, f1);
    addLine(f1, f2);
    addLine(f2, f3);
    addLine(f3, f0);

    const std::array<Vec3, 4> ring{{f0, f1, f2, f3}};
    float remaining = std::clamp(progress, 0.0f, 1.0f) * 4.0f;
    for (int i = 0; i < 4; ++i) {
        const float piece = std::clamp(remaining, 0.0f, 1.0f);
        if (piece > 0.0f) addLine(ring[i], ring[i] + (ring[(i + 1) % 4] - ring[i]) * piece);
        remaining -= 1.0f;
    }

    glUseProgram(lineUniforms.program);
    glUniformMatrix4fv(lineUniforms.mvp, 1, GL_FALSE, vp.m);
    const float pulse = 0.65f + 0.35f * std::sin(time * 4.0f);
    if (building) glUniform4f(lineUniforms.color, 0.55f, 0.72f + pulse * 0.16f, 0.82f, 0.72f + pulse * 0.23f);
    else glUniform4f(lineUniforms.color, 0.95f, 0.78f + pulse * 0.18f, 0.52f, 0.72f + pulse * 0.23f);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    glBindVertexArray(lineVao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.size() / 3));
    glLineWidth(1.0f);
}

void drawVoxelScene(const VoxelUniforms& uniforms, const Mat4& vp, const Mesh& opaque, const Mesh& transparentMesh, const Mesh* satellite, Vec3 satellitePosition, const Mesh* rocketMesh, Vec3 rocketPosition, Vec3 rocketDirection, Vec3 rocketUp, Vec3 camera, Vec3 headlampDir, float time, float feedClarity = 0.0f, bool drawTransparent = true) {
    glUseProgram(uniforms.program);
    glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, vp.m);
    Mat4 model = identity();
    glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, model.m);
    glUniform3f(uniforms.camera, camera.x, camera.y, camera.z);
    glUniform3f(uniforms.headlampDir, headlampDir.x, headlampDir.y, headlampDir.z);
    glUniform3f(uniforms.sunDir, -0.18f, -0.72f, -0.35f);
    glUniform3f(uniforms.fogColor, 0.12f, 0.105f, 0.095f);
    glUniform1f(uniforms.time, time);
    glUniform1f(uniforms.feedClarity, feedClarity);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindVertexArray(opaque.vao);
    glDrawArrays(GL_TRIANGLES, 0, opaque.count);

    if (satellite && satellite->count > 0) {
        model = translation(satellitePosition);
        glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, model.m);
        glBindVertexArray(satellite->vao);
        glDrawArrays(GL_TRIANGLES, 0, satellite->count);
        model = identity();
        glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, model.m);
    }

    if (rocketMesh && rocketMesh->count > 0) {
        model = transformFromBasis(rocketPosition, rocketDirection, rocketUp);
        glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, model.m);
        glBindVertexArray(rocketMesh->vao);
        glDrawArrays(GL_TRIANGLES, 0, rocketMesh->count);
        model = identity();
        glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, model.m);
    }

    if (drawTransparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBindVertexArray(transparentMesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, transparentMesh.count);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void drawForcefield(const ForcefieldUniforms& uniforms, const Mesh& forcefield, const Mat4& vp, float time, float feedClarity = 0.0f) {
    if (forcefield.count <= 0) return;

    glUseProgram(uniforms.program);
    glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, vp.m);
    glUniform1f(uniforms.time, time);
    glUniform1f(uniforms.feedClarity, feedClarity);

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(forcefield.vao);
    glDrawArrays(GL_TRIANGLES, 0, forcefield.count);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void drawOrbitTrail(const World& world, const LineUniforms& lineUniforms, GLuint lineVao, GLuint lineVbo, const Mat4& vp, float time, SatelliteOrbit orbit, float orbitHeight) {
    std::vector<float> data;
    constexpr int Segments = 64;
    data.reserve(Segments * 3);
    const float baseAngle = time * 0.22f;
    for (int i = 0; i < Segments; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(Segments)) * Pi * 2.0f;
        const float orbitTime = (baseAngle + angle) / 0.22f;
        const Vec3 p = satellitePositionAt(world, orbitTime, orbit, orbitHeight);
        data.push_back(p.x);
        data.push_back(p.y);
        data.push_back(p.z);
    }

    glUseProgram(lineUniforms.program);
    glUniformMatrix4fv(lineUniforms.mvp, 1, GL_FALSE, vp.m);
    glUniform4f(lineUniforms.color, 0.86f, 0.18f, 0.08f, 0.36f);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    glBindVertexArray(lineVao);
    glLineWidth(1.5f);
    glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(data.size() / 3));
    glLineWidth(1.0f);
}

void drawBlast(const LineUniforms& lineUniforms, GLuint lineVao, GLuint lineVbo, const Mat4& vp, const Blast& blast) {
    if (!blast.active) return;
    const float lifetime = blast.atomic ? 1.9f : 1.35f;
    const float t = std::clamp(blast.age / lifetime, 0.0f, 1.0f);
    const float radius = (blast.atomic ? 2.0f : 1.2f) + t * (blast.atomic ? 16.0f : 9.0f);
    std::vector<float> data;
    data.reserve(3 * 220);
    auto addLine = [&data](Vec3 a, Vec3 b) {
        data.push_back(a.x); data.push_back(a.y); data.push_back(a.z);
        data.push_back(b.x); data.push_back(b.y); data.push_back(b.z);
    };
    constexpr int Segments = 36;
    for (int i = 0; i < Segments; ++i) {
        const float a0 = static_cast<float>(i) / static_cast<float>(Segments) * Pi * 2.0f;
        const float a1 = static_cast<float>(i + 1) / static_cast<float>(Segments) * Pi * 2.0f;
        addLine(blast.position + Vec3{std::cos(a0) * radius, std::sin(a0) * radius, 0.0f},
                blast.position + Vec3{std::cos(a1) * radius, std::sin(a1) * radius, 0.0f});
        addLine(blast.position + Vec3{std::cos(a0) * radius, 0.0f, std::sin(a0) * radius},
                blast.position + Vec3{std::cos(a1) * radius, 0.0f, std::sin(a1) * radius});
    }
    for (int i = 0; i < 12; ++i) {
        const float a = static_cast<float>(i) / 12.0f * Pi * 2.0f;
        const Vec3 dir = normalize({std::cos(a), std::sin(a * 1.7f) * 0.45f, std::sin(a)});
        addLine(blast.position, blast.position + dir * (radius * 1.28f));
    }

    glUseProgram(lineUniforms.program);
    glUniformMatrix4fv(lineUniforms.mvp, 1, GL_FALSE, vp.m);
    if (blast.atomic) glUniform4f(lineUniforms.color, 0.90f, 1.0f, 0.05f, (1.0f - t) * 0.98f);
    else glUniform4f(lineUniforms.color, 1.0f, 0.32f + (1.0f - t) * 0.45f, 0.06f, (1.0f - t) * 0.95f);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    glBindVertexArray(lineVao);
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.size() / 3));
    glLineWidth(1.0f);
}

void drawPlanetCubeWireframe(const LineUniforms& lineUniforms, GLuint lineVao, GLuint lineVbo, const Mat4& vp) {
    constexpr float min = 0.0f;
    constexpr float max = static_cast<float>(WorldSize);
    const std::array<Vec3, 8> corners{{
        {min, min, min}, {max, min, min}, {max, max, min}, {min, max, min},
        {min, min, max}, {max, min, max}, {max, max, max}, {min, max, max}
    }};
    const std::array<int, 24> edges{{0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7}};
    std::vector<float> data;
    data.reserve(edges.size() * 3);
    for (int index : edges) {
        data.push_back(corners[index].x);
        data.push_back(corners[index].y);
        data.push_back(corners[index].z);
    }

    glUseProgram(lineUniforms.program);
    glUniformMatrix4fv(lineUniforms.mvp, 1, GL_FALSE, vp.m);
    glUniform4f(lineUniforms.color, 0.92f, 0.98f, 0.72f, 0.88f);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    glBindVertexArray(lineVao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.size() / 3));
    glLineWidth(1.0f);
}

void drawSatellitePlayerTracker(const LineUniforms& lineUniforms, GLuint lineVao, GLuint lineVbo, const Mat4& vp, Vec3 playerPosition, float upSign, std::array<float, 4> color) {
    const Vec3 head = playerPosition + Vec3{0.0f, upSign * (PlayerHeight + 0.35f), 0.0f};
    const Vec3 center = head + Vec3{0.0f, upSign * 0.55f, 0.0f};
    const float radius = 1.15f;
    const float height = 2.65f;
    std::vector<float> data;
    data.reserve(3 * 180);
    auto addLine = [&data](Vec3 a, Vec3 b) {
        data.push_back(a.x); data.push_back(a.y); data.push_back(a.z);
        data.push_back(b.x); data.push_back(b.y); data.push_back(b.z);
    };

    constexpr int Segments = 24;
    for (int i = 0; i < Segments; ++i) {
        const float a0 = static_cast<float>(i) / static_cast<float>(Segments) * Pi * 2.0f;
        const float a1 = static_cast<float>(i + 1) / static_cast<float>(Segments) * Pi * 2.0f;
        addLine(center + Vec3{std::cos(a0) * radius, 0.0f, std::sin(a0) * radius},
                center + Vec3{std::cos(a1) * radius, 0.0f, std::sin(a1) * radius});
    }

    addLine(center + Vec3{-radius * 1.65f, 0.0f, 0.0f}, center + Vec3{radius * 1.65f, 0.0f, 0.0f});
    addLine(center + Vec3{0.0f, 0.0f, -radius * 1.65f}, center + Vec3{0.0f, 0.0f, radius * 1.65f});
    addLine(head, center + Vec3{0.0f, upSign * height, 0.0f});

    const Vec3 top = center + Vec3{0.0f, upSign * height, 0.0f};
    addLine(top, top + Vec3{radius * 0.65f, upSign * 0.75f, 0.0f});
    addLine(top, top + Vec3{-radius * 0.65f, upSign * 0.75f, 0.0f});
    addLine(top, top + Vec3{0.0f, upSign * 0.75f, radius * 0.65f});
    addLine(top, top + Vec3{0.0f, upSign * 0.75f, -radius * 0.65f});

    glUseProgram(lineUniforms.program);
    glUniformMatrix4fv(lineUniforms.mvp, 1, GL_FALSE, vp.m);
    glUniform4f(lineUniforms.color, color[0], color[1], color[2], color[3]);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    glBindVertexArray(lineVao);
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.size() / 3));
    glLineWidth(1.0f);
}

void drawSatelliteFeed(const TextureUniforms& textureUniforms, GLuint texture, int width, int height, float time) {
    const float pixels = std::clamp(static_cast<float>(height) * 0.58f, 360.0f, 520.0f);
    const float margin = 18.0f;
    const float x0 = (static_cast<float>(width) - pixels - margin) / static_cast<float>(width);
    const float y0 = (static_cast<float>(height) - pixels - margin) / static_cast<float>(height);
    const float x1 = (static_cast<float>(width) - margin) / static_cast<float>(width);
    const float y1 = (static_cast<float>(height) - margin) / static_cast<float>(height);
    glUseProgram(textureUniforms.program);
    glUniform4f(textureUniforms.rect, x0, y0, x1, y1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureUniforms.texture, 0);
    glUniform1f(textureUniforms.time, time);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void drawMissileFeed(const MissileFeedUniforms& uniforms, GLuint texture, int width, int height) {
    const float pixels = std::clamp(static_cast<float>(height) * 0.58f, 360.0f, 520.0f);
    const float margin = 18.0f;
    const float x0 = (static_cast<float>(width) - pixels - margin) / static_cast<float>(width);
    const float y0 = (static_cast<float>(height) - pixels - margin) / static_cast<float>(height);
    const float x1 = (static_cast<float>(width) - margin) / static_cast<float>(width);
    const float y1 = (static_cast<float>(height) - margin) / static_cast<float>(height);
    glUseProgram(uniforms.program);
    glUniform4f(uniforms.rect, x0, y0, x1, y1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(uniforms.texture, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void cursorCallback(GLFWwindow*, double xpos, double ypos) {
    if (!mouseCaptured) return;
    if (firstMouse) {
        lastMouse = {static_cast<float>(xpos), static_cast<float>(ypos)};
        firstMouse = false;
    }
    const float dx = static_cast<float>(xpos) - lastMouse.x;
    const float dy = static_cast<float>(ypos) - lastMouse.y;
    lastMouse = {static_cast<float>(xpos), static_cast<float>(ypos)};
    pendingMouseLook.x += dx;
    pendingMouseLook.y += dy;
}

bool keyPressed(int key) {
    static std::array<int, 512> previous{};
    const int current = glfwGetKey(window, key);
    const bool pressed = current == GLFW_PRESS && previous[key] != GLFW_PRESS;
    previous[key] = current;
    return pressed;
}

bool gamepadState(GLFWgamepadstate& state) {
    return glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &state);
}

bool gamepadButtonPressed(int button) {
    static std::array<unsigned char, GLFW_GAMEPAD_BUTTON_LAST + 1> previous{};
    GLFWgamepadstate state{};
    if (!gamepadState(state)) {
        previous.fill(GLFW_RELEASE);
        return false;
    }
    const bool pressed = state.buttons[button] == GLFW_PRESS && previous[button] != GLFW_PRESS;
    previous[button] = state.buttons[button];
    return pressed;
}

bool gamepadButtonDown(int button) {
    GLFWgamepadstate state{};
    return gamepadState(state) && state.buttons[button] == GLFW_PRESS;
}

float gamepadAxis(int axis) {
    GLFWgamepadstate state{};
    if (!gamepadState(state)) return 0.0f;
    const float value = state.axes[axis];
    return std::abs(value) > 0.18f ? value : 0.0f;
}

bool gamepadTriggerPressed(int axis) {
    static std::array<bool, GLFW_GAMEPAD_AXIS_LAST + 1> previous{};
    const bool down = gamepadAxis(axis) > 0.45f;
    const bool pressed = down && !previous[axis];
    previous[axis] = down;
    return pressed;
}

const char* voxelVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;
layout (location = 3) in float aKind;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vColor;
out float vKind;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    vKind = aKind;
    gl_Position = uMVP * world;
}
)GLSL";

const char* voxelFragmentShader = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vColor;
in float vKind;
uniform vec3 uCamera;
uniform vec3 uHeadlampDir;
uniform vec3 uSunDir;
uniform vec3 uFogColor;
uniform float uTime;
uniform float uFeedClarity;
out vec4 FragColor;

float hash(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.11, 0.17, 0.13));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash(i + vec3(0, 0, 0));
    float n100 = hash(i + vec3(1, 0, 0));
    float n010 = hash(i + vec3(0, 1, 0));
    float n110 = hash(i + vec3(1, 1, 0));
    float n001 = hash(i + vec3(0, 0, 1));
    float n101 = hash(i + vec3(1, 0, 1));
    float n011 = hash(i + vec3(0, 1, 1));
    float n111 = hash(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; ++i) {
        v += noise(p) * a;
        p *= 2.17;
        a *= 0.52;
    }
    return v;
}

float cracks(vec3 p) {
    vec3 cell = abs(fract(p) - 0.5);
    float line = 1.0 - smoothstep(0.015, 0.075, min(min(cell.x, cell.y), cell.z));
    float broken = smoothstep(0.42, 0.76, fbm(p * 0.7));
    return line * broken;
}

void main() {
    vec3 n = normalize(vNormal);
    float kind = floor(vKind + 0.5);
    vec3 p = vWorldPos;
    vec3 base = vColor.rgb;
    float alpha = vColor.a;

    if (kind == 1.0) {
        float fine = fbm(p * 6.5);
        float crack = cracks(p * 1.10);
        base *= mix(0.44, 1.18, fine);
        base = mix(base, vec3(0.030, 0.030, 0.031), crack * 0.78);
    } else if (kind == 2.0) {
        float medium = fbm(p * 2.2);
        float crack = cracks(p * 1.18);
        float clots = smoothstep(0.48, 0.86, medium);
        base = mix(base * 0.50, vec3(0.23, 0.20, 0.18), clots);
        base *= 1.0 - crack * 0.50;
    } else if (kind == 3.0) {
        float fine = fbm(p * 6.2);
        float crack = cracks(p * 1.18);
        base *= mix(0.32, 1.12, fine);
        base = mix(base, vec3(0.014, 0.014, 0.016), pow(crack, 1.25) * 0.94);
    } else if (kind == 4.0) {
        float fine = fbm(p * 5.5);
        float crack = cracks(p * 1.05);
        float grains = smoothstep(0.28, 0.92, fine);
        base = mix(vec3(0.20, 0.195, 0.185), vec3(0.58, 0.56, 0.50), grains);
        base *= 1.0 - crack * 0.54;
    } else if (kind == 7.0) {
        float fine = fbm(p * 7.0);
        float shard = pow(smoothstep(0.42, 1.0, fine), 2.0);
        base = mix(vec3(0.055, 0.060, 0.065), vec3(0.30, 0.34, 0.35), shard);
        alpha = 0.72;
    } else if (kind == 8.0) {
        float fine = fbm(p * 5.8);
        float crack = cracks(p * 1.0);
        float pores = smoothstep(0.55, 0.95, fine);
        base = mix(base * 0.58, vec3(0.78, 0.74, 0.62), pores);
        base *= 1.0 - crack * 0.44;
    } else if (kind == 9.0) {
        float medium = fbm(p * 2.0);
        float crack = cracks(p * 1.0);
        base = mix(vec3(0.025, 0.012, 0.018), vec3(0.42, 0.055, 0.075), medium);
        base = mix(vec3(0.018, 0.019, 0.022), vec3(0.18, 0.19, 0.21), medium);
        base *= 1.0 - crack * 0.46;
        alpha = 0.58;
    } else if (kind == 10.0) {
        float pulse = smoothstep(0.34, 0.92, fbm(p * 5.0));
        float vein = cracks(p * 0.82);
        base = mix(vec3(0.00, 0.18, 0.58), vec3(0.00, 0.95, 1.0), pulse);
        base += vec3(0.18, 0.90, 1.0) * vein * 0.85;
    } else if (kind == 11.0) {
        float grit = smoothstep(0.42, 0.88, fbm(p * 6.0));
        float vein = cracks(p * 0.95);
        base = mix(vec3(0.36, 0.48, 0.00), vec3(1.0, 1.0, 0.00), grit);
        base += vec3(0.95, 1.0, 0.06) * vein * 0.72;
    } else if (kind == 12.0) {
        float ripple = smoothstep(0.46, 0.80, fbm(vec3(p.x * 0.55, p.y * 0.16, p.z * 2.9)));
        float grains = fbm(p * 10.0);
        base = mix(vec3(0.42, 0.34, 0.20), vec3(0.86, 0.72, 0.43), ripple);
        base *= mix(0.82, 1.14, grains);
    } else if (kind == 13.0) {
        float strata = smoothstep(0.12, 0.16, abs(fract(p.y * 0.55 + fbm(p * 0.35) * 0.35) - 0.5));
        float weather = fbm(p * 4.0);
        base = mix(vec3(0.34, 0.25, 0.14), vec3(0.68, 0.52, 0.30), weather);
        base *= mix(0.72, 1.10, strata);
    } else if (kind == 14.0) {
        float rough = fbm(p * 5.2);
        float crack = cracks(p * 0.9);
        base = mix(vec3(0.17, 0.125, 0.085), vec3(0.44, 0.32, 0.19), rough);
        base *= 1.0 - crack * 0.46;
    } else if (kind == 15.0) {
        float blades = fbm(vec3(p.x * 6.0, p.y * 1.2, p.z * 6.0));
        base = mix(vec3(0.035, 0.20, 0.050), vec3(0.20, 0.62, 0.12), blades);
    } else if (kind == 16.0) {
        float loam = fbm(p * 5.0);
        float root = cracks(p * 0.72);
        base = mix(vec3(0.16, 0.095, 0.045), vec3(0.34, 0.22, 0.11), loam);
        base *= 1.0 - root * 0.28;
    } else if (kind == 17.0) {
        float rings = smoothstep(0.18, 0.72, fbm(vec3(p.x * 2.0, p.y * 8.0, p.z * 2.0)));
        base = mix(vec3(0.20, 0.10, 0.040), vec3(0.48, 0.27, 0.10), rings);
    } else if (kind == 18.0) {
        float leaf = fbm(p * 6.5);
        base = mix(vec3(0.030, 0.22, 0.035), vec3(0.16, 0.58, 0.11), leaf);
        alpha = 1.0;
    } else if (kind == 19.0) {
        float pores = fbm(p * 8.0);
        float stain = cracks(p * 0.85);
        base = mix(vec3(0.26, 0.26, 0.24), vec3(0.62, 0.62, 0.57), pores);
        base *= 1.0 - stain * 0.22;
    } else if (kind == 20.0) {
        float grime = fbm(p * 5.4);
        base = mix(vec3(0.09, 0.09, 0.085), vec3(0.28, 0.28, 0.26), grime);
    } else if (kind == 21.0) {
        float grit = fbm(p * 10.0);
        base = mix(vec3(0.025, 0.025, 0.024), vec3(0.12, 0.12, 0.11), grit);
    } else if (kind == 22.0) {
        float reflection = smoothstep(0.35, 0.86, fbm(p * 3.0));
        base = mix(vec3(0.035, 0.055, 0.065), vec3(0.28, 0.34, 0.36), reflection);
        alpha = 0.62;
    }

    float grey = dot(base, vec3(0.299, 0.587, 0.114));
    base = mix(vec3(grey), base, kind >= 10.0 ? 1.0 : 0.28);
    vec3 faceOffset = abs(vWorldPos - vec3(36.0));
    bool sideFace = max(faceOffset.x, faceOffset.z) > faceOffset.y;
    if (sideFace && kind < 10.0) {
        base = mix(base, vec3(0.025, 0.115, 0.070), 0.72);
    }

    vec3 local = abs(fract(p) - 0.5);
    float edge = 1.0 - smoothstep(0.38, 0.50, max(max(local.x, local.y), local.z));
    float faceAO = mix(0.72, 1.0, edge);
    faceAO *= 0.82 + 0.18 * clamp(n.y * 0.5 + 0.5, 0.0, 1.0);

    float sun = max(dot(n, normalize(-uSunDir)), 0.0);
    float sky = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    float rim = pow(max(1.0 - dot(normalize(uCamera - vWorldPos), n), 0.0), 2.0) * 0.08;
    vec3 light = vec3(0.12, 0.12, 0.13) + vec3(0.50, 0.51, 0.50) * sun * 0.42 + vec3(0.16, 0.17, 0.18) * sky * 0.18 + rim;
    light += vec3(0.18, 0.20, 0.19) * uFeedClarity;
    vec3 toPoint = vWorldPos - uCamera;
    float lampDistance = length(toPoint);
    vec3 lampRay = normalize(toPoint);
    float cone = smoothstep(0.82, 0.985, dot(lampRay, normalize(uHeadlampDir)));
    float spill = smoothstep(0.58, 0.86, dot(lampRay, normalize(uHeadlampDir))) * 0.22;
    float facing = max(dot(n, -lampRay), 0.0);
    float falloff = 1.0 / (1.0 + lampDistance * lampDistance * 0.030);
    vec3 headlamp = vec3(0.92, 0.94, 0.86) * (cone * 2.90 + spill * 2.0) * facing * falloff;
    light += headlamp;
    vec3 color = pow(base * light * faceAO, vec3(0.95));
    if (kind == 10.0) color += base * 1.15 + vec3(0.0, 0.35, 0.55) * edge;
    if (kind == 11.0) color += base * 0.95 + vec3(0.45, 0.55, 0.0) * edge;
    float distanceToEye = length(uCamera - vWorldPos);
    float distanceFog = smoothstep(18.0, 70.0, distanceToEye) * mix(1.0, 0.28, uFeedClarity);
    float lowFog = (1.0 - smoothstep(4.0, 24.0, vWorldPos.y)) * smoothstep(5.0, 45.0, distanceToEye) * mix(1.0, 0.18, uFeedClarity);
    float ashBands = smoothstep(0.38, 0.72, noise(vec3(vWorldPos.xz * 0.085, 3.0))) * distanceFog;
    vec3 hotHorizon = vec3(0.46, 0.085, 0.030);
    vec3 ashFog = mix(uFogColor, vec3(0.13, 0.12, 0.11), lowFog * 0.90 + ashBands * 0.45);
    vec3 fogColor = mix(ashFog, hotHorizon, smoothstep(24.0, 80.0, distanceToEye) * (1.0 - smoothstep(0.0, 18.0, vWorldPos.y)) * 0.32);
    float fog = clamp(distanceFog + lowFog * 0.56 + ashBands * 0.26, 0.0, 0.985);
    color = mix(color, fogColor, fog);
    if (kind == 10.0) color = mix(color, vec3(0.00, 0.92, 1.0), 0.34);
    if (kind == 11.0) color = mix(color, vec3(1.0, 1.0, 0.02), 0.30);
    color = mix(color, pow(color * 1.55, vec3(0.88)), uFeedClarity);
    FragColor = vec4(color, alpha);
}
)GLSL";

const char* satelliteFragmentShader = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vColor;
in float vKind;
uniform vec3 uCamera;
out vec4 FragColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 base = vColor.rgb;
    float kind = floor(vKind + 0.5);
    float grey = dot(base, vec3(0.299, 0.587, 0.114));
    base = mix(vec3(grey), base, kind >= 10.0 ? 1.0 : 0.05);
    vec3 faceOffset = abs(vWorldPos - vec3(36.0));
    bool sideFace = max(faceOffset.x, faceOffset.z) > faceOffset.y;
    if (sideFace && kind < 10.0) {
        base = mix(base, vec3(0.020, 0.130, 0.075), 0.78);
    }
    float light = 0.44 + max(dot(n, normalize(vec3(-0.35, 0.75, -0.25))), 0.0) * 0.48;
    float grid = 1.0 - smoothstep(0.45, 0.50, max(max(abs(fract(vWorldPos.x) - 0.5), abs(fract(vWorldPos.y) - 0.5)), abs(fract(vWorldPos.z) - 0.5)));
    vec3 color = base * light + grid * 0.030;
    if (kind == 10.0) color = mix(color, vec3(0.00, 0.92, 1.0), 0.72) + vec3(0.00, 0.45, 0.70) * grid;
    if (kind == 11.0) color = mix(color, vec3(1.0, 1.0, 0.02), 0.68) + vec3(0.50, 0.55, 0.00) * grid;
    if (kind >= 12.0 && kind <= 14.0) color = mix(color, base * 1.22, 0.38);
    float dist = length(uCamera - vWorldPos);
    color *= 1.0 - smoothstep(70.0, 145.0, dist) * 0.35;
    FragColor = vec4(color, 1.0);
}
)GLSL";

const char* forcefieldVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;
out vec3 vWorldPos;
void main() {
    vWorldPos = aPos;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char* forcefieldFragmentShader = R"GLSL(
#version 330 core
in vec3 vWorldPos;
uniform float uTime;
uniform float uFeedClarity;
out vec4 FragColor;

void main() {
    vec2 p = vWorldPos.xz;
    vec2 coarseCell = abs(fract(p * 0.36) - 0.5);
    vec2 fineCell = abs(fract(p * 1.18 + vec2(uTime * 0.42, -uTime * 0.31)) - 0.5);
    float coarseGrid = 1.0 - smoothstep(0.020, 0.060, min(coarseCell.x, coarseCell.y));
    float fineGrid = 1.0 - smoothstep(0.012, 0.035, min(fineCell.x, fineCell.y));
    float sweep = 1.0 - smoothstep(0.00, 0.11, abs(fract((p.x + p.y) * 0.050 - uTime * 0.75) - 0.5));
    vec2 edgeDistance = min(p + vec2(5.0), vec2(77.0) - p);
    float edgeGlow = 1.0 - smoothstep(0.0, 5.0, min(edgeDistance.x, edgeDistance.y));
    float flicker = 0.72 + 0.28 * step(0.5, fract(uTime * 10.0 + p.x * 0.07 + p.y * 0.11));
    float energy = max(coarseGrid * 0.70, fineGrid * 0.22);
    energy = max(energy, sweep * 0.42);
    energy = max(energy, edgeGlow * 0.62);
    energy *= flicker;
    vec3 color = mix(vec3(0.00, 0.18, 0.05), vec3(0.18, 1.0, 0.34), energy);
    color += vec3(0.22, 0.85, 0.28) * max(sweep, edgeGlow) * 0.38;
    float alpha = clamp(0.16 + energy * 0.46 + uFeedClarity * 0.12, 0.0, 0.72);
    FragColor = vec4(color, alpha);
}
)GLSL";

const char* skyVertexShader = R"GLSL(
#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* skyFragmentShader = R"GLSL(
#version 330 core
in vec2 vUV;
uniform float uTime;
uniform int uTheme;
out vec4 FragColor;
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += noise(p) * a;
        p *= 2.05;
        a *= 0.52;
    }
    return v;
}
void main() {
    vec2 uv = vUV;
    if (uTheme == 3) {
        float slow = uTime * 0.006;
        float smog = fbm(uv * vec2(7.0, 3.5) + vec2(slow, -slow * 0.4));
        float low = 1.0 - smoothstep(0.04, 0.58, uv.y);
        vec3 horizon = vec3(0.38, 0.36, 0.32);
        vec3 zenith = vec3(0.075, 0.078, 0.082);
        vec3 color = mix(horizon, zenith, smoothstep(0.0, 1.0, uv.y));
        color += vec3(0.16, 0.15, 0.13) * smog * low * 0.72;
        vec2 sunPos = vec2(0.72, 0.16);
        float sunDist = distance(uv, sunPos);
        color += vec3(0.48, 0.38, 0.22) * pow(max(0.0, 0.28 - sunDist), 2.0) * 2.2;
        FragColor = vec4(color, 1.0);
        return;
    }
    if (uTheme == 2) {
        float slow = uTime * 0.012;
        float cloud = fbm(uv * vec2(6.0, 3.0) + vec2(slow, 0.0));
        float haze = 1.0 - smoothstep(0.04, 0.62, uv.y);
        vec3 horizon = vec3(0.58, 0.76, 0.52);
        vec3 zenith = vec3(0.18, 0.34, 0.52);
        vec3 color = mix(horizon, zenith, smoothstep(0.0, 1.0, uv.y));
        color = mix(color, vec3(0.76, 0.86, 0.72), haze * 0.22);
        color += vec3(0.50, 0.58, 0.50) * smoothstep(0.60, 0.86, cloud) * smoothstep(0.25, 0.95, uv.y) * 0.20;
        vec2 sunPos = vec2(0.68, 0.20);
        float sunDist = distance(uv, sunPos);
        float glow = pow(max(0.0, 0.34 - sunDist), 2.0);
        color += vec3(0.88, 0.72, 0.36) * glow * 2.5;
        FragColor = vec4(color, 1.0);
        return;
    }
    if (uTheme == 1) {
        float slow = uTime * 0.010;
        float dust = fbm(uv * vec2(8.0, 3.2) + vec2(slow, slow * 0.35));
        float highDust = fbm(uv * vec2(18.0, 6.0) + vec2(-slow * 2.4, slow));
        float horizonBand = 1.0 - smoothstep(0.03, 0.55, uv.y);
        vec3 zenith = vec3(0.18, 0.13, 0.095);
        vec3 hotSky = vec3(0.82, 0.47, 0.18);
        vec3 dustColor = vec3(0.58, 0.39, 0.20);
        vec3 color = mix(hotSky, zenith, smoothstep(0.0, 1.0, uv.y));
        color = mix(color, dustColor, horizonBand * (0.35 + dust * 0.40));
        vec2 sunPos = vec2(0.70, 0.18);
        float sunDist = distance(uv, sunPos);
        float glare = pow(max(0.0, 0.44 - sunDist), 2.0);
        float disk = 1.0 - smoothstep(0.115, 0.135, sunDist);
        color += vec3(1.0, 0.67, 0.24) * glare * 3.0;
        color = mix(color, vec3(1.0, 0.78, 0.34), disk * 0.72);
        color += vec3(0.24, 0.13, 0.06) * smoothstep(0.48, 0.90, highDust) * horizonBand;
        FragColor = vec4(color, 1.0);
        return;
    }
    float slow = uTime * 0.018;
    float smoke = fbm(uv * vec2(5.5, 2.6) + vec2(slow, -slow * 0.7));
    float highSmoke = fbm(uv * vec2(13.0, 7.0) + vec2(-slow * 2.0, slow * 0.8));
    float horizonBand = 1.0 - smoothstep(0.06, 0.50, uv.y);
    float soot = smoothstep(0.22, 0.82, uv.y);
    float tornFire = smoothstep(0.48, 0.84, smoke + highSmoke * 0.42) * horizonBand;

    vec3 zenith = vec3(0.006, 0.006, 0.007);
    vec3 burnedRed = vec3(0.17, 0.038, 0.026);
    vec3 horizon = vec3(0.62, 0.12, 0.035);
    vec3 color = mix(horizon, burnedRed, smoothstep(0.0, 0.42, uv.y));
    color = mix(color, zenith, soot);
    color += vec3(0.72, 0.18, 0.045) * tornFire * 0.50;
    color += vec3(0.08, 0.075, 0.070) * smoothstep(0.45, 0.90, highSmoke) * (0.35 + soot * 0.85);

    vec2 sunPos = vec2(0.73, 0.13);
    float sunDist = distance(uv, sunPos);
    float sun = pow(max(0.0, 0.34 - sunDist), 2.0);
    float occlusion = smoothstep(0.36, 0.80, fbm(uv * 9.0 + vec2(slow * 1.7, 0.0)));
    color += vec3(0.95, 0.20, 0.045) * sun * 5.2 * (1.0 - occlusion * 0.68);
    float disk = 1.0 - smoothstep(0.135, 0.155, sunDist);
    float eclipse = 1.0 - smoothstep(0.125, 0.180, distance(uv, sunPos + vec2(-0.035, 0.018)));
    color = mix(color, vec3(0.020, 0.013, 0.010), disk * 0.62);
    color += vec3(0.90, 0.16, 0.030) * max(disk - eclipse, 0.0) * 0.70;
    float ridge = 0.075 + fbm(vec2(uv.x * 6.2, 0.21)) * 0.070 + noise(vec2(uv.x * 19.0, 4.0)) * 0.028;
    float farRidge = smoothstep(ridge + 0.018, ridge - 0.012, uv.y);
    float backRidge = smoothstep(ridge + 0.075, ridge + 0.030, uv.y) * 0.55;
    color = mix(color, vec3(0.016, 0.014, 0.013), backRidge);
    color = mix(color, vec3(0.004, 0.004, 0.004), farRidge);
    color = mix(color, vec3(0.018, 0.017, 0.017), smoothstep(0.66, 1.0, smoke) * 0.46);
    float vignette = 1.0 - smoothstep(0.18, 0.92, distance(uv, vec2(0.5, 0.48)));
    color *= mix(0.58, 1.08, vignette);
    color = pow(color, vec3(0.94, 1.02, 1.08));
    FragColor = vec4(color, 1.0);
}
)GLSL";

const char* lineVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char* lineFragmentShader = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)GLSL";

const char* uiVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

const char* uiFragmentShader = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)GLSL";

const char* textureVertexShader = R"GLSL(
#version 330 core
uniform vec4 uRect;
out vec2 vUV;
void main() {
    vec2 corners[6] = vec2[6](
        vec2(uRect.x, uRect.y), vec2(uRect.z, uRect.y), vec2(uRect.z, uRect.w),
        vec2(uRect.x, uRect.y), vec2(uRect.z, uRect.w), vec2(uRect.x, uRect.w)
    );
    vec2 uvs[6] = vec2[6](
        vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0)
    );
    vec2 p = corners[gl_VertexID];
    vUV = uvs[gl_VertexID];
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* textureFragmentShader = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uTime;
out vec4 FragColor;
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(41.7, 289.3))) * 15342.731);
}
void main() {
    vec2 uv = vUV;
    float tear = step(0.986, hash(vec2(floor(uv.y * 34.0), floor(uTime * 9.0)))) * 0.020;
    uv.x = clamp(uv.x + tear * sin(uv.y * 70.0 + uTime * 11.0), 0.0, 1.0);
    vec3 color = texture(uTexture, uv).rgb;
    vec2 edge = min(vUV, 1.0 - vUV);
    float frame = 1.0 - smoothstep(0.012, 0.026, min(edge.x, edge.y));
    float grey = dot(color, vec3(0.299, 0.587, 0.114));
    float row = floor(vUV.y * 95.0);
    float staticNoise = hash(vec2(floor(vUV.x * 120.0) + floor(uTime * 22.0), row));
    float scan = sin(vUV.y * 520.0) * 0.035;
    float band = smoothstep(0.035, 0.0, abs(fract(vUV.y * 4.0 + uTime * 0.35) - 0.5)) * 0.16;
    float dropout = step(0.975, hash(vec2(row, floor(uTime * 7.0)))) * 0.55;
    float signal = grey * 0.86 + staticNoise * 0.16 + scan - band - dropout;
    vec3 cctv = vec3(signal * 0.42, signal * 1.08, signal * 0.72);
    float vignette = smoothstep(0.78, 0.18, distance(vUV, vec2(0.5)));
    cctv *= 0.64 + vignette * 0.62;
    cctv = floor(max(cctv, vec3(0.0)) * 18.0) / 18.0;
    cctv = mix(cctv, vec3(0.004, 0.010, 0.006), frame * 0.84);
    FragColor = vec4(cctv, 1.0);
}
)GLSL";

const char* missileFeedFragmentShader = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec3 color = texture(uTexture, vUV).rgb;
    vec2 edge = min(vUV, 1.0 - vUV);
    float frame = 1.0 - smoothstep(0.010, 0.018, min(edge.x, edge.y));
    color = pow(max(color, vec3(0.0)), vec3(0.88)) * 1.18;
    color = mix(color, vec3(0.02, 0.025, 0.022), frame * 0.58);
    FragColor = vec4(color, 1.0);
}
)GLSL";

} // namespace

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);

    window = glfwCreateWindow(1280, 800, "", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    World world;
    world.seed = randomPlanetSeed();
    generateWorld(world);
    Player player;
    Player playerTwo;
    respawnPlayer(world, player);
    respawnPlayer(world, playerTwo, true);
    glfwSetWindowUserPointer(window, &player);
    glfwSetCursorPosCallback(window, cursorCallback);

    GLuint voxelProgram = makeProgram(voxelVertexShader, voxelFragmentShader);
    GLuint satelliteProgram = makeProgram(voxelVertexShader, satelliteFragmentShader);
    GLuint skyProgram = makeProgram(skyVertexShader, skyFragmentShader);
    GLuint lineProgram = makeProgram(lineVertexShader, lineFragmentShader);
    GLuint uiProgram = makeProgram(uiVertexShader, uiFragmentShader);
    GLuint textureProgram = makeProgram(textureVertexShader, textureFragmentShader);
    GLuint missileFeedProgram = makeProgram(textureVertexShader, missileFeedFragmentShader);
    GLuint forcefieldProgram = makeProgram(forcefieldVertexShader, forcefieldFragmentShader);
    const VoxelUniforms voxelUniform = voxelUniforms(voxelProgram);
    const VoxelUniforms satelliteUniform = voxelUniforms(satelliteProgram);
    const LineUniforms lineUniform = lineUniforms(lineProgram);
    const TextureUniforms textureUniform = textureUniforms(textureProgram);
    const MissileFeedUniforms missileFeedUniform = missileFeedUniforms(missileFeedProgram);
    const ForcefieldUniforms forcefieldUniform = forcefieldUniforms(forcefieldProgram);
    const GLint skyTimeUniform = glGetUniformLocation(skyProgram, "uTime");
    const GLint skyThemeUniform = glGetUniformLocation(skyProgram, "uTheme");
    GLuint emptyVao = 0;
    glGenVertexArrays(1, &emptyVao);
    GLuint lineVbo = 0;
    GLuint lineVao = makeLineVao(lineVbo);
    GLuint uiVbo = 0;
    GLuint uiVao = makeUiVao(uiVbo);
    SatelliteCamera satelliteCamera;
    SatelliteCamera satelliteCameraTwo;
    ensureSatelliteCamera(satelliteCamera);
    ensureSatelliteCamera(satelliteCameraTwo);

    Mesh opaque;
    Mesh transparentMesh;
    Mesh satelliteMesh;
    Mesh rocketMesh;
    Mesh forcefieldMesh;
    rebuildMeshes(world, opaque, transparentMesh);
    rebuildSatelliteMesh(satelliteMesh);
    rebuildRocketMesh(rocketMesh);
    float forcefieldY = forcefieldPlaneY(world);
    rebuildForcefieldMesh(world, forcefieldMesh);

    BuildType selectedBuild = BuildType::Normal;
    double lastTime = glfwGetTime();
    Vec3 miningTarget{-9999.0f, -9999.0f, -9999.0f};
    Vec3 buildingTarget{-9999.0f, -9999.0f, -9999.0f};
    float miningTimer = 0.0f;
    float buildingTimer = 0.0f;
    float buildCooldown = 0.0f;
    float toolHitCooldown = 0.0f;
    float toolHitCooldownTwo = 0.0f;
    SatelliteOrbit satelliteOrbit = SatelliteOrbit::Polar;
    float satelliteOrbitHeight = 56.0f;
    float targetSatelliteOrbitHeight = satelliteOrbitHeight;
    double lastSatelliteFeedTime = -1.0;
    double lastSatelliteFeedTimeTwo = -1.0;
    bool satelliteFeedDirty = true;
    ToolMode playerToolMode = ToolMode::MineBuild;
    ToolMode playerTwoToolMode = ToolMode::MineBuild;
    Rocket rocket;
    Rocket rocketTwo;
    Blast blast;
    Blast blastTwo;
    MatchState match;
    match.startedAt = lastTime;
    match.firstHunter = (world.seed & 1) ? 1 : 2;
    GameScreen screen = GameScreen::Title;
    P2Control p2Control = P2Control::Keyboard;
    int titleRow = 0;
    auto resetMatch = [&](double now) {
        player.score = 0;
        playerTwo.score = 0;
        respawnPlayer(world, player);
        respawnPlayer(world, playerTwo, true);
        player.health = 100;
        playerTwo.health = 100;
        player.fuel = 3;
        player.plutonium = 3;
        playerTwo.fuel = 3;
        playerTwo.plutonium = 3;
        match.startedAt = now;
        match.suddenDeath = false;
        match.over = false;
        match.winner = 0;
        match.firstHunter = ((world.seed + static_cast<int>(now * 10.0)) & 1) ? 1 : 2;
        rocket.active = false;
        rocketTwo.active = false;
        blast.active = false;
        blastTwo.active = false;
        satelliteFeedDirty = true;
    };
    auto startGame = [&](double now) {
        resetMatch(now);
        screen = GameScreen::Playing;
        mouseCaptured = true;
        firstMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    };
    auto rebuildSelectedWorld = [&](double now) {
        generateWorld(world);
        rebuildMeshes(world, opaque, transparentMesh);
        forcefieldY = forcefieldPlaneY(world);
        rebuildForcefieldMesh(world, forcefieldMesh);
        resetMatch(now);
    };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const float dt = std::min(static_cast<float>(now - lastTime), 1.0f / 30.0f);
        lastTime = now;

        glfwPollEvents();
        if (keyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
        if (screen == GameScreen::Title) {
            if (keyPressed(GLFW_KEY_UP) || keyPressed(GLFW_KEY_W) || gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_UP)) titleRow = (titleRow + 2) % 3;
            if (keyPressed(GLFW_KEY_DOWN) || keyPressed(GLFW_KEY_S) || gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_DOWN)) titleRow = (titleRow + 1) % 3;
            const bool previousChoice = keyPressed(GLFW_KEY_LEFT) || keyPressed(GLFW_KEY_A) || gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT);
            const bool nextChoice = keyPressed(GLFW_KEY_RIGHT) || keyPressed(GLFW_KEY_D) || gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT);
            if (previousChoice || nextChoice) {
                if (titleRow == 0) match.type = previousChoice ? previousGameType(match.type) : nextGameType(match.type);
                else if (titleRow == 1) {
                    world.theme = previousChoice ? previousTheme(world.theme) : nextTheme(world.theme);
                    world.seed += previousChoice ? 5297 : 7331;
                    rebuildSelectedWorld(now);
                } else {
                    p2Control = p2Control == P2Control::Keyboard ? P2Control::Gamepad : P2Control::Keyboard;
                }
            }
            if (keyPressed(GLFW_KEY_ENTER) || keyPressed(GLFW_KEY_SPACE) || gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_A) || gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_START)) {
                startGame(now);
            }

            int width = 1;
            int height = 1;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            glUseProgram(skyProgram);
            glUniform1f(skyTimeUniform, static_cast<float>(now));
            glUniform1i(skyThemeUniform, themeShaderValue(world.theme));
            glBindVertexArray(emptyVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawTitleScreen(uiProgram, uiVao, uiVbo, world.theme, match.type, p2Control, titleRow, static_cast<float>(now));
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glfwSetWindowTitle(window, "");
            glfwSwapBuffers(window);
            continue;
        }
        if (keyPressed(GLFW_KEY_F1)) {
            mouseCaptured = !mouseCaptured;
            firstMouse = true;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
        if (keyPressed(GLFW_KEY_1)) selectedBuild = BuildType::Normal;
        if (keyPressed(GLFW_KEY_2)) selectedBuild = BuildType::Hard;
        if (keyPressed(GLFW_KEY_G)) {
            match.type = nextGameType(match.type);
            resetMatch(now);
        }
        if (keyPressed(GLFW_KEY_T)) {
            world.theme = nextTheme(world.theme);
            world.seed += 7331;
            generateWorld(world);
            rebuildMeshes(world, opaque, transparentMesh);
            forcefieldY = forcefieldPlaneY(world);
            rebuildForcefieldMesh(world, forcefieldMesh);
            resetMatch(now);
        }
        if (keyPressed(GLFW_KEY_M)) playerToolMode = nextToolMode(playerToolMode);
        if (keyPressed(GLFW_KEY_P) || (p2Control == P2Control::Gamepad && gamepadButtonPressed(GLFW_GAMEPAD_BUTTON_Y))) playerTwoToolMode = nextToolMode(playerTwoToolMode);
        if (keyPressed(GLFW_KEY_MINUS)) {
            targetSatelliteOrbitHeight = std::max(16.0f, targetSatelliteOrbitHeight - 8.0f);
            satelliteFeedDirty = true;
        }
        if (keyPressed(GLFW_KEY_EQUAL)) {
            targetSatelliteOrbitHeight = std::min(140.0f, targetSatelliteOrbitHeight + 8.0f);
            satelliteFeedDirty = true;
        }
        if (keyPressed(GLFW_KEY_O)) {
            satelliteOrbit = nextOrbit(satelliteOrbit);
            satelliteFeedDirty = true;
        }
        if (keyPressed(GLFW_KEY_R)) {
            world.seed += 1337;
            generateWorld(world);
            rebuildMeshes(world, opaque, transparentMesh);
            forcefieldY = forcefieldPlaneY(world);
            rebuildForcefieldMesh(world, forcefieldMesh);
            resetMatch(now);
        }

        satelliteOrbitHeight += (targetSatelliteOrbitHeight - satelliteOrbitHeight) * std::min(1.0f, dt * 2.4f);
        if (std::abs(targetSatelliteOrbitHeight - satelliteOrbitHeight) > 0.05f) satelliteFeedDirty = true;

        const double matchElapsed = now - match.startedAt;
        if (!match.over && !match.suddenDeath) {
            if (match.type == GameType::FirstToThree && matchElapsed >= 600.0 && player.score < 3 && playerTwo.score < 3) {
                match.suddenDeath = true;
                player.health = 1;
                playerTwo.health = 1;
            } else if (match.type == GameType::PhasedTurns && matchElapsed >= 600.0) {
                if (player.score != playerTwo.score) {
                    match.over = true;
                    match.winner = player.score > playerTwo.score ? 1 : 2;
                } else {
                    match.suddenDeath = true;
                    player.health = 1;
                    playerTwo.health = 1;
                }
            }
        }
        if (!match.over && match.suddenDeath) {
            player.health = std::min(player.health, 1);
            playerTwo.health = std::min(playerTwo.health, 1);
        }

        int activeHunter = 0;
        if (match.type == GameType::AlternatingHunter) {
            const int turn = std::max(0, static_cast<int>(matchElapsed / 60.0));
            activeHunter = (turn % 2) == 0 ? match.firstHunter : 3 - match.firstHunter;
        }
        const bool phasedBuildTurn = match.type == GameType::PhasedTurns && !match.suddenDeath && !match.over
            && (std::clamp(static_cast<int>(matchElapsed / 60.0), 0, 9) % 2) == 0;
        const bool phasedAttackTurn = match.type == GameType::PhasedTurns && !match.suddenDeath && !match.over && !phasedBuildTurn;
        const bool p1CanAct = !match.over && (match.type != GameType::AlternatingHunter || activeHunter == 1);
        const bool p2CanAct = !match.over && (match.type != GameType::AlternatingHunter || activeHunter == 2);
        const bool p1CanMove = p1CanAct;
        const bool p2CanMove = p2CanAct;
        const bool p1CanMineBuild = p1CanAct && (match.type != GameType::PhasedTurns || match.suddenDeath || phasedBuildTurn);
        const bool p1CanAttack = p1CanAct && (match.type != GameType::PhasedTurns || match.suddenDeath || phasedAttackTurn);
        const bool p2CanAttack = p2CanAct && (match.type != GameType::PhasedTurns || match.suddenDeath || phasedAttackTurn);

        const bool missileGuided = rocket.active && rocket.age >= 1.0f;
        updatePlayer(world, player, dt, forcefieldY, p1CanMove && !missileGuided, p1CanMove);
        updatePlayerTwo(world, playerTwo, dt, forcefieldY, p2CanMove, p2Control);
        const Vec3 eye{player.position.x, player.position.y + player.upSign * PlayerHeight * 0.88f, player.position.z};
        const Vec3 look = forwardFromAngles(player.yaw, player.pitch);
        const Vec3 satellitePosition = satellitePositionAt(world, static_cast<float>(now), satelliteOrbit, satelliteOrbitHeight);
        const Vec3 satellitePositionTwo = satellitePositionAt(world, static_cast<float>(now) + Pi / 0.22f, SatelliteOrbit::OppositeFace, satelliteOrbitHeight);

        Vec3 hit{};
        Vec3 previous{};
        bool hasHit = raycastBlock(world, eye, look, 8.0f, hit, previous);
        const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        static bool wasRocketFireDown = false;
        const bool playerMissileMode = playerToolMode == ToolMode::Missile || playerToolMode == ToolMode::AtomicMissile;
        const bool rocketFirePressed = p1CanAttack && playerMissileMode && leftDown && !wasRocketFireDown;
        wasRocketFireDown = p1CanAttack && playerMissileMode && leftDown;
        player.rocketCooldown = std::max(0.0f, player.rocketCooldown - dt);
        playerTwo.rocketCooldown = std::max(0.0f, playerTwo.rocketCooldown - dt);
        player.hurtTimer = std::max(0.0f, player.hurtTimer - dt);
        playerTwo.hurtTimer = std::max(0.0f, playerTwo.hurtTimer - dt);
        toolHitCooldown = std::max(0.0f, toolHitCooldown - dt);
        toolHitCooldownTwo = std::max(0.0f, toolHitCooldownTwo - dt);
        const int scoreBeforeCombatOne = player.score;
        const int scoreBeforeCombatTwo = playerTwo.score;
        const bool playerAtomic = playerToolMode == ToolMode::AtomicMissile;
        if (rocketFirePressed && !rocket.active && canFireRocket(player, playerAtomic) && player.rocketCooldown <= 0.0f) {
            spendRocketResources(player, playerAtomic);
            player.rocketCooldown = playerAtomic ? 2.4f : 1.25f;
            fireRocket(rocket, 1, playerAtomic, eye, look, player.yaw, player.pitch);
            satelliteFeedDirty = true;
        }
        static bool wasPlayerTwoFireDown = false;
        const bool playerTwoMissileMode = playerTwoToolMode == ToolMode::Missile || playerTwoToolMode == ToolMode::AtomicMissile;
        const bool playerTwoFireDown = p2Control == P2Control::Gamepad
            ? (gamepadButtonDown(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER) || gamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.45f)
            : glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        const bool playerTwoFirePressed = p2CanAttack && playerTwoMissileMode && playerTwoFireDown && !wasPlayerTwoFireDown;
        wasPlayerTwoFireDown = p2CanAttack && playerTwoMissileMode && playerTwoFireDown;
        const Vec3 eyeTwoForFire{playerTwo.position.x, playerTwo.position.y + playerTwo.upSign * PlayerHeight * 0.88f, playerTwo.position.z};
        const Vec3 lookTwoForFire = forwardFromAngles(playerTwo.yaw, playerTwo.pitch);
        const bool playerTwoAtomic = playerTwoToolMode == ToolMode::AtomicMissile;
        if (playerTwoFirePressed && !rocketTwo.active && canFireRocket(playerTwo, playerTwoAtomic) && playerTwo.rocketCooldown <= 0.0f) {
            spendRocketResources(playerTwo, playerTwoAtomic);
            playerTwo.rocketCooldown = playerTwoAtomic ? 2.4f : 1.25f;
            fireRocket(rocketTwo, 2, playerTwoAtomic, eyeTwoForFire, lookTwoForFire, playerTwo.yaw, playerTwo.pitch);
            satelliteFeedDirty = true;
        }
        const bool rocketWasActive = rocket.active;
        updateRocket(world, rocket, blast, dt, rocket.active && rocket.age >= 1.0f);
        if (rocketWasActive && !rocket.active && blast.active && blast.age == 0.0f) {
            const float crater = blast.atomic ? 9.5f : 5.8f;
            const float damageRadius = blast.atomic ? 13.0f : 7.5f;
            collectResourcesInBlast(world, player, blast.position, crater);
            applyBlastDamage(world, playerTwo, true, player, blast.position, damageRadius);
            applyBlastDamage(world, player, false, playerTwo, blast.position, blast.atomic ? 8.0f : 5.0f);
            if (destroyBlocksInBlast(world, blast.position, crater)) {
                rebuildMeshes(world, opaque, transparentMesh);
                satelliteFeedDirty = true;
            }
        }
        const bool rocketTwoWasActive = rocketTwo.active;
        updateRocket(world, rocketTwo, blastTwo, dt, false);
        if (rocketTwoWasActive && !rocketTwo.active && blastTwo.active && blastTwo.age == 0.0f) {
            const float crater = blastTwo.atomic ? 9.5f : 5.8f;
            const float damageRadius = blastTwo.atomic ? 13.0f : 7.5f;
            collectResourcesInBlast(world, playerTwo, blastTwo.position, crater);
            applyBlastDamage(world, player, false, playerTwo, blastTwo.position, damageRadius);
            applyBlastDamage(world, playerTwo, true, player, blastTwo.position, blastTwo.atomic ? 8.0f : 5.0f);
            if (destroyBlocksInBlast(world, blastTwo.position, crater)) {
                rebuildMeshes(world, opaque, transparentMesh);
                satelliteFeedDirty = true;
            }
        }
        updateBlast(blast, dt);
        updateBlast(blastTwo, dt);
        const bool rocketBlocksTools = playerMissileMode || rocket.active;

        buildCooldown = std::max(0.0f, buildCooldown - dt);
        if (p1CanAttack && !rocketBlocksTools && mouseCaptured && (leftDown || rightDown) && toolHitCooldown <= 0.0f && rayHitsPlayer(eye, look, playerTwo, 4.0f)) {
            applyToolDamage(world, playerTwo, true, player, rightDown ? 18 : 12);
            toolHitCooldown = 0.42f;
        }
        const bool playerTwoToolSwing = playerTwoToolMode == ToolMode::MineBuild && playerTwoFireDown;
        if (p2CanAttack && playerTwoToolSwing && toolHitCooldownTwo <= 0.0f && rayHitsPlayer(eyeTwoForFire, lookTwoForFire, player, 4.0f)) {
            applyToolDamage(world, player, false, playerTwo, 12);
            toolHitCooldownTwo = 0.42f;
        }
        if (p1CanMineBuild && !rocketBlocksTools && mouseCaptured && hasHit && leftDown) {
            if (!sameBlockCell(miningTarget, hit)) {
                miningTarget = hit;
                miningTimer = 0.0f;
            }
            miningTimer += dt;
            const Block targetBlock = world.get(static_cast<int>(hit.x), static_cast<int>(hit.y), static_cast<int>(hit.z));
            if (miningTimer >= mineDuration(targetBlock)) {
                collectResource(player, targetBlock);
                world.set(static_cast<int>(hit.x), static_cast<int>(hit.y), static_cast<int>(hit.z), Block::Air);
                rebuildMeshes(world, opaque, transparentMesh);
                satelliteFeedDirty = true;
                miningTimer = 0.0f;
                miningTarget = {-9999.0f, -9999.0f, -9999.0f};
            }
        } else {
            miningTimer = 0.0f;
            miningTarget = {-9999.0f, -9999.0f, -9999.0f};
        }

        if (p1CanMineBuild && !rocketBlocksTools && mouseCaptured && hasHit && rightDown) {
            const int x = static_cast<int>(previous.x);
            const int y = static_cast<int>(previous.y);
            const int z = static_cast<int>(previous.z);
            if (world.inBounds(x, y, z)) {
                if (!sameBlockCell(buildingTarget, previous)) {
                    buildingTarget = previous;
                    buildingTimer = 0.0f;
                }
                if (buildCooldown <= 0.0f && world.get(x, y, z) == Block::Air) {
                    buildingTimer += dt;
                    if (buildingTimer >= buildDuration(selectedBuild)) {
                        world.set(x, y, z, blockForBuildType(selectedBuild, world.theme));
                        if (playerCollides(world, player.position)) world.set(x, y, z, Block::Air);
                        rebuildMeshes(world, opaque, transparentMesh);
                        satelliteFeedDirty = true;
                        buildCooldown = 0.18f;
                        buildingTimer = 0.0f;
                    }
                }
            }
        } else {
            buildingTimer = 0.0f;
            buildingTarget = {-9999.0f, -9999.0f, -9999.0f};
        }

        if (!match.over && (player.score != scoreBeforeCombatOne || playerTwo.score != scoreBeforeCombatTwo)) {
            if (match.type == GameType::FirstToThree) {
                if (player.score >= 3 || (match.suddenDeath && player.score > scoreBeforeCombatOne)) {
                    match.over = true;
                    match.winner = 1;
                } else if (playerTwo.score >= 3 || (match.suddenDeath && playerTwo.score > scoreBeforeCombatTwo)) {
                    match.over = true;
                    match.winner = 2;
                }
            } else if (match.type == GameType::PhasedTurns) {
                if (match.suddenDeath) {
                    match.over = true;
                    match.winner = player.score > scoreBeforeCombatOne ? 1 : 2;
                }
            } else if (match.type == GameType::AlternatingHunter) {
                match.over = true;
                match.winner = player.score > scoreBeforeCombatOne ? 1 : 2;
            }
            if (match.over) {
                rocket.active = false;
                rocketTwo.active = false;
            }
        }

        int width = 1;
        int height = 1;
        glfwGetFramebufferSize(window, &width, &height);
        const SatelliteView liveSatelliteView = makeSatelliteView(satellitePosition);
        const SatelliteView liveSatelliteViewTwo = makeSatelliteView(satellitePositionTwo);

        const bool missileCameraActive = rocket.active && rocket.age >= 1.0f;
        if (satelliteFeedDirty || missileCameraActive || now - lastSatelliteFeedTime >= 0.12) {
            resizeSatelliteCamera(satelliteCamera, missileCameraActive ? satelliteCamera.missileSize : satelliteCamera.satelliteSize);
            glBindFramebuffer(GL_FRAMEBUFFER, satelliteCamera.framebuffer);
            glViewport(0, 0, satelliteCamera.size, satelliteCamera.size);
            glClearColor(0.018f, 0.014f, 0.012f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (missileCameraActive) {
                const Vec3 missileEye = rocket.position + rocket.direction * 0.35f;
                const Vec3 missileTarget = missileEye + rocket.direction;
                glDisable(GL_DEPTH_TEST);
                glUseProgram(skyProgram);
                glUniform1f(skyTimeUniform, static_cast<float>(now));
                glUniform1i(skyThemeUniform, themeShaderValue(world.theme));
                glBindVertexArray(emptyVao);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glEnable(GL_DEPTH_TEST);
                const Mat4 missileProj = perspective(58.0f * Pi / 180.0f, 1.0f, 0.05f, 220.0f);
                const Mat4 missileView = lookAt(missileEye, missileTarget, rocket.up);
                const Mat4 missileVp = multiply(missileProj, missileView);
                drawVoxelScene(voxelUniform, missileVp, opaque, transparentMesh, nullptr, {}, nullptr, {}, {}, {}, missileEye, rocket.direction, static_cast<float>(now), 1.0f, true);
                drawForcefield(forcefieldUniform, forcefieldMesh, missileVp, static_cast<float>(now), 1.0f);
            } else {
                const SatelliteView satelliteView = makeSatelliteView(satellitePosition);
                drawVoxelScene(satelliteUniform, satelliteView.vp, opaque, transparentMesh, nullptr, {}, nullptr, {}, {}, {}, satelliteView.position, satelliteView.down, static_cast<float>(now), 0.0f, false);
                drawForcefield(forcefieldUniform, forcefieldMesh, satelliteView.vp, static_cast<float>(now), 0.45f);
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                drawPlanetCubeWireframe(lineUniform, lineVao, lineVbo, satelliteView.vp);
                drawSatellitePlayerTracker(lineUniform, lineVao, lineVbo, satelliteView.vp, player.position, player.upSign, {1.0f, 0.30f, 0.08f, 0.95f});
                drawSatellitePlayerTracker(lineUniform, lineVao, lineVbo, satelliteView.vp, playerTwo.position, playerTwo.upSign, {0.15f, 0.55f, 1.0f, 0.95f});
                glDisable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            lastSatelliteFeedTime = now;
            satelliteFeedDirty = false;
        }
        if (satelliteFeedDirty || now - lastSatelliteFeedTimeTwo >= 0.12) {
            resizeSatelliteCamera(satelliteCameraTwo, satelliteCameraTwo.satelliteSize);
            glBindFramebuffer(GL_FRAMEBUFFER, satelliteCameraTwo.framebuffer);
            glViewport(0, 0, satelliteCameraTwo.size, satelliteCameraTwo.size);
            glClearColor(0.018f, 0.014f, 0.012f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const SatelliteView satelliteViewTwo = makeSatelliteView(satellitePositionTwo);
            drawVoxelScene(satelliteUniform, satelliteViewTwo.vp, opaque, transparentMesh, nullptr, {}, nullptr, {}, {}, {}, satelliteViewTwo.position, satelliteViewTwo.down, static_cast<float>(now), 0.0f, false);
            drawForcefield(forcefieldUniform, forcefieldMesh, satelliteViewTwo.vp, static_cast<float>(now), 0.45f);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawPlanetCubeWireframe(lineUniform, lineVao, lineVbo, satelliteViewTwo.vp);
            drawSatellitePlayerTracker(lineUniform, lineVao, lineVbo, satelliteViewTwo.vp, player.position, player.upSign, {1.0f, 0.30f, 0.08f, 0.95f});
            drawSatellitePlayerTracker(lineUniform, lineVao, lineVbo, satelliteViewTwo.vp, playerTwo.position, playerTwo.upSign, {0.15f, 0.55f, 1.0f, 0.95f});
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            lastSatelliteFeedTimeTwo = now;
        }
        const int leftWidth = std::max(1, width / 2);
        const int rightWidth = std::max(1, width - leftWidth);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glViewport(0, 0, leftWidth, height);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(skyProgram);
        glUniform1f(skyTimeUniform, static_cast<float>(now));
        glUniform1i(skyThemeUniform, themeShaderValue(world.theme));
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glEnable(GL_DEPTH_TEST);

        const Mat4 proj = perspective(68.0f * Pi / 180.0f, static_cast<float>(leftWidth) / static_cast<float>(height), 0.05f, 140.0f);
        const Mat4 view = lookAt(eye, eye + look, {0.0f, player.upSign, 0.0f});
        const Mat4 vp = multiply(proj, view);

        const Rocket& visibleRocket = rocket.active ? rocket : rocketTwo;
        drawVoxelScene(voxelUniform, vp, opaque, transparentMesh, &satelliteMesh, satellitePosition, visibleRocket.active ? &rocketMesh : nullptr, visibleRocket.position, visibleRocket.direction, visibleRocket.up, eye, look, static_cast<float>(now));
        drawForcefield(forcefieldUniform, forcefieldMesh, vp, static_cast<float>(now));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawOrbitTrail(world, lineUniform, lineVao, lineVbo, vp, static_cast<float>(now), satelliteOrbit, satelliteOrbitHeight);
        drawBlast(lineUniform, lineVao, lineVbo, vp, blast);
        drawBlast(lineUniform, lineVao, lineVbo, vp, blastTwo);
        glDisable(GL_BLEND);

        float interactionProgress = 0.0f;
        bool showingBuild = false;
        bool showingMine = false;
        if (p1CanMineBuild && !rocketBlocksTools && hasHit) {
            if (rightDown && sameBlockCell(buildingTarget, previous)) {
                interactionProgress = std::clamp(buildingTimer / buildDuration(selectedBuild), 0.0f, 1.0f);
                showingBuild = true;
            } else if (leftDown && sameBlockCell(miningTarget, hit)) {
                const Block targetBlock = world.get(static_cast<int>(hit.x), static_cast<int>(hit.y), static_cast<int>(hit.z));
                interactionProgress = std::clamp(miningTimer / mineDuration(targetBlock), 0.0f, 1.0f);
                showingMine = true;
            }
            drawSelection(lineUniform, lineVao, lineVbo, vp, hit, previous, interactionProgress, showingBuild, static_cast<float>(now));
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(emptyVao);
        if (missileCameraActive) drawMissileFeed(missileFeedUniform, satelliteCamera.color, leftWidth, height);
        else drawSatelliteFeed(textureUniform, satelliteCamera.color, leftWidth, height, static_cast<float>(now));
        if (missileCameraActive) drawMissileHud(uiProgram, uiVao, uiVbo, rocket, leftWidth, height, static_cast<float>(now));
        else drawSatellitePositionLabels(uiProgram, uiVao, uiVbo, liveSatelliteView.vp, player, playerTwo, leftWidth, height);
        if (playerMissileMode || rocket.active) drawReticle(uiProgram, uiVao, uiVbo, rocket.active && rocket.age >= 1.0f, static_cast<float>(now));
        else drawSmallReticle(uiProgram, uiVao, uiVbo);
        drawPlayerStatus(uiProgram, uiVao, uiVbo, player);
        drawHand(uiProgram, uiVao, uiVbo, showingMine, showingBuild, playerMissileMode, interactionProgress, static_cast<float>(now));
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        glViewport(leftWidth, 0, rightWidth, height);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(skyProgram);
        glUniform1f(skyTimeUniform, static_cast<float>(now));
        glUniform1i(skyThemeUniform, themeShaderValue(world.theme));
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glEnable(GL_DEPTH_TEST);

        const Vec3 eyeTwo{playerTwo.position.x, playerTwo.position.y + playerTwo.upSign * PlayerHeight * 0.88f, playerTwo.position.z};
        const Vec3 lookTwo = forwardFromAngles(playerTwo.yaw, playerTwo.pitch);
        const Mat4 projTwo = perspective(68.0f * Pi / 180.0f, static_cast<float>(rightWidth) / static_cast<float>(height), 0.05f, 140.0f);
        const Mat4 viewTwo = lookAt(eyeTwo, eyeTwo + lookTwo, {0.0f, playerTwo.upSign, 0.0f});
        const Mat4 vpTwo = multiply(projTwo, viewTwo);
        drawVoxelScene(voxelUniform, vpTwo, opaque, transparentMesh, &satelliteMesh, satellitePositionTwo, visibleRocket.active ? &rocketMesh : nullptr, visibleRocket.position, visibleRocket.direction, visibleRocket.up, eyeTwo, lookTwo, static_cast<float>(now));
        drawForcefield(forcefieldUniform, forcefieldMesh, vpTwo, static_cast<float>(now));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawOrbitTrail(world, lineUniform, lineVao, lineVbo, vpTwo, static_cast<float>(now) + Pi / 0.22f, SatelliteOrbit::OppositeFace, satelliteOrbitHeight);
        drawBlast(lineUniform, lineVao, lineVbo, vpTwo, blast);
        drawBlast(lineUniform, lineVao, lineVbo, vpTwo, blastTwo);
        glDisable(GL_BLEND);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(emptyVao);
        drawSatelliteFeed(textureUniform, satelliteCameraTwo.color, rightWidth, height, static_cast<float>(now));
        drawSatellitePositionLabels(uiProgram, uiVao, uiVbo, liveSatelliteViewTwo.vp, player, playerTwo, rightWidth, height);
        drawSmallReticle(uiProgram, uiVao, uiVbo, true);
        drawPlayerStatus(uiProgram, uiVao, uiVbo, playerTwo, true);
        drawHand(uiProgram, uiVao, uiVbo, false, false, playerTwoMissileMode, 0.0f, static_cast<float>(now), true);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int modeSecondsRemaining = 0;
        if (!match.over && !match.suddenDeath) {
            if (match.type == GameType::FirstToThree) modeSecondsRemaining = std::max(0, 600 - static_cast<int>(matchElapsed));
            else modeSecondsRemaining = std::max(0, 60 - static_cast<int>(static_cast<int>(matchElapsed) % 60));
        }
        glfwSetWindowTitle(window, "");
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
