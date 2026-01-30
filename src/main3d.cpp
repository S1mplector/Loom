#include "raylib.h"
#include "core/Vector3D.hpp"
#include "core/Quaternion.hpp"
#include "physics/WindField3D.hpp"
#include "physics/Cape3D.hpp"
#include "entities/Character3D.hpp"
#include "entities/Camera3D.hpp"
#include "entities/FlightController3D.hpp"
#include "environment/Terrain.hpp"
#include "rendering/Renderer3D.hpp"
#include "utils/PerformanceMonitor.hpp"

using namespace ethereal;

int main() {
    RenderConfig3D renderConfig;
    renderConfig.screenWidth = 1280;
    renderConfig.screenHeight = 720;
    
    // Warm desert sky colors
    renderConfig.skyColorTop = {120, 170, 215, 255};
    renderConfig.skyColorBottom = {250, 240, 225, 255};
    renderConfig.sunColor = {255, 250, 235, 255};
    renderConfig.cloudColor = {255, 255, 255, 50};
    renderConfig.sunDirection = Vector3D(0.3f, 0.85f, 0.15f);
    
    // Cape colors - warm red/orange gradient
    renderConfig.capeColorInner = {200, 80, 60, 255};
    renderConfig.capeColorOuter = {255, 140, 90, 255};
    
    // Trail color
    renderConfig.trailColor = {255, 220, 180, 200};
    
    Renderer3D renderer(renderConfig);
    renderer.initialize();

    // Wind disabled by default for calm flight
    WindConfig3D windConfig;
    windConfig.baseStrength = 0.0f;
    windConfig.gustStrength = 0.0f;
    windConfig.turbulence = 0.0f;
    windConfig.noiseScale = 0.004f;
    windConfig.timeScale = 0.3f;
    windConfig.baseDirection = Vector3D(0.0f, 0.0f, 0.0f);
    windConfig.curlStrength = 0.0f;
    
    WindField3D wind(windConfig);

    Vector3D startPos(0.0f, 100.0f, 0.0f);
    
    CharacterConfig3D charConfig;
    charConfig.radius = 6.0f;
    charConfig.maxSpeed = 160.0f;
    charConfig.acceleration = 120.0f;
    charConfig.drag = 0.985f;
    charConfig.trailLength = 20;
    charConfig.rotationSpeed = 5.0f;
    
    Character3D character(startPos, charConfig);

    CapeConfig3D capeConfig;
    capeConfig.segments = 14;
    capeConfig.width = 10;
    capeConfig.segmentLength = 4.5f;
    capeConfig.widthSpacing = 3.0f;
    capeConfig.stiffness = 0.85f;
    capeConfig.bendStiffness = 0.15f;
    capeConfig.gravity = 18.0f;
    capeConfig.windInfluence = 0.8f;
    capeConfig.damping = 0.992f;
    capeConfig.aerodynamicDrag = 0.02f;
    capeConfig.liftCoefficient = 0.15f;
    
    Cape3D cape(character.getCapeAttachPoint(), character.getForward() * -1.0f, capeConfig);

    // Smooth, responsive flight
    FlightConfig3D flightConfig;
    flightConfig.liftForce = 90.0f;
    flightConfig.diveForce = 40.0f;
    flightConfig.horizontalForce = 85.0f;
    flightConfig.glideRatio = 3.5f;
    flightConfig.windAssist = 0.0f;
    
    FlightController3D flight(&character, flightConfig);

    // Camera locked behind player
    FlightCameraConfig cameraConfig;
    cameraConfig.followDistance = 55.0f;
    cameraConfig.followHeight = 18.0f;
    cameraConfig.smoothSpeed = 6.0f;
    cameraConfig.fov = 65.0f;
    
    FlightCamera camera(startPos + Vector3D(0, 30, 80), startPos, cameraConfig);

    // Procedural terrain - dramatic mountains with desert sand
    TerrainConfig terrainConfig;
    terrainConfig.gridSize = 100;
    terrainConfig.tileSize = 18.0f;
    terrainConfig.maxHeight = 350.0f;
    terrainConfig.mountainFrequency = 0.004f;
    terrainConfig.duneFrequency = 0.015f;
    terrainConfig.mountainPower = 2.2f;
    terrainConfig.duneAmplitude = 18.0f;
    terrainConfig.mountainOctaves = 6;
    terrainConfig.duneOctaves = 4;
    terrainConfig.baseHeight = -80.0f;
    
    // Warm sand and cool rock colors
    terrainConfig.sandColorLight = {245, 230, 200, 255};
    terrainConfig.sandColorDark = {215, 190, 155, 255};
    terrainConfig.rockColorLight = {175, 155, 140, 255};
    terrainConfig.rockColorDark = {110, 95, 85, 255};
    terrainConfig.peakColor = {255, 250, 245, 255};
    terrainConfig.rockThreshold = 0.40f;
    terrainConfig.peakThreshold = 0.78f;
    
    Terrain terrain(terrainConfig);
    terrain.generate(12345);

    PerformanceMonitor perfMonitor;
    float time = 0.0f;
    bool showWindDebug = false;

    while (!renderer.shouldClose()) {
        perfMonitor.beginFrame();
        
        float dt = GetFrameTime();
        dt = std::min(dt, 0.033f);
        time += dt;

        flight.stopVertical();
        flight.stopHorizontal();
        
        if (IsKeyDown(KEY_W)) flight.moveForward();
        if (IsKeyDown(KEY_S)) flight.moveBackward();
        if (IsKeyDown(KEY_A)) flight.moveLeft();
        if (IsKeyDown(KEY_D)) flight.moveRight();
        if (IsKeyDown(KEY_SPACE)) flight.moveUp();
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) flight.moveDown();
        if (IsKeyPressed(KEY_E)) flight.boost();
        
        if (IsKeyPressed(KEY_V)) {
            showWindDebug = !showWindDebug;
            RenderConfig3D cfg = renderer.getConfig();
            cfg.showWindDebug = showWindDebug;
            renderer.setConfig(cfg);
        }
        
        if (IsKeyPressed(KEY_R)) {
            character.setPosition(Vector3D(0, 100, 0));
            character.setVelocity(Vector3D::zero());
        }

        // Camera is locked - no manual orbit

        wind.update(dt);
        flight.update(dt, wind);
        character.update(dt);

        // Ground collision with terrain
        Vector3D pos = character.getPosition();
        float groundHeight = terrain.getHeightAt(pos.x, pos.z) + character.getRadius() + 2.0f;
        if (pos.y < groundHeight) {
            pos.y = groundHeight;
            character.setPosition(pos);
            
            // Dampen vertical velocity on ground contact
            Vector3D vel = character.getVelocity();
            if (vel.y < 0) {
                vel.y *= -0.3f;  // Small bounce
                character.setVelocity(vel);
            }
        }

        Vector3D capeAttach = character.getCapeAttachPoint();
        Vector3D capeForward = character.getForward() * -1.0f;
        cape.setAttachPoint(capeAttach, capeForward);
        cape.setAttachVelocity(character.getVelocity());
        cape.update(dt, wind);
        cape.solveConstraints(6);

        camera.followTarget(character.getPosition(), character.getVelocity(), dt);

        renderer.beginFrame(camera);
        
        renderer.drawSky(camera, time);
        renderer.drawTerrain(terrain, camera);
        renderer.drawClouds(camera, time);
        renderer.drawAtmosphere(wind, dt, camera);
        renderer.drawWindField(wind, character.getPosition());
        renderer.drawTrail(character);
        renderer.drawCape(cape);
        renderer.drawCharacter(character);
        renderer.drawUI(flight, perfMonitor, camera);

        if (IsKeyDown(KEY_TAB)) {
            Vector3D pos = character.getPosition();
            Vector3D vel = character.getVelocity();
            DrawText(TextFormat("Pos: %.0f, %.0f, %.0f", pos.x, pos.y, pos.z), 20, 160, 14, WHITE);
            DrawText(TextFormat("Vel: %.0f, %.0f, %.0f", vel.x, vel.y, vel.z), 20, 180, 14, WHITE);
            DrawText(TextFormat("Speed: %.0f", character.getSpeed()), 20, 200, 14, WHITE);
            DrawText(TextFormat("Glide Eff: %.0f%%", flight.getGlideEfficiency() * 100), 20, 220, 14, WHITE);
        }
        
        renderer.endFrame();
        perfMonitor.endFrame();
    }

    renderer.shutdown();
    return 0;
}
