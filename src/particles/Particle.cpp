/**
 * Particle.cpp
 *
 *  Created on: 23.02.2010
 *      Author: eckhardw
 */

#include "Particle.h"

#include <iostream>

#include "io/logger/Logger.h"
#include "utils/ArrayUtils.h"

Particle::Particle(const Particle& other) {
    x = other.x;
    v = other.v;
    f = other.f;
    old_f = other.old_f;
    m = other.m;
    type = other.type;
    epsilon = other.epsilon;
    sigma = other.sigma;
    lock_state = other.lock_state;
    connected_particles = other.connected_particles;
    Logger::logger->debug("Particle created");
}

Particle::Particle(std::array<double, 3> x_arg, std::array<double, 3> v_arg, double m_arg, int type_arg, double epsilon_arg,
                   double sigma_arg, LockState lock_state_arg) {
    x = x_arg;
    v = v_arg;
    m = m_arg;
    type = type_arg;
    f = {0., 0., 0.};
    epsilon = epsilon_arg;
    sigma = sigma_arg;
    lock_state = lock_state_arg;
    old_f = {0., 0., 0.};
    Logger::logger->debug("Particle created");
}

Particle::Particle(std::array<double, 3> x_arg, std::array<double, 3> v_arg, std::array<double, 3> f_arg, std::array<double, 3> old_f_arg,
                   double m_arg, int type_arg, double epsilon_arg, double sigma_arg, LockState lock_state_arg) {
    x = x_arg;
    v = v_arg;
    f = f_arg;
    old_f = old_f_arg;
    m = m_arg;
    type = type_arg;
    epsilon = epsilon_arg;
    sigma = sigma_arg;
    lock_state = lock_state_arg;
    Logger::logger->debug("Particle created");
}

Particle::~Particle() { Logger::logger->debug("Particle destroyed"); }

Particle& Particle::operator=(const Particle& other) {
    x = other.x;
    v = other.v;
    f = other.f;
    old_f = other.old_f;
    m = other.m;
    type = other.type;
    epsilon = other.epsilon;
    sigma = other.sigma;
    lock_state = other.lock_state;
    connected_particles = other.connected_particles;
    Logger::logger->debug("Particle created");
    return *this;
}

void Particle::addConnectedParticle(long ptr_diff, double l_0, double k) { connected_particles.push_back({ptr_diff, l_0, k}); }

std::string Particle::toString() const {
    std::stringstream stream;
    stream << "Particle: X:" << x << " v: " << v << " f: " << f << " old_f: " << old_f << " type: " << type;
    return stream.str();
}

bool Particle::operator==(Particle& other) {
    return (x == other.x) and (v == other.v) and (f == other.f) and (type == other.type) and (m == other.m) and (old_f == other.old_f);
}

bool Particle::operator==(const Particle& other) const {
    return (x == other.x) and (v == other.v) and (f == other.f) and (type == other.type) and (m == other.m) and (old_f == other.old_f);
}

std::ostream& operator<<(std::ostream& stream, Particle& p) {
    stream << p.toString();
    return stream;
}
