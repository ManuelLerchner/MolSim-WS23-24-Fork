#pragma once

#include <string>

#include "particles/ParticleContainer.h"
#include "simulation/SimulationParams.h"

/**
 * @brief Abstract base class for all custom file readers
 */
class FileReader {
   public:
    virtual ~FileReader() = default;

    /**
     * @brief Reads the file with the given path and fills the given ParticleContainer with the particle data stored in the file
     * @param filepath Path to the file to be read
     * @param particle_container ParticleContainer to be filled
     */
    virtual SimulationParams readFile(const std::string& filepath, ParticleContainer& particle_container) const = 0;

    /**
     * @brief Exception to be thrown when the file format is invalid
     */
    class FileFormatException : public std::exception {};
};