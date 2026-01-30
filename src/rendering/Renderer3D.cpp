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
    // Draw gradient background
    for (int y = 0; y < config.screenHeight; ++y) {
        float t = static_cast<float>(y) / config.screenHeight;
        Color lineColor = {
            (unsigned char)(config.skyColorTop.r + (config.skyColorBottom.r - config.skyColorTop.r) * t),
            (unsigned char)(config.skyColorTop.g + (config.skyColorBottom.g - config.skyColorTop.g) * t),
            (unsigned char)(config.skyColorTop.b + (config.skyColorBottom.b - config.skyColorTop.b) * t),
            255
        };
        DrawLine(0, y, config.screenWidth, y, lineColor);
    }
    
    BeginMode3D(raylibCamera);
    
    rlDisableDepthMask();
    rlDisableDepthTest();
    
    Vector3D camPos = camera.getPosition();
    
    // Sun with layered glow
    Vector3D sunDir = config.sunDirection.normalized();
    Vector3D sunPos = camPos + sunDir * 700.0f;
    
    DrawSphere({sunPos.x, sunPos.y, sunPos.z}, 120.0f, {255, 250, 235, 15});
    DrawSphere({sunPos.x, sunPos.y, sunPos.z}, 80.0f, {255, 250, 230, 30});
    DrawSphere({sunPos.x, sunPos.y, sunPos.z}, 50.0f, {255, 252, 240, 60});
    DrawSphere({sunPos.x, sunPos.y, sunPos.z}, 30.0f, config.sunColor);
    
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
    
    // Layered cloud system - distant horizon clouds
    for (int layer = 0; layer < 3; ++layer) {
        float layerHeight = 200.0f + layer * 80.0f;
        float layerRadius = 400.0f + layer * 150.0f;
        int cloudCount = 12 - layer * 2;
        
        for (int i = 0; i < cloudCount; ++i) {
            float angle = i * (6.28f / cloudCount) + time * (0.01f - layer * 0.003f) + layer * 0.5f;
            
            float x = camPos.x + std::cos(angle) * layerRadius;
            float z = camPos.z + std::sin(angle) * layerRadius;
            float y = layerHeight + std::sin(angle * 2.0f + time * 0.1f) * 15.0f;
            
            // Cloud opacity decreases with distance/height
            unsigned char alpha = (unsigned char)(40 - layer * 10);
            Color cloudCol = {255, 255, 255, alpha};
            
            // Fluffy cloud cluster
            float baseSize = 45.0f + (i % 3) * 20.0f - layer * 5.0f;
            
            // Main body
            DrawSphere({x, y, z}, baseSize, cloudCol);
            
            // Puffs around main body
            for (int p = 0; p < 5; ++p) {
                float pAngle = p * 1.25f + i * 0.7f;
                float pDist = baseSize * 0.6f;
                float px = x + std::cos(pAngle) * pDist;
                float pz = z + std::sin(pAngle) * pDist;
                float py = y + std::sin(pAngle * 2) * baseSize * 0.3f;
                float pSize = baseSize * (0.5f + (p % 3) * 0.15f);
                DrawSphere({px, py, pz}, pSize, cloudCol);
            }
        }
    }
    
    // Wispy high-altitude clouds
    for (int i = 0; i < 8; ++i) {
        float angle = i * 0.78f + time * 0.005f;
        float radius = 500.0f + (i % 2) * 100.0f;
        float x = camPos.x + std::cos(angle) * radius;
        float z = camPos.z + std::sin(angle) * radius;
        float y = 350.0f + (i % 3) * 30.0f;
        
        Color wispyCol = {255, 255, 255, 15};
        DrawSphere({x, y, z}, 60.0f + (i % 2) * 20.0f, wispyCol);
    }
    
    EndMode3D();
}

void Renderer3D::drawCapeMesh(const Cape3D& cape) {
    int segments = cape.getSegments();
    int width = cape.getWidth();
    
    static float time = 0;
    time += GetFrameTime();
    
    // Draw flowing light trails along each column (strand)
    for (int col = 0; col < width; ++col) {
        float colRatio = static_cast<float>(col) / (width - 1);
        float colOffset = (colRatio - 0.5f) * 2.0f; // -1 to 1
        
        // Each strand has slight timing offset for organic flow
        float strandPhase = col * 0.3f + time * 2.0f;
        
        for (int row = 0; row < segments - 1; ++row) {
            float rowRatio = static_cast<float>(row) / (segments - 1);
            
            const Vector3D& p1 = cape.getParticle(row, col).position;
            const Vector3D& p2 = cape.getParticle(row + 1, col).position;
            
            // Intensity pulses along strand like flowing energy
            float pulse = 0.6f + 0.4f * std::sin(strandPhase - rowRatio * 4.0f);
            
            // Fade out toward edges and tips
            float edgeFade = 1.0f - std::abs(colOffset) * 0.4f;
            float tipFade = 1.0f - rowRatio * 0.7f;
            float intensity = pulse * edgeFade * tipFade;
            
            // Color: bright warm yellow at base → soft orange → fading amber at tips
            unsigned char r = 255;
            unsigned char g = (unsigned char)(255 - rowRatio * 120);
            unsigned char b = (unsigned char)(200 - rowRatio * 180);
            
            // Core glow - bright center of each light strand
            float coreSize = (2.5f - rowRatio * 1.5f) * intensity;
            unsigned char coreAlpha = (unsigned char)(200 * intensity);
            DrawSphere({p1.x, p1.y, p1.z}, coreSize, {r, g, b, coreAlpha});
            
            // Outer glow halo
            float glowSize = coreSize * 2.5f;
            unsigned char glowAlpha = (unsigned char)(80 * intensity);
            DrawSphere({p1.x, p1.y, p1.z}, glowSize, {255, (unsigned char)(g * 0.8f), (unsigned char)(b * 0.5f), glowAlpha});
            
            // Connecting light between particles (like flowing energy)
            if (row < segments - 2) {
                Vector3D mid = (p1 + p2) * 0.5f;
                float midSize = coreSize * 0.7f;
                unsigned char midAlpha = (unsigned char)(120 * intensity);
                DrawSphere({mid.x, mid.y, mid.z}, midSize, {r, g, b, midAlpha});
            }
        }
    }
    
    // Add subtle cross-connections for ethereal web effect
    for (int row = 1; row < segments; row += 2) {
        float rowRatio = static_cast<float>(row) / (segments - 1);
        float rowIntensity = (1.0f - rowRatio) * 0.5f;
        
        for (int col = 0; col < width - 1; ++col) {
            const Vector3D& p1 = cape.getParticle(row, col).position;
            const Vector3D& p2 = cape.getParticle(row, col + 1).position;
            Vector3D mid = (p1 + p2) * 0.5f;
            
            float pulse = 0.5f + 0.5f * std::sin(time * 3.0f + row * 0.5f + col * 0.3f);
            unsigned char alpha = (unsigned char)(40 * rowIntensity * pulse);
            
            if (alpha > 5) {
                DrawSphere({mid.x, mid.y, mid.z}, 1.5f, {255, 200, 120, alpha});
            }
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
    
    // Animated flicker for candlelight effect
    static float flickerTime = 0;
    flickerTime += GetFrameTime() * 8.0f;
    float flicker = 1.0f + std::sin(flickerTime * 3.7f) * 0.1f 
                        + std::sin(flickerTime * 7.3f) * 0.05f
                        + std::sin(flickerTime * 13.1f) * 0.03f;
    
    // Large outer glow - warm orange ambient
    DrawSphere({pos.x, pos.y, pos.z}, radius * 5.0f * flicker, {255, 150, 50, 40});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 3.5f * flicker, {255, 170, 70, 70});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 2.5f * flicker, {255, 200, 100, 100});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 1.8f * flicker, {255, 220, 150, 150});
    
    // Main flame body - bright warm yellow/orange
    DrawSphere({pos.x, pos.y, pos.z}, radius * 1.2f, {255, 230, 180, 255});
    DrawSphere({pos.x, pos.y, pos.z}, radius, {255, 240, 200, 255});
    
    // Inner hot core - bright white/yellow
    DrawSphere({pos.x, pos.y, pos.z}, radius * 0.6f, {255, 255, 230, 255});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 0.3f, {255, 255, 255, 255});
    
    EndMode3D();
}

void Renderer3D::drawTrail(const Character3D& character) {
    BeginMode3D(raylibCamera);
    
    const auto& trail = character.getTrail();
    for (size_t i = 0; i < trail.size(); ++i) {
        const auto& point = trail[i];
        float t = static_cast<float>(i) / std::max(1.0f, static_cast<float>(trail.size()));
        
        // Warm ember trail - fades from bright yellow to deep orange
        unsigned char r = 255;
        unsigned char g = (unsigned char)(220 - t * 100);
        unsigned char b = (unsigned char)(150 - t * 100);
        unsigned char a = static_cast<unsigned char>(point.alpha * 200);
        
        Color trailCol = {r, g, b, a};
        DrawSphere({point.position.x, point.position.y, point.position.z}, point.size, trailCol);
        
        // Glow around each ember
        Color glowCol = {255, 200, 120, (unsigned char)(a / 3)};
        DrawSphere({point.position.x, point.position.y, point.position.z}, point.size * 2.0f, glowCol);
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
        // Gentle floating motion with wind influence
        Vector3D windForce = wind.getWindAt(p.position) * 0.003f;
        
        // Add slight upward drift and swirl
        float swirl = std::sin(p.lifetime * 2.0f + p.position.x * 0.01f) * 0.5f;
        Vector3D drift(swirl, 0.3f, std::cos(p.lifetime * 1.5f) * 0.3f);
        
        p.velocity = p.velocity * 0.96f + windForce + drift * dt;
        p.position += p.velocity * dt * 40.0f;
        p.lifetime += dt;
        
        // Fade based on lifetime
        p.alpha = std::max(0.0f, 0.4f - p.lifetime * 0.02f);
        
        float dist = (p.position - camPos).length();
        if (dist > 500.0f || p.position.y < -60.0f || p.alpha <= 0.0f) {
            // Respawn around camera
            float angle = GetRandomValue(0, 628) * 0.01f;
            float spawnDist = GetRandomValue(50, 300);
            p.position = camPos + Vector3D(
                std::cos(angle) * spawnDist,
                GetRandomValue(-30, 150),
                std::sin(angle) * spawnDist
            );
            p.velocity = Vector3D::zero();
            p.lifetime = 0;
            p.alpha = 0.3f + GetRandomValue(0, 20) * 0.01f;
            p.size = 0.8f + GetRandomValue(0, 15) * 0.1f;
        }
    }
}

void Renderer3D::drawAtmosphere(const WindField3D& wind, float dt, const FlightCamera& camera) {
    updateParticles(dt, wind, camera);
    
    BeginMode3D(raylibCamera);
    
    Vector3D camPos = camera.getPosition();
    
    for (const auto& p : particles) {
        // Distance fade
        float dist = (p.position - camPos).length();
        float distFade = 1.0f - std::min(dist / 400.0f, 1.0f);
        
        // Warm dust color with slight variation
        unsigned char alpha = (unsigned char)(p.alpha * distFade * 180);
        Color particleColor = {255, 250, 240, alpha};
        
        if (alpha > 5) {
            DrawSphere({p.position.x, p.position.y, p.position.z}, p.size, particleColor);
            // Subtle glow
            DrawSphere({p.position.x, p.position.y, p.position.z}, p.size * 2.0f, {255, 240, 200, (unsigned char)(alpha / 4)});
        }
    }
    
    EndMode3D();
}

void Renderer3D::drawUI(const FlightController3D& flight, const PerformanceMonitor& perf, const FlightCamera& camera) {
    // Sky: Children of the Light style minimal HUD
    
    // Energy indicator - small arc in bottom left (like Sky's wing energy)
    float energy = flight.getEnergy();
    int centerX = 60;
    int centerY = config.screenHeight - 60;
    float radius = 35.0f;
    
    // Outer glow
    Color glowColor = {255, 255, 255, 30};
    DrawCircle(centerX, centerY, radius + 8, glowColor);
    
    // Background arc
    DrawCircle(centerX, centerY, radius + 2, {0, 0, 0, 60});
    
    // Energy arc - draw as segments
    float energyAngle = (energy / 100.0f) * 360.0f;
    Color energyColor = energy > 30 ? Color{255, 255, 255, 200} : Color{255, 180, 120, 220};
    
    for (float angle = 0; angle < energyAngle; angle += 5.0f) {
        float rad1 = (angle - 90) * 0.0174533f;
        float rad2 = (std::min(angle + 5.0f, energyAngle) - 90) * 0.0174533f;
        
        Vector2 p1 = {centerX + std::cos(rad1) * (radius - 3), centerY + std::sin(rad1) * (radius - 3)};
        Vector2 p2 = {centerX + std::cos(rad2) * (radius - 3), centerY + std::sin(rad2) * (radius - 3)};
        Vector2 p3 = {centerX + std::cos(rad2) * (radius + 3), centerY + std::sin(rad2) * (radius + 3)};
        Vector2 p4 = {centerX + std::cos(rad1) * (radius + 3), centerY + std::sin(rad1) * (radius + 3)};
        
        DrawTriangle(p1, p2, p3, energyColor);
        DrawTriangle(p1, p3, p4, energyColor);
    }
    
    // Center orb icon
    DrawCircle(centerX, centerY, 12, {255, 255, 255, 180});
    DrawCircle(centerX, centerY, 8, {255, 250, 240, 255});
    
    // Minimal altitude indicator - top right, very subtle
    float altitude = flight.getAltitude();
    Color altColor = {255, 255, 255, 120};
    DrawText(TextFormat("%.0f", altitude), config.screenWidth - 70, 25, 20, altColor);
    
    // Tiny wing icon next to altitude
    int wx = config.screenWidth - 95;
    int wy = 30;
    DrawTriangle({(float)wx, (float)wy}, {(float)wx-12, (float)wy+8}, {(float)wx-6, (float)wy}, {255, 255, 255, 100});
    DrawTriangle({(float)wx, (float)wy}, {(float)wx+12, (float)wy+8}, {(float)wx+6, (float)wy}, {255, 255, 255, 100});
    
    // Debug info only when TAB held (handled in main)
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
