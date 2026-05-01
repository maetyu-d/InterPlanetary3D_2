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
    Plutonium
};

enum class BuildType {
    Normal,
    Hard
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

struct World {
    std::vector<Block> blocks;
    int seed = 0;

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
    float yaw = 0.0f;
    float pitch = -0.12f;
    float lookVelocityX = 0.0f;
    float lookVelocityY = 0.0f;
    bool grounded = false;
    float coyoteTimer = 0.0f;
    float jumpBufferTimer = 0.0f;
};

GLFWwindow* window = nullptr;
bool mouseCaptured = true;
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
    return block == Block::Air || block == Block::DeadCrystal || block == Block::ObsidianGlass;
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
        case Block::Air: c = {0.0f, 0.0f, 0.0f, 0.0f}; break;
    }
    c[0] = std::clamp(c[0] + speckle, 0.0f, 1.0f);
    c[1] = std::clamp(c[1] + speckle, 0.0f, 1.0f);
    c[2] = std::clamp(c[2] + speckle, 0.0f, 1.0f);
    return c;
}

Block blockForBuildType(BuildType type) {
    return type == BuildType::Normal ? Block::Ash : Block::Basalt;
}

const char* buildTypeName(BuildType type) {
    return type == BuildType::Normal ? "normal" : "hard";
}

float buildDuration(BuildType type) {
    return type == BuildType::Normal ? NormalBuildTime : HardBuildTime;
}

float mineDuration(Block block) {
    switch (block) {
        case Block::Ash: return 0.42f;
        case Block::BloodSoil: return 0.50f;
        case Block::Basalt: return 1.15f;
        case Block::PaleStone: return 0.85f;
        case Block::DeadCrystal: return 0.70f;
        case Block::Bone: return 0.62f;
        case Block::ObsidianGlass: return 1.35f;
        case Block::Fuel: return 1.05f;
        case Block::Plutonium: return 1.45f;
        case Block::Air: break;
    }
    return 0.5f;
}

bool sameBlockCell(Vec3 a, Vec3 b) {
    return static_cast<int>(a.x) == static_cast<int>(b.x)
        && static_cast<int>(a.y) == static_cast<int>(b.y)
        && static_cast<int>(a.z) == static_cast<int>(b.z);
}

int faceTerrainHeight(int u, int v, int face, int seed) {
    const float nx = static_cast<float>(u) / 22.0f;
    const float nz = static_cast<float>(v) / 22.0f;
    const float continent = fractalNoise(nx, nz, seed + face * 503);
    const float ridges = std::abs(fractalNoise(nx * 2.45f + 32.0f, nz * 2.45f - 8.0f, seed + face * 503 + 7) - 0.5f) * 2.0f;
    const float pits = fractalNoise(nx * 3.0f - 11.0f, nz * 3.0f + 19.0f, seed + face * 503 + 31);
    const float crags = std::abs(fractalNoise(nx * 5.0f - 17.0f, nz * 5.0f + 29.0f, seed + face * 503 + 83) - 0.5f) * 2.0f;
    const float scars = fractalNoise(nx * 9.0f + 3.0f, nz * 9.0f - 6.0f, seed + face * 503 + 151);
    int height = 2 + static_cast<int>(continent * 18.0f + ridges * 18.0f + crags * 10.0f - pits * 9.0f);
    if (scars > 0.68f) height += 5;
    if (scars < 0.24f) height -= 4;
    height = (height / 2) * 2;
    return std::clamp(height, 2, FaceRelief);
}

Block cubeFaceMaterial(int depth, int height, int u, int v, int face, int seed) {
    const float vein = fractalNoise((u + depth * 3) / 7.0f, (v - depth * 2) / 7.0f, seed + face * 503 + 99);
    const float resourceA = fractalNoise((u + face * 19) / 5.5f, (v + depth * 2) / 5.5f, seed + face * 701 + 211);
    const float resourceB = fractalNoise((u - depth * 3) / 6.5f, (v + face * 23) / 6.5f, seed + face * 701 + 409);
    if (depth >= 5 && depth <= 11) {
        const float fleck = hash2(u + depth * 13, v - depth * 7, seed + face * 997);
        if (resourceB > 0.835f && fleck > 0.74f) return Block::Plutonium;
        if (resourceA > 0.815f && fleck > 0.64f) return Block::Fuel;
    }
    if (depth == 0) {
        if (height < 10) return hash2(u, v, seed + face * 17 + 91) > 0.68f ? Block::ObsidianGlass : Block::Ash;
        if (height > FaceRelief - 9) return Block::Bone;
        return hash2(u, v, seed + face * 17 + 144) > 0.66f ? Block::Ash : Block::BloodSoil;
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
                for (int y = std::max(0, surface - shell); y <= surface; ++y) world.set(u, y, v, cubeFaceMaterial(surface - y, height, u, v, face, world.seed));
            }
            break;
        case 1:
            {
                const int surface = lowSurface;
                for (int y = 0; y < surface; ++y) world.set(u, y, v, Block::Air);
                for (int y = surface; y <= std::min(WorldHeight - 1, surface + shell); ++y) world.set(u, y, v, cubeFaceMaterial(y - surface, height, u, v, face, world.seed));
            }
            break;
        case 2:
            {
                const int surface = highSurface;
                for (int x = surface + 1; x < WorldSize; ++x) world.set(x, u, v, Block::Air);
                for (int x = std::max(0, surface - shell); x <= surface; ++x) world.set(x, u, v, cubeFaceMaterial(surface - x, height, u, v, face, world.seed));
            }
            break;
        case 3:
            {
                const int surface = lowSurface;
                for (int x = 0; x < surface; ++x) world.set(x, u, v, Block::Air);
                for (int x = surface; x <= std::min(WorldSize - 1, surface + shell); ++x) world.set(x, u, v, cubeFaceMaterial(x - surface, height, u, v, face, world.seed));
            }
            break;
        case 4:
            {
                const int surface = highSurface;
                for (int z = surface + 1; z < WorldSize; ++z) world.set(u, v, z, Block::Air);
                for (int z = std::max(0, surface - shell); z <= surface; ++z) world.set(u, v, z, cubeFaceMaterial(surface - z, height, u, v, face, world.seed));
            }
            break;
        case 5:
            {
                const int surface = lowSurface;
                for (int z = 0; z < surface; ++z) world.set(u, v, z, Block::Air);
                for (int z = surface; z <= std::min(WorldSize - 1, surface + shell); ++z) world.set(u, v, z, cubeFaceMaterial(z - surface, height, u, v, face, world.seed));
            }
            break;
    }
}

void generateWorld(World& world) {
    std::fill(world.blocks.begin(), world.blocks.end(), Block::Air);

    constexpr std::array<int, 6> faceOrder{{1, 2, 3, 4, 5, 0}};
    for (int face : faceOrder) {
        for (int v = 0; v < WorldSize; ++v) {
            for (int u = 0; u < WorldSize; ++u) {
                setCubeFaceColumn(world, face, u, v, faceTerrainHeight(u, v, face, world.seed));
            }
        }
    }

    for (int z = 4; z < WorldSize - 4; ++z) {
        for (int x = 4; x < WorldSize - 4; ++x) {
            int y = WorldHeight - 1;
            while (y > 1 && world.get(x, y, z) == Block::Air) --y;
            const Block surface = world.get(x, y, z);
            if ((surface == Block::Ash || surface == Block::ObsidianGlass) && hash2(x, z, world.seed + 70) > 0.996f && world.inBounds(x, y + 1, z)) {
                world.set(x, y + 1, z, Block::DeadCrystal);
            } else if ((surface == Block::Ash || surface == Block::BloodSoil) && hash2(x, z, world.seed + 64) > 0.988f && world.inBounds(x, y + 1, z)) {
                world.set(x, y + 1, z, Block::Bone);
            }
        }
    }
}

Vec3 spawnPosition(const World& world) {
    const int x = WorldSize / 2;
    const int z = WorldSize / 2;
    for (int y = WorldHeight - 1; y >= 0; --y) {
        if (solid(world.get(x, y, z))) {
            return {x + 0.5f, y + 1.2f, z + 0.5f};
        }
    }
    return {WorldSize / 2.0f, WorldHeight + 4.0f, WorldSize / 2.0f};
}

void respawnPlayer(const World& world, Player& player) {
    player.position = spawnPosition(world);
    player.velocity = {};
    player.grounded = false;
    player.coyoteTimer = 0.0f;
    player.jumpBufferTimer = 0.0f;
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
                if (block == Block::DeadCrystal || block == Block::ObsidianGlass) appendCube(transparentVertices, world, x, y, z, block);
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

float approach(float value, float target, float maxDelta) {
    if (value < target) return std::min(value + maxDelta, target);
    return std::max(value - maxDelta, target);
}

bool moveAxis(const World& world, Player& player, float& coord, float delta) {
    coord += delta;
    if (playerCollides(world, player.position)) {
        coord -= delta;
        if (&coord == &player.position.y) player.velocity.y = 0.0f;
        return true;
    }
    return false;
}

void updatePlayer(const World& world, Player& player, float dt) {
    player.lookVelocityX = player.lookVelocityX * 0.42f + pendingMouseLook.x * 0.58f;
    player.lookVelocityY = player.lookVelocityY * 0.42f + pendingMouseLook.y * 0.58f;
    pendingMouseLook = {};
    player.yaw -= player.lookVelocityX * 0.0021f;
    player.pitch -= player.lookVelocityY * 0.0021f;
    player.pitch = std::clamp(player.pitch, -1.45f, 1.45f);

    Vec3 forward = normalize(forwardFromAngles(player.yaw, 0.0f));
    Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
    Vec3 input{};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) input = input + forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) input = input - forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) input = input + right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) input = input - right;
    if (length(input) > 0.0f) input = normalize(input);

    static bool wasJumpDown = false;
    const bool jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (jumpDown && !wasJumpDown) {
        player.jumpBufferTimer = JumpBufferTime;
    } else {
        player.jumpBufferTimer = std::max(0.0f, player.jumpBufferTimer - dt);
    }
    wasJumpDown = jumpDown;

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
        player.velocity.y = JumpSpeed;
        player.grounded = false;
        player.coyoteTimer = 0.0f;
        player.jumpBufferTimer = 0.0f;
    }

    player.velocity.y -= Gravity * dt;
    player.velocity.y = std::max(player.velocity.y, -32.0f);

    moveAxis(world, player, player.position.x, player.velocity.x * dt);
    moveAxis(world, player, player.position.z, player.velocity.z * dt);
    const float beforeY = player.position.y;
    const float verticalVelocity = player.velocity.y;
    const bool hitVertical = moveAxis(world, player, player.position.y, player.velocity.y * dt);
    player.grounded = hitVertical && verticalVelocity <= 0.0f && beforeY > player.position.y - 0.0001f;

    if (player.position.y < -4.0f) {
        respawnPlayer(world, player);
    }
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

GLuint makeLineVao(GLuint& vbo) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 96, nullptr, GL_DYNAMIC_DRAW);
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(UiVertex) * 256, nullptr, GL_DYNAMIC_DRAW);
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

void drawHand(GLuint uiProgram, GLuint uiVao, GLuint uiVbo, bool mining, bool building, float progress, float time) {
    const float action = (mining || building) ? std::sin(std::clamp(progress, 0.0f, 1.0f) * Pi) : 0.0f;
    const float idle = std::sin(time * 1.7f) * 0.006f;
    const float sx = -0.015f - action * 0.030f;
    const float sy = idle + action * 0.030f;
    const float swing = -0.50f - action * 0.28f;
    std::vector<UiVertex> vertices;
    vertices.reserve(72);

    const Vec2 pivot{0.73f + sx, 0.73f + sy};
    addUiRectRotated(vertices, pivot, swing, -0.018f, -0.015f, 0.036f, 0.250f, {0.32f, 0.21f, 0.11f, 1.0f});
    addUiRectRotated(vertices, pivot, swing, -0.026f, 0.070f, 0.052f, 0.030f, {0.20f, 0.13f, 0.07f, 1.0f});
    const std::array<float, 4> toolColor = building ? std::array<float, 4>{0.38f, 0.40f, 0.38f, 1.0f} : std::array<float, 4>{0.05f, 0.60f, 0.58f, 1.0f};
    const std::array<float, 4> toolShadow = building ? std::array<float, 4>{0.18f, 0.19f, 0.19f, 1.0f} : std::array<float, 4>{0.02f, 0.28f, 0.29f, 1.0f};
    addUiRectRotated(vertices, pivot, swing, -0.105f, -0.060f, 0.180f, 0.050f, toolShadow);
    addUiRectRotated(vertices, pivot, swing, -0.085f, -0.080f, 0.115f, 0.060f, toolColor);

    addUiRect(vertices, 0.75f + sx, 0.76f + sy, 0.24f, 0.14f, {0.055f, 0.048f, 0.044f, 0.94f});
    addUiRect(vertices, 0.72f + sx, 0.70f + sy, 0.21f, 0.14f, {0.34f, 0.27f, 0.22f, 1.0f});
    addUiRect(vertices, 0.79f + sx, 0.66f + sy, 0.12f, 0.10f, {0.62f, 0.50f, 0.40f, 1.0f});
    addUiRect(vertices, 0.89f + sx, 0.66f + sy, 0.052f, 0.074f, {0.70f, 0.56f, 0.44f, 1.0f});

    glUseProgram(uiProgram);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)), vertices.data());
    glBindVertexArray(uiVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void drawSelection(GLuint lineProgram, GLuint lineVao, GLuint lineVbo, const Mat4& vp, Vec3 hit, Vec3 previous, float progress, bool building, float time) {
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

    glUseProgram(lineProgram);
    glUniformMatrix4fv(glGetUniformLocation(lineProgram, "uMVP"), 1, GL_FALSE, vp.m);
    const float pulse = 0.65f + 0.35f * std::sin(time * 4.0f);
    if (building) glUniform4f(glGetUniformLocation(lineProgram, "uColor"), 0.55f, 0.72f + pulse * 0.16f, 0.82f, 0.72f + pulse * 0.23f);
    else glUniform4f(glGetUniformLocation(lineProgram, "uColor"), 0.95f, 0.78f + pulse * 0.18f, 0.52f, 0.72f + pulse * 0.23f);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    glBindVertexArray(lineVao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.size() / 3));
    glLineWidth(1.0f);
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
    gl_Position = uMVP * vec4(aPos, 1.0);
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
        base = mix(vec3(0.01, 0.14, 0.38), vec3(0.08, 0.82, 1.0), pulse);
        base += vec3(0.12, 0.70, 1.0) * vein * 0.45;
    } else if (kind == 11.0) {
        float grit = smoothstep(0.42, 0.88, fbm(p * 6.0));
        float vein = cracks(p * 0.95);
        base = mix(vec3(0.25, 0.32, 0.02), vec3(0.92, 1.0, 0.04), grit);
        base += vec3(0.80, 1.0, 0.10) * vein * 0.35;
    }

    float grey = dot(base, vec3(0.299, 0.587, 0.114));
    base = mix(vec3(grey), base, kind >= 10.0 ? 0.94 : 0.28);

    vec3 local = abs(fract(p) - 0.5);
    float edge = 1.0 - smoothstep(0.38, 0.50, max(max(local.x, local.y), local.z));
    float faceAO = mix(0.72, 1.0, edge);
    faceAO *= 0.82 + 0.18 * clamp(n.y * 0.5 + 0.5, 0.0, 1.0);

    float sun = max(dot(n, normalize(-uSunDir)), 0.0);
    float sky = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    float rim = pow(max(1.0 - dot(normalize(uCamera - vWorldPos), n), 0.0), 2.0) * 0.08;
    vec3 light = vec3(0.12, 0.12, 0.13) + vec3(0.50, 0.51, 0.50) * sun * 0.42 + vec3(0.16, 0.17, 0.18) * sky * 0.18 + rim;
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
    if (kind == 10.0) color += base * 0.38;
    if (kind == 11.0) color += base * 0.30;
    float distanceToEye = length(uCamera - vWorldPos);
    float distanceFog = smoothstep(18.0, 70.0, distanceToEye);
    float lowFog = (1.0 - smoothstep(4.0, 24.0, vWorldPos.y)) * smoothstep(5.0, 45.0, distanceToEye);
    float ashBands = smoothstep(0.38, 0.72, noise(vec3(vWorldPos.xz * 0.085, 3.0))) * distanceFog;
    vec3 hotHorizon = vec3(0.46, 0.085, 0.030);
    vec3 ashFog = mix(uFogColor, vec3(0.13, 0.12, 0.11), lowFog * 0.90 + ashBands * 0.45);
    vec3 fogColor = mix(ashFog, hotHorizon, smoothstep(24.0, 80.0, distanceToEye) * (1.0 - smoothstep(0.0, 18.0, vWorldPos.y)) * 0.32);
    float fog = clamp(distanceFog + lowFog * 0.56 + ashBands * 0.26, 0.0, 0.985);
    color = mix(color, fogColor, fog);
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

} // namespace

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window = glfwCreateWindow(1280, 800, "Dead Cube World - WASD move, mouse look, click build/mine, R regenerate, F1 mouse", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    World world;
    world.seed = randomPlanetSeed();
    generateWorld(world);
    Player player;
    respawnPlayer(world, player);
    glfwSetWindowUserPointer(window, &player);
    glfwSetCursorPosCallback(window, cursorCallback);

    GLuint voxelProgram = makeProgram(voxelVertexShader, voxelFragmentShader);
    GLuint skyProgram = makeProgram(skyVertexShader, skyFragmentShader);
    GLuint lineProgram = makeProgram(lineVertexShader, lineFragmentShader);
    GLuint uiProgram = makeProgram(uiVertexShader, uiFragmentShader);
    GLuint emptyVao = 0;
    glGenVertexArrays(1, &emptyVao);
    GLuint lineVbo = 0;
    GLuint lineVao = makeLineVao(lineVbo);
    GLuint uiVbo = 0;
    GLuint uiVao = makeUiVao(uiVbo);

    Mesh opaque;
    Mesh transparentMesh;
    rebuildMeshes(world, opaque, transparentMesh);

    BuildType selectedBuild = BuildType::Normal;
    double lastTime = glfwGetTime();
    Vec3 miningTarget{-9999.0f, -9999.0f, -9999.0f};
    Vec3 buildingTarget{-9999.0f, -9999.0f, -9999.0f};
    float miningTimer = 0.0f;
    float buildingTimer = 0.0f;
    float buildCooldown = 0.0f;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const float dt = std::min(static_cast<float>(now - lastTime), 1.0f / 30.0f);
        lastTime = now;

        glfwPollEvents();
        if (keyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
        if (keyPressed(GLFW_KEY_F1)) {
            mouseCaptured = !mouseCaptured;
            firstMouse = true;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
        if (keyPressed(GLFW_KEY_1)) selectedBuild = BuildType::Normal;
        if (keyPressed(GLFW_KEY_2)) selectedBuild = BuildType::Hard;
        if (keyPressed(GLFW_KEY_R)) {
            world.seed += 1337;
            generateWorld(world);
            rebuildMeshes(world, opaque, transparentMesh);
            respawnPlayer(world, player);
        }

        updatePlayer(world, player, dt);
        const Vec3 eye{player.position.x, player.position.y + PlayerHeight * 0.88f, player.position.z};
        const Vec3 look = forwardFromAngles(player.yaw, player.pitch);

        Vec3 hit{};
        Vec3 previous{};
        bool hasHit = raycastBlock(world, eye, look, 8.0f, hit, previous);
        const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        buildCooldown = std::max(0.0f, buildCooldown - dt);
        if (mouseCaptured && hasHit && leftDown) {
            if (!sameBlockCell(miningTarget, hit)) {
                miningTarget = hit;
                miningTimer = 0.0f;
            }
            miningTimer += dt;
            const Block targetBlock = world.get(static_cast<int>(hit.x), static_cast<int>(hit.y), static_cast<int>(hit.z));
            if (miningTimer >= mineDuration(targetBlock)) {
                world.set(static_cast<int>(hit.x), static_cast<int>(hit.y), static_cast<int>(hit.z), Block::Air);
                rebuildMeshes(world, opaque, transparentMesh);
                miningTimer = 0.0f;
                miningTarget = {-9999.0f, -9999.0f, -9999.0f};
            }
        } else {
            miningTimer = 0.0f;
            miningTarget = {-9999.0f, -9999.0f, -9999.0f};
        }

        if (mouseCaptured && hasHit && rightDown) {
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
                        world.set(x, y, z, blockForBuildType(selectedBuild));
                        if (playerCollides(world, player.position)) world.set(x, y, z, Block::Air);
                        rebuildMeshes(world, opaque, transparentMesh);
                        buildCooldown = 0.18f;
                        buildingTimer = 0.0f;
                    }
                }
            }
        } else {
            buildingTimer = 0.0f;
            buildingTarget = {-9999.0f, -9999.0f, -9999.0f};
        }

        int width = 1;
        int height = 1;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glUseProgram(skyProgram);
        glUniform1f(glGetUniformLocation(skyProgram, "uTime"), static_cast<float>(now));
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glEnable(GL_DEPTH_TEST);

        const Mat4 proj = perspective(68.0f * Pi / 180.0f, static_cast<float>(width) / static_cast<float>(height), 0.05f, 140.0f);
        const Mat4 view = lookAt(eye, eye + look, {0.0f, 1.0f, 0.0f});
        const Mat4 vp = multiply(proj, view);

        glUseProgram(voxelProgram);
        glUniformMatrix4fv(glGetUniformLocation(voxelProgram, "uMVP"), 1, GL_FALSE, vp.m);
        Mat4 model = identity();
        glUniformMatrix4fv(glGetUniformLocation(voxelProgram, "uModel"), 1, GL_FALSE, model.m);
        glUniform3f(glGetUniformLocation(voxelProgram, "uCamera"), eye.x, eye.y, eye.z);
        glUniform3f(glGetUniformLocation(voxelProgram, "uHeadlampDir"), look.x, look.y, look.z);
        glUniform3f(glGetUniformLocation(voxelProgram, "uSunDir"), -0.18f, -0.72f, -0.35f);
        glUniform3f(glGetUniformLocation(voxelProgram, "uFogColor"), 0.12f, 0.105f, 0.095f);
        glUniform1f(glGetUniformLocation(voxelProgram, "uTime"), static_cast<float>(now));

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glBindVertexArray(opaque.vao);
        glDrawArrays(GL_TRIANGLES, 0, opaque.count);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBindVertexArray(transparentMesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, transparentMesh.count);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        float interactionProgress = 0.0f;
        bool showingBuild = false;
        bool showingMine = false;
        if (hasHit) {
            if (rightDown && sameBlockCell(buildingTarget, previous)) {
                interactionProgress = std::clamp(buildingTimer / buildDuration(selectedBuild), 0.0f, 1.0f);
                showingBuild = true;
            } else if (leftDown && sameBlockCell(miningTarget, hit)) {
                const Block targetBlock = world.get(static_cast<int>(hit.x), static_cast<int>(hit.y), static_cast<int>(hit.z));
                interactionProgress = std::clamp(miningTimer / mineDuration(targetBlock), 0.0f, 1.0f);
                showingMine = true;
            }
            drawSelection(lineProgram, lineVao, lineVbo, vp, hit, previous, interactionProgress, showingBuild, static_cast<float>(now));
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawHand(uiProgram, uiVao, uiVbo, showingMine, showingBuild, interactionProgress, static_cast<float>(now));
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        const std::string title = "Dead Cube World - " + std::to_string(opaque.count / 6) + " bleak faces - build: "
            + buildTypeName(selectedBuild) + " - seed " + std::to_string(world.seed) + " - hold click to mine/build";
        glfwSetWindowTitle(window, title.c_str());
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
