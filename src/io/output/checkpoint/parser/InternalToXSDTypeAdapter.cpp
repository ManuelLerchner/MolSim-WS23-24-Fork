#include "InternalToXSDTypeAdapter.h"

#include <array>

#include "io/logger/Logger.h"
#include "utils/ArrayUtils.h"
#include "utils/MaxwellBoltzmannDistribution.h"

ParticleType InternalToXSDTypeAdapter::convertToParticle(const Particle& particle) {
    auto position = convertToVector(particle.getX());
    auto velocity = convertToVector(particle.getV());
    auto force = convertToVector(particle.getF());
    auto old_force = convertToVector(particle.getOldF());
    auto mass = particle.getM();
    auto type = particle.getType();

    return ParticleType{position, velocity, force, old_force, mass, type};
}

DoubleVec3Type InternalToXSDTypeAdapter::convertToVector(const std::array<double, 3>& vector) {
    return DoubleVec3Type{vector[0], vector[1], vector[2]};
}

IntVec3Type InternalToXSDTypeAdapter::convertToVector(const std::array<int, 3>& vector) {
    return IntVec3Type{vector[0], vector[1], vector[2]};
}
