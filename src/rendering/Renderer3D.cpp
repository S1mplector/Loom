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
    Vector3D sunDir = config.sunDirection.normalized();
    float viewDistance = 1000.0f;
    float nearDistance = 100.0f;
    
    // === LOW-POLY SMOOTH AESTHETIC ===
    // Each triangle gets smooth color based on lighting + height + distance
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        const auto& v0 = vertices[indices[i]];
        const auto& v1 = vertices[indices[i + 1]];
        const auto& v2 = vertices[indices[i + 2]];
        
        Vector3D center = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
        float dist = (center - camPos).length();
        
        if (dist > viewDistance) continue;
        
        // Calculate face normal for flat shading (low-poly look)
        Vector3D edge1 = v1.position - v0.position;
        Vector3D edge2 = v2.position - v0.position;
        Vector3D faceNormal = edge1.cross(edge2).normalized();
        
        // === Lighting calculations ===
        // Diffuse lighting from sun
        float diffuse = std::max(0.0f, faceNormal.dot(sunDir));
        diffuse = 0.4f + diffuse * 0.6f;  // Ambient + diffuse
        
        // Soft rim lighting for silhouette enhancement
        Vector3D toCamera = (camPos - center).normalized();
        float rim = 1.0f - std::max(0.0f, faceNormal.dot(toCamera));
        rim = std::pow(rim, 2.0f) * 0.15f;
        
        // Height-based color with smooth gradients
        float avgHeight = (v0.height + v1.height + v2.height) / 3.0f;
        float normalizedHeight = (avgHeight - terrainConfig.baseHeight) / terrainConfig.maxHeight;
        normalizedHeight = std::clamp(normalizedHeight, 0.0f, 1.0f);
        
        // === Smooth color palette (low-poly aesthetic) ===
        Color baseColor;
        
        if (normalizedHeight > 0.85f) {
            // Snow caps - soft white with slight blue tint
            float t = (normalizedHeight - 0.85f) / 0.15f;
            baseColor = {
                (unsigned char)(220 + t * 35),
                (unsigned char)(225 + t * 30),
                (unsigned char)(235 + t * 20),
                255
            };
        } else if (normalizedHeight > 0.6f) {
            // Rocky slopes - warm gray transitioning to snow
            float t = (normalizedHeight - 0.6f) / 0.25f;
            baseColor = {
                (unsigned char)(140 + t * 80),
                (unsigned char)(130 + t * 95),
                (unsigned char)(120 + t * 115),
                255
            };
        } else if (normalizedHeight > 0.35f) {
            // Mid elevation - earthy tones
            float t = (normalizedHeight - 0.35f) / 0.25f;
            baseColor = {
                (unsigned char)(160 - t * 20),
                (unsigned char)(145 - t * 15),
                (unsigned char)(110 + t * 10),
                255
            };
        } else if (normalizedHeight > 0.15f) {
            // Lower slopes - warm sand/grass blend
            float t = (normalizedHeight - 0.15f) / 0.2f;
            baseColor = {
                (unsigned char)(180 - t * 20),
                (unsigned char)(175 - t * 30),
                (unsigned char)(130 - t * 20),
                255
            };
        } else {
            // Valley floor - soft green-sand
            float t = normalizedHeight / 0.15f;
            baseColor = {
                (unsigned char)(165 + t * 15),
                (unsigned char)(180 - t * 5),
                (unsigned char)(140 - t * 10),
                255
            };
        }
        
        // Apply diffuse lighting
        float lightFactor = diffuse + rim;
        baseColor.r = (unsigned char)std::clamp((int)(baseColor.r * lightFactor), 0, 255);
        baseColor.g = (unsigned char)std::clamp((int)(baseColor.g * lightFactor), 0, 255);
        baseColor.b = (unsigned char)std::clamp((int)(baseColor.b * lightFactor), 0, 255);
        
        // === Smooth distance fog ===
        float fogStart = 200.0f;
        float fogFactor = std::clamp((dist - fogStart) / (viewDistance - fogStart), 0.0f, 1.0f);
        fogFactor = fogFactor * fogFactor;  // Quadratic falloff for smoothness
        
        // Fog color gradient based on height (atmospheric perspective)
        float heightFog = std::clamp(center.y / 200.0f, 0.0f, 1.0f);
        Color fogColor = {
            (unsigned char)(config.skyColorBottom.r - heightFog * 20),
            (unsigned char)(config.skyColorBottom.g - heightFog * 10),
            (unsigned char)(config.skyColorBottom.b + heightFog * 10),
            255
        };
        
        Color finalColor = {
            (unsigned char)(baseColor.r + (fogColor.r - baseColor.r) * fogFactor * 0.85f),
            (unsigned char)(baseColor.g + (fogColor.g - baseColor.g) * fogFactor * 0.85f),
            (unsigned char)(baseColor.b + (fogColor.b - baseColor.b) * fogFactor * 0.85f),
            255
        };
        
        // Draw both sides of the triangle
        DrawTriangle3D(
            {v0.position.x, v0.position.y, v0.position.z},
            {v1.position.x, v1.position.y, v1.position.z},
            {v2.position.x, v2.position.y, v2.position.z},
            finalColor
        );
        DrawTriangle3D(
            {v0.position.x, v0.position.y, v0.position.z},
            {v2.position.x, v2.position.y, v2.position.z},
            {v1.position.x, v1.position.y, v1.position.z},
            finalColor
        );
    }
    
    EndMode3D();
}

void Renderer3D::drawClouds(const FlightCamera& camera, float time) {
    BeginMode3D(raylibCamera);
    
    Vector3D camPos = camera.getPosition();
    
    // === LAYER 1: Distant horizon clouds (soft, painterly) ===
    for (int layer = 0; layer < 4; ++layer) {
        float layerHeight = 180.0f + layer * 60.0f;
        float layerRadius = 350.0f + layer * 120.0f;
        int cloudCount = 10 - layer;
        
        for (int i = 0; i < cloudCount; ++i) {
            float angle = i * (6.28318f / cloudCount) + time * (0.008f - layer * 0.002f) + layer * 0.8f;
            
            float x = camPos.x + std::cos(angle) * layerRadius;
            float z = camPos.z + std::sin(angle) * layerRadius;
            float y = layerHeight + std::sin(angle * 1.5f + time * 0.08f) * 12.0f;
            
            // Soft gradient opacity - inner clouds brighter
            float layerFade = 1.0f - layer * 0.2f;
            unsigned char baseAlpha = (unsigned char)(35 * layerFade);
            
            // Warm-tinted clouds (sunset feel)
            unsigned char r = 255;
            unsigned char g = (unsigned char)(252 - layer * 3);
            unsigned char b = (unsigned char)(248 - layer * 8);
            
            float baseSize = 50.0f + (i % 4) * 15.0f - layer * 8.0f;
            
            // Multi-layered cloud puff for volume
            // Core (brightest)
            DrawSphere({x, y, z}, baseSize * 0.6f, {r, g, b, (unsigned char)(baseAlpha * 1.2f)});
            // Mid layer
            DrawSphere({x, y, z}, baseSize * 0.85f, {r, g, b, (unsigned char)(baseAlpha * 0.7f)});
            // Outer soft edge
            DrawSphere({x, y, z}, baseSize * 1.1f, {r, g, b, (unsigned char)(baseAlpha * 0.3f)});
            
            // Organic puffs around main body
            for (int p = 0; p < 4; ++p) {
                float pAngle = p * 1.57f + i * 0.9f + time * 0.02f;
                float pDist = baseSize * 0.55f;
                float px = x + std::cos(pAngle) * pDist;
                float pz = z + std::sin(pAngle) * pDist;
                float py = y + std::sin(pAngle * 2.5f) * baseSize * 0.25f;
                float pSize = baseSize * (0.4f + (p % 3) * 0.12f);
                
                DrawSphere({px, py, pz}, pSize * 0.7f, {r, g, b, (unsigned char)(baseAlpha * 0.8f)});
                DrawSphere({px, py, pz}, pSize, {r, g, b, (unsigned char)(baseAlpha * 0.4f)});
            }
        }
    }
    
    // === LAYER 2: Mid-altitude fluffy clouds ===
    for (int i = 0; i < 6; ++i) {
        float angle = i * 1.047f + time * 0.012f + 0.5f;
        float radius = 280.0f + (i % 3) * 60.0f;
        float x = camPos.x + std::cos(angle) * radius;
        float z = camPos.z + std::sin(angle) * radius;
        float y = 140.0f + (i % 2) * 25.0f + std::sin(time * 0.15f + i) * 8.0f;
        
        float size = 35.0f + (i % 3) * 12.0f;
        
        // Brighter, more defined clouds
        DrawSphere({x, y, z}, size * 0.5f, {255, 253, 250, 50});
        DrawSphere({x, y, z}, size * 0.75f, {255, 252, 248, 30});
        DrawSphere({x, y, z}, size, {255, 250, 245, 15});
    }
    
    // === LAYER 3: High-altitude wisps (cirrus-like) ===
    for (int i = 0; i < 12; ++i) {
        float angle = i * 0.524f + time * 0.004f;
        float radius = 450.0f + (i % 3) * 80.0f;
        float x = camPos.x + std::cos(angle) * radius;
        float z = camPos.z + std::sin(angle) * radius;
        float y = 320.0f + (i % 4) * 25.0f;
        
        // Very soft, stretched wisps
        float wispSize = 70.0f + (i % 2) * 30.0f;
        
        // Slight blue tint for high altitude
        DrawSphere({x, y, z}, wispSize * 0.4f, {250, 252, 255, 18});
        DrawSphere({x, y, z}, wispSize * 0.7f, {248, 250, 255, 10});
        DrawSphere({x, y, z}, wispSize, {245, 248, 255, 5});
    }
    
    EndMode3D();
}

void Renderer3D::drawCapeMesh(const Cape3D& cape) {
    int segments = cape.getSegments();
    int width = cape.getWidth();
    
    static float time = 0;
    time += GetFrameTime();
    
    // === LAYER 1: Ethereal ribbon mesh with smooth gradients ===
    for (int row = 0; row < segments - 1; ++row) {
        float rowRatio = static_cast<float>(row) / (segments - 1);
        float nextRowRatio = static_cast<float>(row + 1) / (segments - 1);
        
        for (int col = 0; col < width - 1; ++col) {
            float colRatio = static_cast<float>(col) / (width - 1);
            float colOffset = (colRatio - 0.5f) * 2.0f;
            
            const Vector3D& p00 = cape.getParticle(row, col).position;
            const Vector3D& p01 = cape.getParticle(row, col + 1).position;
            const Vector3D& p10 = cape.getParticle(row + 1, col).position;
            const Vector3D& p11 = cape.getParticle(row + 1, col + 1).position;
            
            // Flowing wave pattern across the cape
            float wavePhase = time * 1.5f + colRatio * 3.14159f + rowRatio * 2.0f;
            float wave = 0.7f + 0.3f * std::sin(wavePhase);
            
            // Aurora-like color shifting
            float hueShift = std::sin(time * 0.8f + colRatio * 2.0f) * 0.5f + 0.5f;
            float edgeFade = 1.0f - std::abs(colOffset) * 0.3f;
            float tipFade = 1.0f - rowRatio * 0.5f;
            float intensity = wave * edgeFade * tipFade;
            
            // Gradient: warm gold → soft rose → ethereal blue at tips
            unsigned char r = (unsigned char)(255 - rowRatio * 40 * hueShift);
            unsigned char g = (unsigned char)(220 - rowRatio * 80 + hueShift * 30);
            unsigned char b = (unsigned char)(180 + rowRatio * 60 + hueShift * 40);
            unsigned char alpha = (unsigned char)(180 * intensity);
            
            Color quadColor = {r, g, b, alpha};
            
            // Draw smooth quad as two triangles
            DrawTriangle3D(
                {p00.x, p00.y, p00.z},
                {p10.x, p10.y, p10.z},
                {p01.x, p01.y, p01.z},
                quadColor
            );
            DrawTriangle3D(
                {p01.x, p01.y, p01.z},
                {p10.x, p10.y, p10.z},
                {p11.x, p11.y, p11.z},
                quadColor
            );
            // Back faces
            DrawTriangle3D(
                {p00.x, p00.y, p00.z},
                {p01.x, p01.y, p01.z},
                {p10.x, p10.y, p10.z},
                quadColor
            );
            DrawTriangle3D(
                {p01.x, p01.y, p01.z},
                {p11.x, p11.y, p11.z},
                {p10.x, p10.y, p10.z},
                quadColor
            );
        }
    }
    
    // === LAYER 2: Glowing edge highlights ===
    for (int row = 0; row < segments; ++row) {
        float rowRatio = static_cast<float>(row) / (segments - 1);
        
        // Left and right edge glow
        for (int edge = 0; edge < 2; ++edge) {
            int col = (edge == 0) ? 0 : width - 1;
            const Vector3D& p = cape.getParticle(row, col).position;
            
            float pulse = 0.6f + 0.4f * std::sin(time * 3.0f + rowRatio * 6.0f + edge * 3.14159f);
            float tipFade = 1.0f - rowRatio * 0.6f;
            float glowIntensity = pulse * tipFade;
            
            float glowSize = (1.8f - rowRatio * 0.8f) * glowIntensity;
            unsigned char glowAlpha = (unsigned char)(140 * glowIntensity);
            
            // Inner bright core
            DrawSphere({p.x, p.y, p.z}, glowSize * 0.5f, {255, 250, 240, glowAlpha});
            // Outer soft glow
            DrawSphere({p.x, p.y, p.z}, glowSize * 1.5f, {255, 220, 180, (unsigned char)(glowAlpha / 3)});
        }
    }
    
    // === LAYER 3: Trailing tip sparkles ===
    int lastRow = segments - 1;
    for (int col = 0; col < width; ++col) {
        float colRatio = static_cast<float>(col) / (width - 1);
        const Vector3D& tipPos = cape.getParticle(lastRow, col).position;
        const Vector3D& prevPos = cape.getParticle(lastRow - 1, col).position;
        
        // Velocity-based sparkle intensity
        Vector3D tipVel = tipPos - prevPos;
        float speed = tipVel.length() * 60.0f;
        float sparkleIntensity = std::min(speed * 0.15f, 1.0f);
        
        float sparklePhase = time * 8.0f + col * 1.7f;
        float sparkle = (std::sin(sparklePhase) * 0.5f + 0.5f) * sparkleIntensity;
        
        if (sparkle > 0.2f) {
            unsigned char alpha = (unsigned char)(200 * sparkle);
            float size = 1.0f + sparkle * 1.5f;
            
            // Bright sparkle
            DrawSphere({tipPos.x, tipPos.y, tipPos.z}, size, {255, 255, 255, alpha});
            // Warm glow halo
            DrawSphere({tipPos.x, tipPos.y, tipPos.z}, size * 2.5f, {255, 200, 150, (unsigned char)(alpha / 4)});
        }
    }
    
    // === LAYER 4: Flowing energy strands (subtle) ===
    for (int col = 1; col < width - 1; col += 2) {
        float colRatio = static_cast<float>(col) / (width - 1);
        float strandPhase = time * 2.5f + col * 0.5f;
        
        for (int row = 0; row < segments - 1; ++row) {
            float rowRatio = static_cast<float>(row) / (segments - 1);
            
            const Vector3D& p1 = cape.getParticle(row, col).position;
            const Vector3D& p2 = cape.getParticle(row + 1, col).position;
            Vector3D mid = (p1 + p2) * 0.5f;
            
            // Energy pulse traveling down the strand
            float energyPos = std::fmod(strandPhase, 1.0f);
            float distToEnergy = std::abs(rowRatio - energyPos);
            float energyIntensity = std::max(0.0f, 1.0f - distToEnergy * 4.0f);
            
            if (energyIntensity > 0.1f) {
                float tipFade = 1.0f - rowRatio * 0.5f;
                unsigned char alpha = (unsigned char)(120 * energyIntensity * tipFade);
                float size = 1.2f * energyIntensity;
                
                DrawSphere({mid.x, mid.y, mid.z}, size, {255, 240, 200, alpha});
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
    
    static float time = 0;
    time += GetFrameTime();
    
    // === Breathing pulse animation ===
    float breathe = 1.0f + std::sin(time * 2.0f) * 0.08f;
    float heartbeat = 1.0f + std::sin(time * 4.5f) * 0.03f * (std::sin(time * 0.5f) > 0 ? 1.0f : 0.0f);
    float pulse = breathe * heartbeat;
    
    // === LAYER 1: Ambient light field (very large, subtle) ===
    DrawSphere({pos.x, pos.y, pos.z}, radius * 8.0f * pulse, {255, 240, 220, 15});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 6.0f * pulse, {255, 230, 200, 25});
    
    // === LAYER 2: Ethereal aura rings ===
    for (int ring = 0; ring < 3; ++ring) {
        float ringPhase = time * (1.5f - ring * 0.3f) + ring * 2.094f;
        float ringPulse = 0.8f + 0.2f * std::sin(ringPhase);
        float ringSize = radius * (3.5f - ring * 0.6f) * ringPulse * pulse;
        unsigned char ringAlpha = (unsigned char)(50 - ring * 12);
        
        // Warm gradient from inner to outer
        unsigned char r = 255;
        unsigned char g = (unsigned char)(200 + ring * 20);
        unsigned char b = (unsigned char)(150 + ring * 30);
        
        DrawSphere({pos.x, pos.y, pos.z}, ringSize, {r, g, b, ringAlpha});
    }
    
    // === LAYER 3: Core glow layers ===
    DrawSphere({pos.x, pos.y, pos.z}, radius * 2.2f * pulse, {255, 220, 180, 80});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 1.6f * pulse, {255, 235, 200, 140});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 1.2f * pulse, {255, 245, 220, 200});
    
    // === LAYER 4: Solid character body ===
    DrawSphere({pos.x, pos.y, pos.z}, radius * 0.9f, {255, 250, 240, 255});
    
    // === LAYER 5: Inner light core ===
    float coreFlicker = 1.0f + std::sin(time * 6.0f) * 0.1f + std::sin(time * 11.0f) * 0.05f;
    DrawSphere({pos.x, pos.y, pos.z}, radius * 0.5f * coreFlicker, {255, 255, 250, 255});
    DrawSphere({pos.x, pos.y, pos.z}, radius * 0.25f * coreFlicker, {255, 255, 255, 255});
    
    // === LAYER 6: Orbiting light wisps ===
    for (int i = 0; i < 4; ++i) {
        float orbitSpeed = 1.2f + i * 0.3f;
        float orbitRadius = radius * (2.0f + i * 0.5f);
        float orbitTilt = i * 0.4f;
        float orbitPhase = time * orbitSpeed + i * 1.57f;
        
        float wx = pos.x + std::cos(orbitPhase) * orbitRadius;
        float wy = pos.y + std::sin(orbitPhase * 0.7f + orbitTilt) * orbitRadius * 0.4f;
        float wz = pos.z + std::sin(orbitPhase) * orbitRadius;
        
        float wispPulse = 0.6f + 0.4f * std::sin(orbitPhase * 2.0f);
        float wispSize = radius * 0.3f * wispPulse;
        unsigned char wispAlpha = (unsigned char)(150 * wispPulse);
        
        DrawSphere({wx, wy, wz}, wispSize, {255, 250, 230, wispAlpha});
        DrawSphere({wx, wy, wz}, wispSize * 2.0f, {255, 230, 180, (unsigned char)(wispAlpha / 3)});
    }
    
    EndMode3D();
}

void Renderer3D::drawTrail(const Character3D& character) {
    BeginMode3D(raylibCamera);
    
    static float time = 0;
    time += GetFrameTime();
    
    const auto& trail = character.getTrail();
    size_t trailSize = trail.size();
    
    if (trailSize < 2) {
        EndMode3D();
        return;
    }
    
    // === LAYER 1: Smooth ribbon trail ===
    for (size_t i = 0; i < trailSize - 1; ++i) {
        const auto& p1 = trail[i];
        const auto& p2 = trail[i + 1];
        float t1 = static_cast<float>(i) / trailSize;
        float t2 = static_cast<float>(i + 1) / trailSize;
        
        // Smooth color gradient: bright gold → warm orange → soft rose → fading lavender
        auto getTrailColor = [&](float t, float alpha) -> Color {
            unsigned char r = (unsigned char)(255 - t * 30);
            unsigned char g = (unsigned char)(240 - t * 100);
            unsigned char b = (unsigned char)(200 - t * 60 + t * t * 80);
            unsigned char a = (unsigned char)(alpha * (1.0f - t * 0.7f) * 255);
            return {r, g, b, a};
        };
        
        Color c1 = getTrailColor(t1, p1.alpha);
        Color c2 = getTrailColor(t2, p2.alpha);
        Color avgColor = {
            (unsigned char)((c1.r + c2.r) / 2),
            (unsigned char)((c1.g + c2.g) / 2),
            (unsigned char)((c1.b + c2.b) / 2),
            (unsigned char)((c1.a + c2.a) / 2)
        };
        
        // Draw connecting ribbon segment
        float size1 = p1.size * (1.0f - t1 * 0.5f);
        float size2 = p2.size * (1.0f - t2 * 0.5f);
        float avgSize = (size1 + size2) * 0.5f;
        
        Vector3D mid = (p1.position + p2.position) * 0.5f;
        DrawSphere({mid.x, mid.y, mid.z}, avgSize * 0.8f, avgColor);
    }
    
    // === LAYER 2: Glowing core particles ===
    for (size_t i = 0; i < trailSize; ++i) {
        const auto& point = trail[i];
        float t = static_cast<float>(i) / trailSize;
        
        // Core brightness decreases along trail
        float coreBrightness = (1.0f - t * 0.6f) * point.alpha;
        if (coreBrightness < 0.1f) continue;
        
        unsigned char r = 255;
        unsigned char g = (unsigned char)(250 - t * 60);
        unsigned char b = (unsigned char)(230 - t * 80);
        unsigned char coreAlpha = (unsigned char)(200 * coreBrightness);
        
        float coreSize = point.size * (1.0f - t * 0.4f);
        DrawSphere({point.position.x, point.position.y, point.position.z}, coreSize, {r, g, b, coreAlpha});
        
        // Outer glow halo
        unsigned char glowAlpha = (unsigned char)(80 * coreBrightness);
        DrawSphere({point.position.x, point.position.y, point.position.z}, coreSize * 2.2f, {255, 220, 180, glowAlpha});
    }
    
    // === LAYER 3: Dissipating sparkles at trail end ===
    for (size_t i = trailSize / 2; i < trailSize; ++i) {
        const auto& point = trail[i];
        float t = static_cast<float>(i) / trailSize;
        
        // Sparkle effect increases toward end
        float sparkleChance = (t - 0.5f) * 2.0f;
        float sparklePhase = time * 10.0f + i * 2.3f;
        float sparkle = std::sin(sparklePhase) * 0.5f + 0.5f;
        
        if (sparkle * sparkleChance > 0.4f && point.alpha > 0.2f) {
            float sparkleIntensity = sparkle * sparkleChance * point.alpha;
            unsigned char alpha = (unsigned char)(180 * sparkleIntensity);
            float size = 0.5f + sparkleIntensity * 1.0f;
            
            // Offset sparkle position slightly for variation
            float ox = std::sin(sparklePhase * 1.3f) * point.size * 0.5f;
            float oy = std::cos(sparklePhase * 1.7f) * point.size * 0.5f;
            float oz = std::sin(sparklePhase * 0.9f) * point.size * 0.5f;
            
            DrawSphere({point.position.x + ox, point.position.y + oy, point.position.z + oz}, 
                       size, {255, 255, 240, alpha});
        }
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
    
    static float time = 0;
    time += dt;
    
    BeginMode3D(raylibCamera);
    
    Vector3D camPos = camera.getPosition();
    
    for (size_t idx = 0; idx < particles.size(); ++idx) {
        const auto& p = particles[idx];
        
        // Distance-based fade with smooth falloff
        float dist = (p.position - camPos).length();
        float distFade = 1.0f - std::pow(std::min(dist / 450.0f, 1.0f), 1.5f);
        
        if (distFade < 0.05f) continue;
        
        // Particle type determines behavior and appearance
        float typePhase = time * 2.0f + idx * 0.7f;
        
        if (p.type == 0) {
            // === Type 0: Floating dust motes ===
            float pulse = 0.7f + 0.3f * std::sin(typePhase * 0.5f);
            unsigned char alpha = (unsigned char)(p.alpha * distFade * 160 * pulse);
            
            if (alpha > 8) {
                // Warm golden dust
                Color dustColor = {255, 248, 235, alpha};
                DrawSphere({p.position.x, p.position.y, p.position.z}, p.size * 0.8f, dustColor);
                
                // Soft glow halo
                DrawSphere({p.position.x, p.position.y, p.position.z}, p.size * 1.8f, 
                          {255, 240, 210, (unsigned char)(alpha / 4)});
            }
        } 
        else if (p.type == 1) {
            // === Type 1: Ethereal firefly sparkles ===
            float sparkle = std::sin(typePhase * 3.0f);
            sparkle = sparkle * sparkle * sparkle;  // Sharp peaks
            
            if (sparkle > 0.3f) {
                float intensity = (sparkle - 0.3f) / 0.7f;
                unsigned char alpha = (unsigned char)(p.alpha * distFade * 220 * intensity);
                
                if (alpha > 15) {
                    // Bright white-gold sparkle
                    float sparkleSize = p.size * 0.5f * (0.5f + intensity * 0.5f);
                    DrawSphere({p.position.x, p.position.y, p.position.z}, sparkleSize, 
                              {255, 255, 250, alpha});
                    // Warm glow
                    DrawSphere({p.position.x, p.position.y, p.position.z}, sparkleSize * 3.0f, 
                              {255, 230, 180, (unsigned char)(alpha / 3)});
                }
            }
        }
        else {
            // === Type 2: Drifting light wisps ===
            float drift = std::sin(typePhase * 0.3f) * 0.5f + 0.5f;
            unsigned char alpha = (unsigned char)(p.alpha * distFade * 100 * drift);
            
            if (alpha > 5) {
                // Soft ethereal blue-white
                unsigned char r = (unsigned char)(240 + drift * 15);
                unsigned char g = (unsigned char)(245 + drift * 10);
                unsigned char b = 255;
                
                DrawSphere({p.position.x, p.position.y, p.position.z}, p.size * 1.2f, 
                          {r, g, b, alpha});
                // Very soft outer glow
                DrawSphere({p.position.x, p.position.y, p.position.z}, p.size * 2.5f, 
                          {r, g, b, (unsigned char)(alpha / 5)});
            }
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
