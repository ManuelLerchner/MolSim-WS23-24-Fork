#pragma once

#include "io/input/custom_formats/CustomFileReader.h"
#include "io/input/custom_formats/FileLineReader.h"

/**
 * @brief Class to read particle data from a '.cub' file
 */
class CubFileReader : public CustomFileReader {
   public:
    /**
     * @brief Reads the '.cub' file with the given path and fills the given ParticleContainer with a cuboid of particles
     * using the arguments stored in the file (see README.md or the extensive description for details on the .cub file format)
     * @param filepath Path to the file to be read
     * @param particle_container ParticleContainer to be filled
     *
     * Reads the '.cub' file with the given path and fills the given ParticleContainer with cuboids of particles
     * using the arguments stored in the file (see \ref InputFileFormats "Input File Formats")
     */
    void readFile(const std::string& filepath, ParticleContainer& particle_container) const override;

   private:
    /**
     * @brief Reports an invalid entry in the given file and terminates the full program
     * @param input_file FileLineReader which read the entry
     * @param expected_format Expected format of the entry
     */
    void checkAndReportInvalidEntry(FileLineReader& input_file, const std::string& expected_format) const;
};