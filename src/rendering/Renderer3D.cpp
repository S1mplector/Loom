#include "Renderer3D.hpp"
#include "rlgl.h"
#include <cmath>
#include <algorithm>

namespace ethereal {

Renderer3D::Renderer3D() : Renderer3D(RenderConfig3D{}) {}

Renderer3D::Renderer3D(const RenderConfig3D& config) : config(config), initialized(false) {
    particles.reserve(config.particleCount);
}

Renderer3D::~Renderer3D() {
    if (initialized) shutdown();
}

void Renderer3D::initialize() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(config.screenWidth, config.screenHeight, "Ethereal Flight 3D - Cape Physics Demo");
    SetTargetFPS(60);
    
    raylibCamera = { 0 };
    raylibCamera.position = { 0.0f, 50.0f, 100.0f };
    raylibCamera.target = { 0.0f, 0.0f, 0.0f };
    raylibCamera.up = { 0.0f, 1.0f, 0.0f };
    raylibCamera.fovy = 60.0f;
    raylibCamera.projection = CAMERA_PERSPECTIVE;
    
    initParticles();
    initialized = true;
}

void Renderer3D::shutdown() {
    if (initialized) {
        CloseWindow();
        initialized = false;
    }
}

void Renderer3D::initParticles() {
    particles.clear();
    for (int i = 0; i < config.particleCount; ++i) {
        AtmosphereParticle p;
        p.position = Vector3D(
            GetRandomValue(-500, 500),
            GetRandomValue(0, 300),
            GetRandomValue(-500, 500)
        );
        p.velocity = Vector3D::zero();
        p.alpha = GetRandomValue(10, 40) / 255.0f;
        p.size = GetRandomValue(5, 20) / 10.0f;
        p.lifetime = GetRandomValue(0, 1000) / 100.0f;
        p.type = GetRandomValue(0, 2);
        particles.push_back(p);
    }
}

void Renderer3D::beginFrame(const FlightCamera& camera) {
    raylibCamera.position = { camera.getPosition().x, camera.getPosition().y, camera.getPosition().z };
    raylibCamera.target = { camera.getTarget().x, camera.getTarget().y, camera.getTarget().z };
    
    BeginDrawing();
    ClearBackground(config.skyColorBottom);
}

void Renderer3D::endFrame() {
    EndDrawing();
}

void Renderer3D::drawSky(const FlightCamera& camera, float time) {
    BeginMode3D(raylibCamera);
    
    rlDisableDepthMask();
    rlDisableDepthTest();
    
    Vector3D camPos = camera.getPosition();
    float skyRadius = 800.0f;
    
    DrawSphere({camPos.x, camPos.y + skyRadius * 0.3f, camPos.z}, skyRadius, config.skyColorTop);
    
    Vector3D sunDir = config.sunDirection.normalized();
    Vector3D sunPos = camPos + sunDir * 700.0f;
    
    Color sunGlow = {255, 250, 230, 60};
    DrawSphere({sunPos.x, sunPos.y, sunPos.z}, 80.0f, sunGlow);
    DrawSphere({sunPos.x, sunPos.y, sunPos.z}, 40.0f, config.sunColor);
    
    rlEnableDepthTest();
    rlEnableDepthMask();
    
    EndMode3D();
}

void Renderer3D::drawGround(const FlightCamera& camera) {
    BeginMode3D(raylibCamera);
    
    Vector3D camPos = camera.getPosition();
    float groundY = -50.0f;
    
    Color groundNear = config.groundColor;
    Color groundFar = {80, 110, 80, 255};
    
    for (int i = -5; i <= 5; ++i) {
        for (int j = -5; j <= 5; ++j) {
            float x = camPos.x + i * 200.0f;
            float z = camPos.z + j * 200.0f;
            float dist = std::sqrt(i*i + j*j) / 7.0f;
            
            Color tileColor = {
                (unsigned char)(groundNear.r + (groundFar.r - groundNear.r) * dist),
                (unsigned char)(groundNear.g + (groundFar.g - groundNear.g) * dist),
                (unsigned char)(groundNear.b + (groundFar.b - groundNear.b) * dist),
                255
            };
            
            DrawPlane({x, groundY, z}, {200.0f, 200.0f}, tileColor);
        }
    }
    
    EndMode3D();
}

void Renderer3D::drawTerrain(const Terrain& terrain, const FlightCamera& camera) {
    BeginMode3D(raylibCamera);
    
    const auto& vertices = terrain.getVertices();
    const auto& indices = terrain.getIndices();
    const auto& terrainConfig = terrain.getConfig();
    
    Vector3D camPos = camera.getPosition();
    float viewDistance = 800.0f;
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        const auto& v0 = vertices[indices[i]];
        const auto& v1 = vertices[indices[i + 1]];
        const auto& v2 = vertices[indices[i + 2]];
        
        Vector3D center = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
        float dist = (center - camPos).length();
        
        if (dist > viewDistance) continue;
        
        Color c0 = terrain.getColorAt(v0.position.x, v0.position.z, v0.height);
        Color c1 = terrain.getColorAt(v1.position.x, v1.position.z, v1.height);
        Color c2 = terrain.getColorAt(v2.position.x, v2.position.z, v2.height);
        
        float fogFactor = std::min(dist / viewDistance, 1.0f);
        fogFactor = fogFactor * fogFactor;
        
        auto applyFog = [&](Color c) -> Color {
            return {
                (unsigned char)(c.r + (config.skyColorBottom.r - c.r) * fogFactor * 0.7f),
                (unsigned char)(c.g + (config.skyColorBottom.g - c.g) * fogFactor * 0.7f),
                (unsigned char)(c.b + (config.skyColorBottom.b - c.b) * fogFactor * 0.7f),
                255
            };
        };
        
        c0 = applyFog(c0);
        c1 = applyFog(c1);
        c2 = applyFog(c2);
        
        Color avgColor = {
            (unsigned char)((c0.r + c1.r + c2.r) / 3),
            (unsigned char)((c0.g + c1.g + c2.g) / 3),
            (unsigned char)((c0.b + c1.b + c2.b) / 3),
            255
        };
        
        DrawTriangle3D(
            {v0.position.x, v0.position.y, v0.position.z},
            {v1.position.x, v1.position.y, v1.position.z},
            {v2.position.x, v2.position.y, v2.position.z},
            avgColor
        );
        
        DrawTriangle3D(
            {v0.position.x, v0.position.y, v0.position.z},
            {v2.position.x, v2.position.y, v2.position.z},
            {v1.position.x, v1.position.y, v1.position.z},
            avgColor
        );
    }
    
    EndMode3D();
}

void Renderer3D::drawClouds(const FlightCamera& camera, float time) {
    BeginMode3D(raylibCamera);
    
    Vector3D camPos = camera.getPosition();
    
    for (int i = 0; i < 15; ++i) {
        float angle = i * 0.42f + time * 0.02f;
        float radius = 200.0f + (i % 3) * 100.0f;
        float height = 150.0f + (i % 4) * 40.0f;
        
        float x = camPos.x + std::cos(angle) * radius;
        float z = camPos.z + std::sin(angle) * radius;
        
        Color cloudCol = config.cloudColor;
        cloudCol.a = 60 + (i % 3) * 20;
        
        float cloudSize = 30.0f + (i % 5) * 15.0f;
        DrawSphere({x, height, z}, cloudSize, cloudCol);
        DrawSphere({x + cloudSize * 0.7f, height - 5, z + cloudSize * 0.3f}, cloudSize * 0.8f, cloudCol);
        DrawSphere({x - cloudSize * 0.5f, height + 5, z - cloudSize * 0.4f}, cloudSize * 0.7f, cloudCol);
    }
    
    EndMode3D();
}

void Renderer3D::drawCapeMesh(const Cape3D& cape) {
    int segments = cape.getSegments();
    int width = cape.getWidth();
    
    for (int row = 0; row < segments - 1; ++row) {
        float rowRatio = static_cast<float>(row) / (segments - 1);
        
        for (int col = 0; col < width - 1; ++col) {
            const Vector3D& p1 = cape.getParticle(row, col).position;
            const Vector3D& p2 = cape.getParticle(row, col + 1).position;
            const Vector3D& p3 = cape.getParticle(row + 1, col).position;
            const Vector3D& p4 = cape.getParticle(row + 1, col + 1).position;
            
            Color color = {
                (unsigned char)(config.capeColorInner.r + (config.capeColorOuter.r - config.capeColorInner.r) * rowRatio),
                (unsigned char)(config.capeColorInner.g + (config.capeColorOuter.g - config.capeColorInner.g) * rowRatio),
                (unsigned char)(config.capeColorInner.b + (config.capeColorOuter.b - config.capeColorInner.b) * rowRatio),
                (unsigned char)(255 - rowRatio * 30)
            };
            
            DrawTriangle3D(
                {p1.x, p1.y, p1.z},
                {p3.x, p3.y, p3.z},
                {p2.x, p2.y, p2.z},
                color
            );
            DrawTriangle3D(
                {p2.x, p2.y, p2.z},
                {p3.x, p3.y, p3.z},
                {p4.x, p4.y, p4.z},
                color
            );
            
            Color backColor = {
                (unsigned char)(color.r * 0.7f),
                (unsigned char)(color.g * 0.7f),
                (unsigned char)(color.b * 0.7f),
                color.a
            };
            DrawTriangle3D(
                {p1.x, p1.y, p1.z},
                {p2.x, p2.y, p2.z},
                {p3.x, p3.y, p3.z},
                backColor
            );
            DrawTriangle3D(
                {p2.x, p2.y, p2.z},
                {p4.x, p4.y, p4.z},
                {p3.x, p3.y, p3.z},
                backColor
            );
        }
    }
}

void Renderer3D::drawCape(const Cape3D& cape) {
    BeginMode3D(raylibCamera);
    drawCapeMesh(cape);
    EndMode3D();
}

void Renderer3D::drawCharacter(const Character3D& character) {
    BeginMode3D(raylibCamera);
    
    Vector3D pos = character.getPosition();
    float radius = character.getRadius();
    
    Color glowColor = {255, 250, 240, 30};
    DrawSphere({pos.x, pos.y, pos.z}, radius * 1.8f, glowColor);
    DrawSphere({pos.x, pos.y, pos.z}, radius * 1.4f, glowColor);
    
    DrawSphere({pos.x, pos.y, pos.z}, radius, config.characterColor);
    
    Color innerColor = {255, 252, 248, 255};
    DrawSphere({pos.x, pos.y, pos.z}, radius * 0.7f, innerColor);
    
    Vector3D forward = character.getForward();
    Vector3D eyePos = pos + forward * (radius * 0.7f) + Vector3D(0, radius * 0.2f, 0);
    DrawSphere({eyePos.x, eyePos.y, eyePos.z}, radius * 0.15f, {50, 70, 90, 255});
    
    EndMode3D();
}

void Renderer3D::drawTrail(const Character3D& character) {
    BeginMode3D(raylibCamera);
    
    const auto& trail = character.getTrail();
    for (const auto& point : trail) {
        Color trailCol = config.trailColor;
        trailCol.a = static_cast<unsigned char>(point.alpha * 180);
        DrawSphere({point.position.x, point.position.y, point.position.z}, point.size, trailCol);
    }
    
    EndMode3D();
}

void Renderer3D::drawWindField(const WindField3D& wind, const Vector3D& center) {
    if (!config.showWindDebug) return;
    
    BeginMode3D(raylibCamera);
    
    float gridSize = 50.0f;
    int gridCount = 5;
    
    for (int x = -gridCount; x <= gridCount; ++x) {
        for (int y = 0; y <= gridCount; ++y) {
            for (int z = -gridCount; z <= gridCount; ++z) {
                Vector3D pos = center + Vector3D(x * gridSize, y * gridSize, z * gridSize);
                Vector3D windVec = wind.getWindAt(pos);
                float strength = std::min(windVec.length() / 50.0f, 1.0f);
                
                Vector3D end = pos + windVec.normalized() * 10.0f;
                Color lineColor = {100, 150, 255, (unsigned char)(50 + strength * 100)};
                
                DrawLine3D({pos.x, pos.y, pos.z}, {end.x, end.y, end.z}, lineColor);
            }
        }
    }
    
    EndMode3D();
}

void Renderer3D::updateParticles(float dt, const WindField3D& wind, const FlightCamera& camera) {
    Vector3D camPos = camera.getPosition();
    
    for (auto& p : particles) {
        Vector3D windForce = wind.getWindAt(p.position) * 0.005f;
        p.velocity = p.velocity * 0.98f + windForce;
        p.position += p.velocity * dt * 60.0f;
        p.lifetime += dt;
        
        float dist = (p.position - camPos).length();
        if (dist > 600.0f || p.position.y < -60.0f) {
            p.position = camPos + Vector3D(
                GetRandomValue(-400, 400),
                GetRandomValue(0, 250),
                GetRandomValue(-400, 400)
            );
            p.velocity = Vector3D::zero();
        }
    }
}

void Renderer3D::drawAtmosphere(const WindField3D& wind, float dt, const FlightCamera& camera) {
    updateParticles(dt, wind, camera);
    
    BeginMode3D(raylibCamera);
    
    for (const auto& p : particles) {
        Color particleColor = {255, 255, 255, (unsigned char)(p.alpha * 255)};
        DrawSphere({p.position.x, p.position.y, p.position.z}, p.size, particleColor);
    }
    
    EndMode3D();
}

void Renderer3D::drawUI(const FlightController3D& flight, const PerformanceMonitor& perf, const FlightCamera& camera) {
    DrawRectangle(10, 10, 320, 130, {0, 0, 0, 140});
    DrawRectangleLines(10, 10, 320, 130, {100, 100, 100, 180});

    DrawText(perf.getStatsString().c_str(), 20, 20, 14, WHITE);

    const char* stateText = "";
    Color stateColor = WHITE;
    switch (flight.getState()) {
        case FlightState3D::Gliding:  stateText = "GLIDING";  stateColor = {150, 200, 255, 255}; break;
        case FlightState3D::Climbing: stateText = "CLIMBING"; stateColor = {255, 200, 150, 255}; break;
        case FlightState3D::Diving:   stateText = "DIVING";   stateColor = {200, 255, 200, 255}; break;
        case FlightState3D::Hovering: stateText = "HOVERING"; stateColor = {200, 200, 200, 255}; break;
        case FlightState3D::Soaring:  stateText = "SOARING";  stateColor = {255, 220, 100, 255}; break;
    }
    DrawText(stateText, 20, 45, 20, stateColor);

    DrawText(TextFormat("Altitude: %.0f m", flight.getAltitude()), 20, 75, 14, WHITE);
    
    float energy = flight.getEnergy();
    DrawRectangle(20, 100, 220, 16, {40, 40, 40, 200});
    Color energyColor = energy > 30 ? Color{100, 200, 255, 255} : Color{255, 100, 100, 255};
    DrawRectangle(20, 100, static_cast<int>(energy * 2.2f), 16, energyColor);
    DrawText("ENERGY", 250, 100, 14, WHITE);

    int bottomY = config.screenHeight - 35;
    DrawText("WASD: Move | Space: Climb | Shift: Dive | E: Boost | V: Wind Debug", 
             20, bottomY, 13, {200, 200, 200, 200});
}

bool Renderer3D::shouldClose() const {
    return WindowShouldClose();
}

float Renderer3D::getAspectRatio() const {
    return static_cast<float>(config.screenWidth) / config.screenHeight;
}

Color Renderer3D::applyFog(Color color, float distance) const {
    float fogFactor = 1.0f - std::exp(-distance * config.fogDensity);
    fogFactor = std::clamp(fogFactor, 0.0f, 1.0f);
    
    return {
        (unsigned char)(color.r + (config.skyColorBottom.r - color.r) * fogFactor),
        (unsigned char)(color.g + (config.skyColorBottom.g - color.g) * fogFactor),
        (unsigned char)(color.b + (config.skyColorBottom.b - color.b) * fogFactor),
        color.a
    };
}

} // namespace ethereal
