#pragma once
#include "raylib.h"
#include "core/Vector3D.hpp"
#include "environment/Terrain.hpp"
#include "entities/Camera3D.hpp"
#include "physics/WindField3D.hpp"
#include <vector>

namespace ethereal {

struct EnvironmentConfig {
    // Sky
    Color skyColorZenith = {90, 140, 200, 255};
    Color skyColorHorizon = {220, 200, 180, 255};
    Color sunColor = {255, 250, 240, 255};
    Vector3D sunDirection = Vector3D(0.4f, 0.7f, 0.3f);
    float sunSize = 35.0f;
    float sunGlowSize = 120.0f;
    
    // Fog
    float fogDensity = 0.0008f;
    float fogStart = 150.0f;
    float fogEnd = 1200.0f;
    Color fogColor = {230, 220, 210, 255};
    
    // Clouds
    int cloudLayers = 3;
    int cloudsPerLayer = 8;
    float cloudBaseHeight = 200.0f;
    float cloudLayerSpacing = 60.0f;
    
    // Terrain
    float terrainViewDistance = 1000.0f;
    float terrainLodDistance = 400.0f;
    bool smoothShading = true;
    
    // Atmosphere particles
    int atmosphereParticles = 200;
    float particleSpawnRadius = 400.0f;
};

struct AtmosphereParticle3D {
    Vector3D position;
    Vector3D velocity;
    float size;
    float alpha;
    float lifetime;
    int type; // 0=dust, 1=sparkle, 2=wisp
};

class EnvironmentRenderer {
public:
    EnvironmentRenderer();
    explicit EnvironmentRenderer(const EnvironmentConfig& config);
    
    void initialize();
    void update(float dt, const Vector3D& cameraPos, const WindField3D& wind);
    
    void renderSky(const FlightCamera& camera, float time);
    void renderTerrain(const Terrain& terrain, const FlightCamera& camera);
    void renderClouds(const FlightCamera& camera, float time);
    void renderAtmosphere(const FlightCamera& camera, float dt);
    void renderDistantMountains(const FlightCamera& camera, float time);
    
    void setConfig(const EnvironmentConfig& cfg) { config = cfg; }
    const EnvironmentConfig& getConfig() const { return config; }

private:
    EnvironmentConfig config;
    std::vector<AtmosphereParticle3D> particles;
    float time;
    
    void initParticles(const Vector3D& center);
    void updateParticles(float dt, const Vector3D& cameraPos, const WindField3D& wind);
    
    Color blendColors(Color a, Color b, float t);
    Color applyFog(Color color, float distance, float height);
    Color getTerrainColor(float height, float steepness);
    float calculateLighting(const Vector3D& normal);
};

} // namespace ethereal
