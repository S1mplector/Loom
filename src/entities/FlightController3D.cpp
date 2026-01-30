#include "FlightController3D.hpp"
#include <cmath>
#include <algorithm>

namespace ethereal {

FlightController3D::FlightController3D()
    : character(nullptr)
    , state(FlightState3D::Gliding)
    , inputUp(false), inputDown(false), inputLeft(false), inputRight(false)
    , inputForward(false), inputBackward(false), isBoosting(false)
    , energy(100.0f), stateTimer(0.0f), boostTimer(0.0f) {}

FlightController3D::FlightController3D(Character3D* character, const FlightConfig3D& config)
    : character(character), config(config), state(FlightState3D::Gliding)
    , inputUp(false), inputDown(false), inputLeft(false), inputRight(false)
    , inputForward(false), inputBackward(false), isBoosting(false)
    , energy(100.0f), stateTimer(0.0f), boostTimer(0.0f) {}

void FlightController3D::update(float dt, const WindField3D& wind) {
    if (!character) return;

    updateState();
    applyGlidePhysics(dt);
    applyWindEffect(dt, wind);

    Vector3D forward = character->getForward();
    Vector3D right = character->getRight();
    Vector3D up(0, 1, 0);
    
    Vector3D force = Vector3D::zero();

    if (inputUp) {
        force += up * config.liftForce;
        energy -= dt * 15.0f;
    }
    if (inputDown) {
        force -= up * config.diveForce;
        energy += dt * 3.0f;
    }
    if (inputLeft) {
        force -= right * config.horizontalForce;
    }
    if (inputRight) {
        force += right * config.horizontalForce;
    }
    if (inputForward) {
        force += forward * config.horizontalForce;
    }
    if (inputBackward) {
        force -= forward * config.horizontalForce * 0.5f;
    }
    
    if (isBoosting && energy > 10.0f) {
        force += forward * config.horizontalForce * 2.0f;
        energy -= dt * 30.0f;
        boostTimer += dt;
        if (boostTimer > 0.5f) {
            isBoosting = false;
            boostTimer = 0;
        }
    }

    energy = std::clamp(energy, 0.0f, 100.0f);
    character->applyForce(force);
    stateTimer += dt;
}

void FlightController3D::updateState() {
    if (!character) return;

    const Vector3D& vel = character->getVelocity();
    float verticalSpeed = vel.y;
    float horizontalSpeed = Vector3D(vel.x, 0, vel.z).length();
    
    if (inputUp && energy > 0) {
        state = FlightState3D::Climbing;
    } else if (inputDown || verticalSpeed < -20.0f) {
        state = FlightState3D::Diving;
    } else if (horizontalSpeed > config.minGlideSpeed * 2.0f && std::abs(verticalSpeed) < 10.0f) {
        state = FlightState3D::Soaring;
    } else if (horizontalSpeed > config.minGlideSpeed || std::abs(verticalSpeed) > 5.0f) {
        state = FlightState3D::Gliding;
    } else {
        state = FlightState3D::Hovering;
    }
}

void FlightController3D::applyGlidePhysics(float dt) {
    if (!character) return;

    Vector3D vel = character->getVelocity();
    float speed = vel.length();
    float horizontalSpeed = Vector3D(vel.x, 0, vel.z).length();

    switch (state) {
        case FlightState3D::Climbing: {
            if (speed > config.minGlideSpeed) {
                float speedLoss = 1.0f - (1.0f - config.speedLossOnClimb) * dt * 60.0f;
                character->setVelocity(vel * speedLoss);
            }
            break;
        }
        case FlightState3D::Diving: {
            if (speed < config.maxGlideSpeed) {
                float speedGain = 1.0f + (config.speedGainOnDive - 1.0f) * dt * 60.0f;
                character->setVelocity(vel * speedGain);
            }
            character->applyForce(Vector3D(0, -50.0f, 0));
            break;
        }
        case FlightState3D::Soaring: {
            float lift = horizontalSpeed * config.altitudeGain * 0.5f;
            character->applyForce(Vector3D(0, lift, 0));
            break;
        }
        case FlightState3D::Gliding: {
            float gravity = 30.0f * (1.0f - std::min(speed / config.maxGlideSpeed, 1.0f) * 0.6f);
            character->applyForce(Vector3D(0, -gravity, 0));
            
            if (horizontalSpeed > config.minGlideSpeed) {
                float lift = horizontalSpeed * config.altitudeGain * 0.2f;
                character->applyForce(Vector3D(0, lift, 0));
            }
            break;
        }
        case FlightState3D::Hovering: {
            character->applyForce(Vector3D(0, -40.0f, 0));
            break;
        }
    }
}

void FlightController3D::applyWindEffect(float dt, const WindField3D& wind) {
    if (!character) return;

    Vector3D windForce = wind.getWindAt(character->getPosition());
    windForce *= config.windAssist;
    
    float turbulence = wind.getTurbulenceAt(character->getPosition()) * config.turbulenceEffect;
    Vector3D turbulenceForce = Vector3D(
        std::sin(stateTimer * 5.0f) * turbulence,
        std::cos(stateTimer * 7.0f) * turbulence * 0.5f,
        std::sin(stateTimer * 6.0f) * turbulence
    ) * 20.0f;

    character->applyForce(windForce + turbulenceForce);
}

void FlightController3D::moveUp() { inputUp = true; }
void FlightController3D::moveDown() { inputDown = true; }
void FlightController3D::moveLeft() { inputLeft = true; }
void FlightController3D::moveRight() { inputRight = true; }
void FlightController3D::moveForward() { inputForward = true; }
void FlightController3D::moveBackward() { inputBackward = true; }
void FlightController3D::stopVertical() { inputUp = false; inputDown = false; }
void FlightController3D::stopHorizontal() { inputLeft = false; inputRight = false; inputForward = false; inputBackward = false; }
void FlightController3D::boost() { isBoosting = true; boostTimer = 0; }

void FlightController3D::setCharacter(Character3D* c) { character = c; }

float FlightController3D::getGlideEfficiency() const {
    if (!character) return 0.0f;
    float speed = character->getSpeed();
    float optimalSpeed = (config.minGlideSpeed + config.maxGlideSpeed) * 0.35f;
    float diff = std::abs(speed - optimalSpeed) / optimalSpeed;
    return std::max(0.0f, 1.0f - diff);
}

float FlightController3D::getAltitude() const {
    if (!character) return 0.0f;
    return character->getPosition().y;
}

} // namespace ethereal
