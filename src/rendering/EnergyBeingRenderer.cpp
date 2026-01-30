#include "EnergyBeingRenderer.hpp"
#include "rlgl.h"
#include <algorithm>
#include <random>

namespace ethereal {

EnergyBeingRenderer::EnergyBeingRenderer() : EnergyBeingRenderer(EnergyBeingConfig{}) {}

EnergyBeingRenderer::EnergyBeingRenderer(const EnergyBeingConfig& cfg)
    : config(cfg)
    , time(0.0f)
    , lastSpeed(0.0f)
    , lastPosition(Vector3D::zero())
    , smoothedVelocity(Vector3D::zero()) {
}

void EnergyBeingRenderer::initialize() {
    createOrbs();
}

void EnergyBeingRenderer::createOrbs() {
    orbs.clear();
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> phaseDist(0.0f, 6.28318f);
    std::uniform_real_distribution<float> brightDist(0.8f, 1.0f);
    
    // Core orbs - tightest cluster
    for (int i = 0; i < config.coreOrbs; ++i) {
        EnergyOrb orb;
        float angle = (float)i / config.coreOrbs * 6.28318f;
        orb.localPosition = Vector3D(
            std::cos(angle) * config.coreSpread * 0.3f,
            std::sin(phaseDist(rng)) * config.coreSpread * 0.2f,
            std::sin(angle) * config.coreSpread * 0.3f
        );
        orb.targetPosition = orb.localPosition;
        orb.velocity = Vector3D::zero();
        orb.radius = config.coreRadius;
        orb.phase = phaseDist(rng);
        orb.brightness = brightDist(rng);
        orb.layer = 0;
        orbs.push_back(orb);
    }
    
    // Mid orbs - medium spread
    for (int i = 0; i < config.midOrbs; ++i) {
        EnergyOrb orb;
        float angle = (float)i / config.midOrbs * 6.28318f + 0.3f;
        float height = std::sin(phaseDist(rng)) * 0.5f;
        orb.localPosition = Vector3D(
            std::cos(angle) * config.midSpread * 0.4f,
            height * config.midSpread * 0.3f,
            std::sin(angle) * config.midSpread * 0.4f
        );
        orb.targetPosition = orb.localPosition;
        orb.velocity = Vector3D::zero();
        orb.radius = config.midRadius;
        orb.phase = phaseDist(rng);
        orb.brightness = brightDist(rng);
        orb.layer = 1;
        orbs.push_back(orb);
    }
    
    // Outer orbs - widest spread
    for (int i = 0; i < config.outerOrbs; ++i) {
        EnergyOrb orb;
        float angle = (float)i / config.outerOrbs * 6.28318f + 0.7f;
        float height = std::sin(phaseDist(rng) + i);
        orb.localPosition = Vector3D(
            std::cos(angle) * config.outerSpread * 0.5f,
            height * config.outerSpread * 0.3f,
            std::sin(angle) * config.outerSpread * 0.5f
        );
        orb.targetPosition = orb.localPosition;
        orb.velocity = Vector3D::zero();
        orb.radius = config.outerRadius;
        orb.phase = phaseDist(rng);
        orb.brightness = brightDist(rng);
        orb.layer = 2;
        orbs.push_back(orb);
    }
}

void EnergyBeingRenderer::update(float dt, const Character3D& character) {
    time += dt;
    
    Vector3D currentPos = character.getPosition();
    Vector3D currentVel = character.getVelocity();
    float speed = currentVel.length();
    
    // Smooth velocity for more organic response
    smoothedVelocity = smoothedVelocity + (currentVel - smoothedVelocity) * std::min(dt * 5.0f, 1.0f);
    
    updateOrbPositions(dt, speed, smoothedVelocity);
    
    lastSpeed = speed;
    lastPosition = currentPos;
}

void EnergyBeingRenderer::updateOrbPositions(float dt, float speed, const Vector3D& velocity) {
    // Speed factor: 0 = stationary, 1 = full speed
    float speedFactor = std::min(speed / 150.0f, 1.0f);
    float smoothSpeedFactor = smoothstep(0.0f, 1.0f, speedFactor);
    
    // Velocity direction for flow
    Vector3D velDir = velocity.lengthSquared() > 0.01f ? velocity.normalized() : Vector3D(0, 0, 1);
    
    for (size_t i = 0; i < orbs.size(); ++i) {
        EnergyOrb& orb = orbs[i];
        
        // Calculate spread based on layer and speed
        float layerSpread = 1.0f;
        float rotSpeed = config.rotationSpeed;
        
        switch (orb.layer) {
            case 0: // Core - stays tight, rotates slowly
                layerSpread = config.coreSpread * (0.2f + smoothSpeedFactor * 0.8f);
                rotSpeed *= 0.5f;
                break;
            case 1: // Mid - moderate spread
                layerSpread = config.midSpread * (0.3f + smoothSpeedFactor * 0.7f);
                rotSpeed *= 0.8f;
                break;
            case 2: // Outer - maximum spread when moving
                layerSpread = config.outerSpread * (0.2f + smoothSpeedFactor * 1.0f);
                rotSpeed *= 1.2f;
                break;
        }
        
        // Rotation around center
        float orbAngle = orb.phase + time * rotSpeed * (1.0f + smoothSpeedFactor * 0.5f);
        float orbHeight = std::sin(orbAngle * 0.7f + orb.phase) * layerSpread * 0.3f;
        
        // Base circular position
        Vector3D basePos(
            std::cos(orbAngle) * layerSpread,
            orbHeight,
            std::sin(orbAngle) * layerSpread
        );
        
        // Flow effect - orbs trail behind when moving
        if (smoothSpeedFactor > 0.1f) {
            float flowOffset = (orb.layer + 1) * config.trailLength * smoothSpeedFactor;
            basePos = basePos - velDir * flowOffset * (1.0f + std::sin(orb.phase + time * 3.0f) * 0.3f);
        }
        
        // Organic wobble
        float wobble = std::sin(time * config.flowSpeed + orb.phase) * 0.5f;
        basePos.x += wobble * (1.0f - smoothSpeedFactor * 0.5f);
        basePos.y += std::cos(time * config.flowSpeed * 0.7f + orb.phase) * 0.3f;
        basePos.z += std::sin(time * config.flowSpeed * 1.3f + orb.phase) * 0.4f;
        
        orb.targetPosition = basePos;
        
        // Smooth movement toward target
        float moveSpeed = smoothSpeedFactor > 0.3f ? config.separateSpeed : config.mergeSpeed;
        Vector3D toTarget = orb.targetPosition - orb.localPosition;
        orb.velocity = orb.velocity + toTarget * moveSpeed * dt;
        orb.velocity = orb.velocity * 0.9f; // Damping
        orb.localPosition = orb.localPosition + orb.velocity * dt;
        
        // Update brightness with pulse
        float pulse = std::sin(time * config.pulseSpeed + orb.phase) * 0.15f + 0.85f;
        orb.brightness = pulse * (0.8f + smoothSpeedFactor * 0.2f);
    }
}

void EnergyBeingRenderer::render(const Character3D& character) {
    Vector3D center = character.getPosition();
    float speed = character.getVelocity().length();
    float speedFactor = std::min(speed / 150.0f, 1.0f);
    
    // Render glow first (background)
    renderGlow(center, speedFactor);
    
    // Render connections between nearby orbs
    renderConnections(center, speedFactor);
    
    // Render orbs back to front (outer first, core last)
    for (int layer = 2; layer >= 0; --layer) {
        for (const auto& orb : orbs) {
            if (orb.layer == layer) {
                renderOrb(orb, center, speedFactor);
            }
        }
    }
}

void EnergyBeingRenderer::renderOrb(const EnergyOrb& orb, const Vector3D& center, float speedFactor) {
    Vector3D worldPos = center + orb.localPosition;
    
    // Size pulses slightly
    float sizePulse = 1.0f + std::sin(time * 3.0f + orb.phase) * 0.1f;
    float radius = orb.radius * sizePulse * orb.brightness;
    
    // Color based on layer
    Color baseColor;
    switch (orb.layer) {
        case 0: baseColor = config.coreColor; break;
        case 1: baseColor = config.midColor; break;
        default: baseColor = config.outerColor; break;
    }
    
    // Brightness adjustment
    unsigned char alpha = (unsigned char)(baseColor.a * orb.brightness);
    
    // Multiple layers for soft appearance
    // Outer glow
    DrawSphere({worldPos.x, worldPos.y, worldPos.z}, radius * 2.0f, 
               {baseColor.r, baseColor.g, baseColor.b, (unsigned char)(alpha * 0.15f)});
    
    // Mid glow
    DrawSphere({worldPos.x, worldPos.y, worldPos.z}, radius * 1.4f, 
               {baseColor.r, baseColor.g, baseColor.b, (unsigned char)(alpha * 0.4f)});
    
    // Core
    DrawSphere({worldPos.x, worldPos.y, worldPos.z}, radius, 
               {baseColor.r, baseColor.g, baseColor.b, alpha});
    
    // Bright center for core orbs
    if (orb.layer == 0) {
        DrawSphere({worldPos.x, worldPos.y, worldPos.z}, radius * 0.5f, 
                   {255, 255, 255, (unsigned char)(alpha * 0.9f)});
    }
}

void EnergyBeingRenderer::renderGlow(const Vector3D& center, float speedFactor) {
    // Overall ambient glow around the being
    float glowPulse = 1.0f + std::sin(time * 1.5f) * 0.1f;
    float glowSize = 12.0f * glowPulse * (1.0f + speedFactor * 0.3f);
    
    // Layered glow for soft effect
    DrawSphere({center.x, center.y, center.z}, glowSize * 1.5f, 
               {config.glowColor.r, config.glowColor.g, config.glowColor.b, 8});
    DrawSphere({center.x, center.y, center.z}, glowSize * 1.2f, 
               {config.glowColor.r, config.glowColor.g, config.glowColor.b, 15});
    DrawSphere({center.x, center.y, center.z}, glowSize * 0.9f, 
               {config.glowColor.r, config.glowColor.g, config.glowColor.b, 25});
    
    // Speed-based energy burst
    if (speedFactor > 0.5f) {
        float burstAlpha = (speedFactor - 0.5f) * 2.0f * 20.0f;
        DrawSphere({center.x, center.y, center.z}, glowSize * 1.8f, 
                   {255, 240, 200, (unsigned char)burstAlpha});
    }
}

void EnergyBeingRenderer::renderConnections(const Vector3D& center, float speedFactor) {
    // When merged (low speed), draw soft connections between close orbs
    if (speedFactor > 0.6f) return; // Skip when moving fast
    
    float connectionStrength = 1.0f - speedFactor * 1.5f;
    connectionStrength = std::max(0.0f, connectionStrength);
    
    for (size_t i = 0; i < orbs.size(); ++i) {
        for (size_t j = i + 1; j < orbs.size(); ++j) {
            const EnergyOrb& a = orbs[i];
            const EnergyOrb& b = orbs[j];
            
            // Only connect orbs that are close
            float dist = (a.localPosition - b.localPosition).length();
            float maxDist = (a.radius + b.radius) * 4.0f;
            
            if (dist < maxDist) {
                float alpha = (1.0f - dist / maxDist) * connectionStrength * 60.0f;
                if (alpha < 5.0f) continue;
                
                Vector3D worldA = center + a.localPosition;
                Vector3D worldB = center + b.localPosition;
                Vector3D mid = (worldA + worldB) * 0.5f;
                
                // Draw soft blob at midpoint
                float midSize = (a.radius + b.radius) * 0.3f * (1.0f - dist / maxDist);
                DrawSphere({mid.x, mid.y, mid.z}, midSize * 1.5f, 
                          {255, 245, 220, (unsigned char)(alpha * 0.5f)});
                DrawSphere({mid.x, mid.y, mid.z}, midSize, 
                          {255, 250, 240, (unsigned char)alpha});
            }
        }
    }
}

float EnergyBeingRenderer::smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vector3D EnergyBeingRenderer::lerpVector(const Vector3D& a, const Vector3D& b, float t) {
    return a + (b - a) * t;
}

} // namespace ethereal
