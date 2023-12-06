#pragma once

#include <memory>
#include <string>

#include "particles/containers/ParticleContainer.h"

/**
 * @brief Abstract base class for all file writers
 */
class FileWriter {
   public:
    /**
     * @brief virtual destructor for correct cleanup of derived classes
     */
    virtual ~FileWriter() = default;

    /**
     * @brief Writes the file to the given path, uses the given ParticleContainer and the current iteration
     *
     * @param output_dir_path Path to the directory in which to save the output
     * @param iteration The current iteration number
     * @param particle_container ParticleContainer which provides the data to be written
     */
    virtual void writeFile(const std::string& output_dir_path, int iteration,
                           const std::unique_ptr<ParticleContainer>& particle_container) const = 0;
};