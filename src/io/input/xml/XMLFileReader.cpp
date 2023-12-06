#include "XMLFileReader.h"

#include <optional>
#include <sstream>

#include "io/input/xsd_type_adaptors/XSDToInternalTypeAdapter.h"
#include "io/logger/Logger.h"
#include "particles/containers/directsum/DirectSumContainer.h"
#include "particles/containers/linkedcells/LinkedCellsContainer.h"
#include "particles/spawners/cuboid/CuboidSpawner.h"

SimulationParams XMLFileReader::readFile(const std::string& filepath, std::unique_ptr<ParticleContainer>& particle_container) const {
    try {
        auto config = configuration_(filepath);

        auto settings = config->settings();
        auto particles = config->particles();

        // Create particle container
        auto container_type = XSDToInternalTypeAdapter::convertToParticleContainer(settings.particle_container());
        if (std::holds_alternative<SimulationParams::LinkedCellsType>(container_type)) {
            auto linked_cells = std::get<SimulationParams::LinkedCellsType>(container_type);
            particle_container = std::make_unique<LinkedCellsContainer>(linked_cells.domain_size, linked_cells.cutoff_radius,
                                                                        linked_cells.boundary_conditions);
        } else if (std::holds_alternative<SimulationParams::DirectSumType>(container_type)) {
            particle_container = std::make_unique<DirectSumContainer>();
        } else {
            throw std::runtime_error("Unknown container type");
        }

        // Spawn particles specified in the XML file
        for (auto xsd_cuboid : particles.cuboid_spawner()) {
            auto spawner = XSDToInternalTypeAdapter::convertToCuboidSpawner(xsd_cuboid, settings.third_dimension());
            particle_container->reserve(particle_container->size() + spawner.getEstimatedNumberOfParticles());
            spawner.spawnParticles(particle_container);
        }

        for (auto xsd_sphere : particles.sphere_spawner()) {
            auto spawner = XSDToInternalTypeAdapter::convertToSphereSpawner(xsd_sphere, settings.third_dimension());
            particle_container->reserve(particle_container->size() + spawner.getEstimatedNumberOfParticles());
            spawner.spawnParticles(particle_container);
        }

        for (auto xsd_single_particle_spawner : particles.single_particle_spawner()) {
            auto spawner =
                XSDToInternalTypeAdapter::convertToSingleParticleSpawner(xsd_single_particle_spawner, settings.third_dimension());
            spawner.spawnParticles(particle_container);
        }

        return SimulationParams{filepath,
                                "",
                                settings.delta_t(),
                                settings.end_time(),
                                static_cast<int>(settings.fps()),
                                static_cast<int>(settings.video_length()),
                                container_type,
                                "vtk"};
    } catch (const xml_schema::exception& e) {
        std::stringstream error_message;
        error_message << "Error: could not parse file '" << filepath << "'.\n";
        error_message << e << std::endl;
        throw FileFormatException(error_message.str());
    }
}