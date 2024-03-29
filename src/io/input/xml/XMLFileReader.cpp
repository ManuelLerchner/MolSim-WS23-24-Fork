#include "XMLFileReader.h"

#include <spdlog/fmt/bundled/core.h>

#include <filesystem>
#include <optional>

#include "io/input/chkpt/ChkptPointFileReader.h"
#include "io/logger/Logger.h"
#include "io/output/FileOutputHandler.h"
#include "io/output/chkpt/CheckPointWriter.h"
#include "io/xml_schemas/xsd_type_adaptors/XSDToInternalTypeAdapter.h"
#include "simulation/Simulation.h"
#include "utils/Enums.h"

std::string trim(const std::string& str) {
    // skip whitespace and newlines
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    auto end = str.find_last_not_of(" \t\n\r\f\v");

    if (start == std::string::npos) {
        return "";
    } else {
        return str.substr(start, end - start + 1);
    }
}

std::filesystem::path sanitizePath(const std::string& text) {
    std::string sanitized = trim(text);

    // Remove trailing slashes
    if (sanitized.back() == '/') {
        sanitized.pop_back();
    }

    // replace spaces with underscores
    std::replace(sanitized.begin(), sanitized.end(), ' ', '_');

    // to lower case
    std::transform(sanitized.begin(), sanitized.end(), sanitized.begin(), [](unsigned char c) { return std::tolower(c); });

    return sanitized;
}

std::filesystem::path convertToPath(const std::filesystem::path& base_path, const std::filesystem::path& path) {
    auto is_relative_path = path.is_relative();

    if (is_relative_path) {
        return base_path / path;
    } else {
        return path;
    }
}

int loadCheckpointFile(std::vector<Particle>& particles, const std::filesystem::path& path) {
    std::string file_extension = path.extension().string();
    if (file_extension != ".chkpt") {
        Logger::logger->error("Error: file extension '{}' is not supported. Only .chkpt files can be used as checkpoints.", file_extension);
        throw FileReader::FileFormatException("File extension is not supported");
    }

    ChkptPointFileReader reader;
    auto [loaded_particles, iteration] = reader.readFile(path);
    particles.insert(particles.end(), loaded_particles.begin(), loaded_particles.end());

    Logger::logger->info("Loaded {} particles from checkpoint file {}", loaded_particles.size(), path.string());

    return iteration;
}

std::optional<std::filesystem::path> loadLatestValidCheckpointFromFolder(const std::filesystem::path& base_path) {
    if (!std::filesystem::exists(base_path)) {
        return std::nullopt;
    }

    std::optional<std::filesystem::path> check_point_path = std::nullopt;
    size_t best_iteration = 0;
    auto directory_iterator = std::filesystem::directory_iterator(base_path);

    std::set<std::filesystem::path> entries;
    for (auto& entry : directory_iterator) {
        if (entry.path().extension() == ".chkpt") {
            entries.insert(entry.path());
        }
    }

    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        auto& entry = *it;

        std::string filename = entry.filename().string();
        std::string iteration_str =
            filename.substr(filename.find_last_of("_") + 1, filename.find_last_of(".") - filename.find_last_of("_") - 1);

        size_t current_file_number = std::stoul(iteration_str);

        if (current_file_number > best_iteration) {
            try {
                auto hash_valid = ChkptPointFileReader::detectSourceFileChanges(entry.string());

                if (!hash_valid) {
                    Logger::logger->warn(
                        "The input file for the checkpoint file {} has changed since the checkpoint file was created. Skipping.",
                        entry.string());
                    continue;
                }

                best_iteration = current_file_number;
                check_point_path = entry;
            } catch (const FileReader::FileFormatException& e) {
                Logger::logger->warn("Error: Could not read checkpoint file {}. Skipping.", entry.string());
            }
        }
    }

    return check_point_path;
}

auto loadConfig(const SubSimulationType& sub_simulation, const std::filesystem::path& curr_file_path,
                const std::filesystem::path& base_path) {
    // Configuration is in a separate file
    auto other_file_name = convertToPath(base_path, std::filesystem::path(std::string(sub_simulation.path())));

    std::string file_extension = other_file_name.extension().string();
    if (file_extension != ".xml") {
        Logger::logger->error("Error: file extension '{}' is not supported. Only .xml files can be used as sub simulations.",
                              file_extension);
        throw FileReader::FileFormatException("File extension is not supported");
    }

    auto config = configuration(other_file_name);
    return std::make_pair(*config, other_file_name);
}

std::tuple<std::vector<Particle>, SimulationParams> prepareParticles(std::filesystem::path curr_file_path, ConfigurationType& config,
                                                                     bool fresh, bool allow_recursion,
                                                                     std::filesystem::path output_base_path = "", int depth = 0) {
    Logger::logger->info("Constructing configuration for file {} at depth {}", curr_file_path.string(), depth);

    auto settings = config.settings();
    auto particle_sources = config.particle_source();

    ThirdDimension third_dimension = settings.third_dimension() ? ThirdDimension::ENABLED : ThirdDimension::DISABLED;

    std::vector<Particle> particles;

    auto container_type = XSDToInternalTypeAdapter::convertToParticleContainer(settings.particle_container());

    auto interceptors = XSDToInternalTypeAdapter::convertToSimulationInterceptors(settings.interceptors(), third_dimension, container_type);

    auto forces = XSDToInternalTypeAdapter::convertToForces(settings.forces(), container_type);

    auto params = SimulationParams{curr_file_path,      settings.delta_t(),  settings.end_time(), container_type, interceptors,
                                   std::get<0>(forces), std::get<1>(forces), std::get<2>(forces), fresh,          output_base_path};

    if (output_base_path.empty()) {
        output_base_path = params.output_dir_path;
    }

    auto curr_folder = std::filesystem::path(curr_file_path).parent_path().string();

    bool load_in_spawners = true;

    // try to load latest checkpoint file and continue from there
    auto latest_checkpoint_path = loadLatestValidCheckpointFromFolder(params.output_dir_path);
    if (latest_checkpoint_path.has_value()) {
        Logger::logger->warn("Found checkpoint file {}", latest_checkpoint_path.value().string());
        Logger::logger->warn("Continue simulation from checkpoint?");
        Logger::logger->warn("  [y] Continue from checkpoint        [n] Start from scratch");

        char answer;
        while (true) {
            std::cin >> answer;
            if (std::cin.fail() || std::cin.eof()) {
                std::cin.clear();
                continue;
            }

            if (answer != 'y' && answer != 'n') {
                Logger::logger->warn("Invalid input. Please enter 'y' or 'n'");
                continue;
            } else {
                break;
            }
        }

        if (answer == 'y') {
            int end_iteration = loadCheckpointFile(particles, *latest_checkpoint_path);

            params.start_iteration = end_iteration;
            load_in_spawners = false;

            Logger::logger->warn("Continuing from checkpoint file {} with iteration {}", latest_checkpoint_path.value().string(),
                                 params.start_iteration);
        } else {
            Logger::logger->warn("Starting simulation from scratch");
        }
    } else {
        Logger::logger->warn("Error: No valid checkpoint file found in output directory {}", params.output_dir_path.string());
    }

    if (load_in_spawners) {
        // Spawn particles specified in the XML file
        for (auto cuboid_spawner : particle_sources.cuboid_spawner()) {
            auto spawner = XSDToInternalTypeAdapter::convertToCuboidSpawner(cuboid_spawner, third_dimension);
            int num_spawned = spawner.spawnParticles(particles);
            Logger::logger->info("Spawned {} particles from cuboid spawner", num_spawned);
        }

        for (auto soft_body_cuboid_spawner : particle_sources.soft_body_cuboid_spawner()) {
            // if container has outflow boundaries
            if (std::holds_alternative<SimulationParams::LinkedCellsType>(container_type)) {
                auto container = std::get<SimulationParams::LinkedCellsType>(container_type);
                if (std::find(container.boundary_conditions.begin(), container.boundary_conditions.end(),
                              LinkedCellsContainer::BoundaryCondition::OUTFLOW) != container.boundary_conditions.end()) {
                    throw FileReader::FileFormatException("Soft body cuboid spawner is not supported with outflow boundary conditions");
                }
            }

            auto spawner = XSDToInternalTypeAdapter::convertToSoftBodyCuboidSpawner(soft_body_cuboid_spawner, third_dimension);
            int num_spawned = spawner.spawnParticles(particles);
            Logger::logger->info("Spawned {} particles from soft body cuboid spawner", num_spawned);
        }

        for (auto sphere_spawner : particle_sources.sphere_spawner()) {
            auto spawner = XSDToInternalTypeAdapter::convertToSphereSpawner(sphere_spawner, third_dimension);
            int num_spawned = spawner.spawnParticles(particles);
            Logger::logger->info("Spawned {} particles from sphere spawner", num_spawned);
        }

        for (auto single_particle_spawner : particle_sources.single_particle_spawner()) {
            auto spawner = XSDToInternalTypeAdapter::convertToSingleParticleSpawner(single_particle_spawner, third_dimension);
            int num_spawned = spawner.spawnParticles(particles);
            Logger::logger->info("Spawned {} particles from single particle spawner", num_spawned);
        }

        for (auto check_point_loader : particle_sources.check_point_loader()) {
            auto path = convertToPath(curr_folder, std::filesystem::path(std::string(check_point_loader.path())));
            loadCheckpointFile(particles, path);
        }

        for (auto sub_simulation : particle_sources.sub_simulation()) {
            if (!allow_recursion) {
                Logger::logger->warn("Error: Recursion is disabled. Skipping sub simulation at depth {}", depth);
                continue;
            }

            auto name = std::filesystem::path(std::string(sub_simulation.path())).stem().string();

            Logger::logger->info("Found sub simulation {} at depth {}", name, depth);

            std::filesystem::path new_output_base_path = output_base_path / sanitizePath(name);

            // Try to find a checkpoint file in the base directory
            auto checkpoint_path = fresh ? std::nullopt : loadLatestValidCheckpointFromFolder(new_output_base_path);

            // If no checkpoint file was found, run the sub simulation
            if (checkpoint_path.has_value()) {
                Logger::logger->warn(
                    "Using cached result for sub simulation {} at depth {}. To force a rerun, delete the checkpoint file at {}", name,
                    depth, checkpoint_path.value().string());
            }

            if (!checkpoint_path.has_value()) {
                Logger::logger->info("Starting sub simulation {} at depth {}", name, depth);

                // Load the configuration from the sub simulation
                auto [loaded_config, file_name] = loadConfig(sub_simulation, curr_file_path, curr_folder);

                // Create the initial conditions for the sub simulation
                auto [sub_particles, sub_config] =
                    prepareParticles(file_name, loaded_config, fresh, allow_recursion, new_output_base_path, depth + 1);
                sub_config.output_dir_path = new_output_base_path;

                // Run the sub simulation
                Simulation simulation{sub_particles, sub_config};

                sub_config.logSummary(depth);
                auto result = simulation.runSimulation();
                result.logSummary(depth);

                // Write the checkpoint file
                FileOutputHandler file_output_handler{OutputFormat::CHKPT, sub_config};

                checkpoint_path = file_output_handler.writeFile(result.total_iterations, result.resulting_particles);

                Logger::logger->info("Wrote {} particles to checkpoint file in: {}", result.resulting_particles.size(),
                                     (*checkpoint_path).string());
            }

            // Load the checkpoint file
            loadCheckpointFile(particles, *checkpoint_path);
        }
    }

    if (settings.log_level()) {
        Logger::update_level(settings.log_level().get());
    }

    params.num_particles = particles.size();

    return std::make_tuple(particles, std::move(params));
}

std::tuple<std::vector<Particle>, std::optional<SimulationParams>> XMLFileReader::readFile(const std::filesystem::path& filepath) const {
    try {
        auto config = configuration(filepath);

        return prepareParticles(filepath, *config, fresh, allow_recursion);
    } catch (const xml_schema::exception& e) {
        std::stringstream error_message;
        error_message << "Error: could not parse file '" << filepath << "'.\n";
        error_message << e << std::endl;
        throw FileFormatException(error_message.str());
    }
}