#pragma once

#include "io/input/FileReader.h"
#include "io/xml_schemas/simulation_input/simulation_input_schema.h"
#include "simulation/SimulationParams.h"

/**
 * @brief Class to read particle and simulation data from a '.xml' file
 */
class XMLFileReader : public FileReader {
   private:
    bool fresh;

   public:
    explicit XMLFileReader(bool fresh = false) : fresh(fresh) {}

    /**
     * @brief Reads particle data from a '.xml' file and returns a vector of particles
     * Other simulation parameters are returned as an optional SimulationParams object.
     *
     * @param filepath Path to the file to read
     */
    [[nodiscard]] std::tuple<std::vector<Particle>, std::optional<SimulationParams>> readFile(const std::string& filepath) const override;
};