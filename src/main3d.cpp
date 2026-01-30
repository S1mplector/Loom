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
    
    // Desert sky colors
    renderConfig.skyColorTop = {135, 180, 220, 255};
    renderConfig.skyColorBottom = {245, 235, 220, 255};
    renderConfig.sunColor = {255, 245, 220, 255};
    renderConfig.cloudColor = {255, 255, 255, 80};
    renderConfig.sunDirection = Vector3D(0.4f, 0.9f, 0.2f);
    
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
    charConfig.radius = 8.0f;
    charConfig.maxSpeed = 220.0f;
    charConfig.acceleration = 180.0f;
    charConfig.drag = 0.992f;  // Smoother, less drag
    charConfig.trailLength = 30;
    charConfig.rotationSpeed = 3.5f;  // Smoother rotation
    
    Character3D character(startPos, charConfig);

    CapeConfig3D capeConfig;
    capeConfig.segments = 16;
    capeConfig.width = 12;
    capeConfig.segmentLength = 5.0f;
    capeConfig.widthSpacing = 3.5f;
    capeConfig.stiffness = 0.9f;
    capeConfig.bendStiffness = 0.2f;
    capeConfig.gravity = 22.0f;
    capeConfig.windInfluence = 1.6f;
    capeConfig.damping = 0.988f;
    capeConfig.aerodynamicDrag = 0.025f;
    capeConfig.liftCoefficient = 0.25f;
    
    Cape3D cape(character.getCapeAttachPoint(), character.getForward() * -1.0f, capeConfig);

    // Smooth, floaty flight like Sky
    FlightConfig3D flightConfig;
    flightConfig.liftForce = 80.0f;
    flightConfig.diveForce = 50.0f;
    flightConfig.horizontalForce = 70.0f;
    flightConfig.glideRatio = 4.0f;
    flightConfig.windAssist = 0.0f;
    
    FlightController3D flight(&character, flightConfig);

    // Camera locked behind player (behind cape)
    FlightCameraConfig cameraConfig;
    cameraConfig.followDistance = 45.0f;
    cameraConfig.followHeight = 12.0f;
    cameraConfig.smoothSpeed = 8.0f;  // Faster follow for locked feel
    cameraConfig.fov = 70.0f;
    
    FlightCamera camera(startPos + Vector3D(0, 30, 80), startPos, cameraConfig);

    // Procedural terrain with low-poly mountains and sand
    TerrainConfig terrainConfig;
    terrainConfig.gridSize = 80;
    terrainConfig.tileSize = 15.0f;
    terrainConfig.maxHeight = 280.0f;
    terrainConfig.mountainFrequency = 0.006f;
    terrainConfig.duneFrequency = 0.02f;
    terrainConfig.mountainPower = 2.8f;
    terrainConfig.duneAmplitude = 12.0f;
    terrainConfig.baseHeight = -60.0f;
    
    // Sand/desert colors
    terrainConfig.sandColorLight = {240, 225, 190, 255};
    terrainConfig.sandColorDark = {210, 185, 145, 255};
    terrainConfig.rockColorLight = {180, 160, 140, 255};
    terrainConfig.rockColorDark = {120, 105, 90, 255};
    terrainConfig.peakColor = {250, 245, 240, 255};
    terrainConfig.rockThreshold = 0.45f;
    terrainConfig.peakThreshold = 0.82f;
    
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
