#include "Camera3D.hpp"
#include <cmath>
#include <algorithm>

namespace ethereal {

FlightCamera::FlightCamera() 
    : position(0, 50, 100)
    , target(0, 0, 0)
    , velocity(Vector3D::zero())
    , yaw(0)
    , pitch(0.3f)
    , currentDistance(80.0f)
    , shakeIntensity(0)
    , shakeDuration(0)
    , shakeTimer(0) {}

FlightCamera::FlightCamera(const Vector3D& position, const Vector3D& target, const FlightCameraConfig& config)
    : position(position)
    , target(target)
    , velocity(Vector3D::zero())
    , config(config)
    , yaw(0)
    , pitch(0.3f)
    , currentDistance(config.followDistance)
    , shakeIntensity(0)
    , shakeDuration(0)
    , shakeTimer(0) {
    
    Vector3D dir = position - target;
    yaw = std::atan2(dir.x, dir.z);
    pitch = std::asin(std::clamp(dir.y / dir.length(), -1.0f, 1.0f));
}

void FlightCamera::update(float dt) {
    if (shakeTimer < shakeDuration) {
        shakeTimer += dt;
    }
}

Vector3D FlightCamera::calculateIdealPosition(const Vector3D& targetPos, const Vector3D& targetVelocity) const {
    float speed = targetVelocity.length();
    float dynamicDistance = config.followDistance + speed * 0.05f;
    float dynamicHeight = config.followHeight + speed * 0.02f;
    
    float adjustedYaw = yaw;
    if (speed > 10.0f) {
        float targetYaw = std::atan2(targetVelocity.x, targetVelocity.z);
        float diff = targetYaw - yaw;
        while (diff > 3.14159f) diff -= 6.28318f;
        while (diff < -3.14159f) diff += 6.28318f;
        adjustedYaw = yaw + diff * 0.3f;
    }

    Vector3D offset;
    offset.x = std::sin(adjustedYaw) * std::cos(pitch) * dynamicDistance;
    offset.y = std::sin(pitch) * dynamicDistance + dynamicHeight;
    offset.z = std::cos(adjustedYaw) * std::cos(pitch) * dynamicDistance;
    
    return targetPos + offset;
}

void FlightCamera::followTarget(const Vector3D& targetPos, const Vector3D& targetVelocity, float dt) {
    target = target.lerp(targetPos, config.smoothSpeed * dt);
    
    Vector3D idealPos = calculateIdealPosition(targetPos, targetVelocity);
    position = position.lerp(idealPos, config.smoothSpeed * dt);
    
    update(dt);
}

void FlightCamera::orbit(float deltaYaw, float deltaPitch) {
    yaw += deltaYaw * config.orbitSpeed;
    pitch += deltaPitch * config.orbitSpeed;
    pitch = std::clamp(pitch, config.minPitch, config.maxPitch);
}

void FlightCamera::zoom(float delta) {
    currentDistance = std::clamp(currentDistance - delta * 10.0f, 20.0f, 200.0f);
    config.followDistance = currentDistance;
}

void FlightCamera::setTarget(const Vector3D& t) {
    target = t;
}

void FlightCamera::setPosition(const Vector3D& pos) {
    position = pos;
}

Vector3D FlightCamera::getForward() const {
    return (target - position).normalized();
}

Vector3D FlightCamera::getRight() const {
    return getForward().cross(Vector3D(0, 1, 0)).normalized();
}

Vector3D FlightCamera::getUp() const {
    return getRight().cross(getForward());
}

Matrix4 FlightCamera::getViewMatrix() const {
    Vector3D pos = position;
    if (shakeTimer < shakeDuration) {
        pos = pos + applyShake();
    }
    return Matrix4::lookAt(pos, target, Vector3D(0, 1, 0));
}

Matrix4 FlightCamera::getProjectionMatrix(float aspectRatio) const {
    return Matrix4::perspective(config.fov * 0.0174533f, aspectRatio, config.nearPlane, config.farPlane);
}

void FlightCamera::shake(float intensity, float duration) {
    shakeIntensity = intensity;
    shakeDuration = duration;
    shakeTimer = 0;
}

Vector3D FlightCamera::applyShake() const {
    if (shakeTimer >= shakeDuration) return Vector3D::zero();
    
    float t = shakeTimer / shakeDuration;
    float decay = 1.0f - t;
    float intensity = shakeIntensity * decay;
    
    float x = std::sin(shakeTimer * 50.0f) * intensity;
    float y = std::cos(shakeTimer * 43.0f) * intensity;
    float z = std::sin(shakeTimer * 37.0f) * intensity * 0.5f;
    
    return Vector3D(x, y, z);
}

} // namespace ethereal
