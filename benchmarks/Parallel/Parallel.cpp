#include <omp.h>

#include <array>
#include <fstream>
#include <memory>
#include <vector>

#include "../FileLoader.h"
#include "io/input/FileInputHandler.h"
#include "io/logger/Logger.h"
#include "io/output/FileOutputHandler.h"
#include "io/output/chkpt/CheckPointWriter.h"
#include "particles/containers/ParticleContainer.h"
#include "particles/containers/directsum/DirectSumContainer.h"
#include "particles/containers/linkedcells/LinkedCellsContainer.h"
#include "particles/spawners/cuboid/CuboidSpawner.h"
#include "physics/pairwiseforces/GravitationalForce.h"
#include "physics/pairwiseforces/LennardJonesForce.h"
#include "simulation/Simulation.h"
#include "utils/ArrayUtils.h"

// #define TEST_DIRECT_SUM

int rect_width = 50;
int rect_height = 50;
int rect_depth = 1;
const double spacing = 1;
const double lc_cutoff = 3;

std::vector<Particle> reference_particles;

void executeSimulation(int t_count, const std::vector<Particle>& particles_model) {
    Logger::logger->set_level(spdlog::level::info);
    Logger::logger->info("Starting benchmark with {} threads", t_count);

    std::filesystem::create_directories("./output/ParallelBenchmark/DirectSum");
    std::filesystem::create_directories("./output/ParallelBenchmark/LinkedCells");

    // Initialize CheckPointWriter
    CheckPointWriter check_point_writer;

    // Parse input file
    auto [initial_particles, simulation_arguments] =
        FileInputHandler::readFile(FileLoader::get_input_file_path("Parallel/ParallelBenchmark.xml"), true);

    // Settings for the Linked Cells Container simulation
    std::array<double, 3> domain_size = {60, 60, 60};
    std::array<LinkedCellsContainer::BoundaryCondition, 6> boundary_conditions = {
        LinkedCellsContainer::BoundaryCondition::PERIODIC,   LinkedCellsContainer::BoundaryCondition::PERIODIC,
        LinkedCellsContainer::BoundaryCondition::REFLECTIVE, LinkedCellsContainer::BoundaryCondition::REFLECTIVE,
        LinkedCellsContainer::BoundaryCondition::PERIODIC,   LinkedCellsContainer::BoundaryCondition::PERIODIC};

    // ############################################################
    // # Direct Sum Container
    // ############################################################
#ifdef TEST_DIRECT_SUM
    // Instantiation of the Direct Sum Container simulation
    SimulationParams params_ds{*simulation_arguments};

    params_ds.num_particles = particles_model.size();

    Simulation simulation_ds(particles_model, params_ds);

    // Simulating with Direct Sum Container
    // params_ds.logSummary();
    SimulationOverview direct_sum_data = simulation_ds.runSimulation();
    // direct_sum_data.logSummary();
    direct_sum_data.savePerformanceDataCSV("DirectSum_" + std::to_string(t_count) + "_threads");
    params_ds.output_dir_path = "./output/ParallelBenchmark/DirectSum";
    check_point_writer.writeFile(params_ds, t_count, direct_sum_data.resulting_particles);

    if (t_count >= 1) {
        std::string serial_checkpoint_path = params_ds.output_dir_path / "MD_CHKPT_0001.chkpt";
        std::string curr_checkpoint_path = params_ds.output_dir_path / ("MD_CHKPT_000" + std::to_string(t_count) + ".chkpt");

        if (!std::filesystem::exists(serial_checkpoint_path)) {
            Logger::logger->error("Serial checkpoint file {} does not exist", serial_checkpoint_path);
        }
        if (!std::filesystem::exists(curr_checkpoint_path)) {
            Logger::logger->error("Current checkpoint file {} does not exist", curr_checkpoint_path);
        }

        // compare with version with 1 thread
        std::fstream serial_checkpoint{serial_checkpoint_path, std::ios::in};
        std::fstream curr_checkpoint{curr_checkpoint_path, std::ios::in};

        // compare the two streams byte by byte
        Logger::logger->info("Comparing files: {} and {}", serial_checkpoint_path, curr_checkpoint_path);

        bool equal = true;
        while (serial_checkpoint && curr_checkpoint) {
            if (serial_checkpoint.get() != curr_checkpoint.get()) {
                equal = false;
                break;
            }
        }

        if (equal) {
            Logger::logger->info("DirectSum Checkpoint on {} threads is equal to serial version", t_count);
        } else {
            Logger::logger->error("DirectSum Checkpoint on {} threads is NOT equal to serial version", t_count);
        }
    }
#endif

    // ############################################################
    // # Linked Cells Container
    // ############################################################

    // Instantiation of the Linked Cells Container simulation
    SimulationParams params_lc{*simulation_arguments};
    params_lc.container_type = SimulationParams::LinkedCellsType{domain_size, lc_cutoff, boundary_conditions};
    params_lc.num_particles = particles_model.size();

    Simulation simulation_lc(particles_model, params_lc);
    // Simulating with Linked Cells Container
    // params_lc.logSummary();
    SimulationOverview linked_cells_data = simulation_lc.runSimulation();
    // linked_cells_data.logSummary();
    linked_cells_data.savePerformanceDataCSV("LinkedCells_" + std::to_string(t_count) + "_threads");
    params_lc.output_dir_path = "./output/ParallelBenchmark/LinkedCells";
    check_point_writer.writeFile(params_lc, t_count, linked_cells_data.resulting_particles);

    if (t_count == 1) {
        reference_particles = linked_cells_data.resulting_particles;
    } else if (t_count > 1) {
        if (reference_particles.size() != linked_cells_data.resulting_particles.size()) {
            Logger::logger->error("LinkedCells Checkpoint on {} threads is NOT equal to serial version", t_count);
        } else {
            double max_diff_f = 0;
            double max_diff_x = 0;
            double max_diff_v = 0;

            for (size_t i = 0; i < reference_particles.size(); i++) {
                auto x_diff = ArrayUtils::L2Norm(reference_particles[i].getX() - linked_cells_data.resulting_particles[i].getX());
                auto v_diff = ArrayUtils::L2Norm(reference_particles[i].getV() - linked_cells_data.resulting_particles[i].getV());
                auto f_diff = ArrayUtils::L2Norm(reference_particles[i].getF() - linked_cells_data.resulting_particles[i].getF());

                if (x_diff > max_diff_x) {
                    max_diff_x = x_diff;
                }

                if (v_diff > max_diff_v) {
                    max_diff_v = v_diff;
                }

                if (f_diff > max_diff_f) {
                    max_diff_f = f_diff;
                }
            }

            Logger::logger->error("LinkedCells Checkpoint on {} threads is NOT equal to serial version", t_count);
            Logger::logger->error("max_diff_x: {}", max_diff_x);
            Logger::logger->error("max_diff_v: {}", max_diff_v);
            Logger::logger->error("max_diff_f: {}", max_diff_f);

            double delta = 1e-10;

            if (max_diff_x < delta && max_diff_v < delta && max_diff_f < delta) {
                Logger::logger->info("LinkedCells Checkpoint on {} threads is equal to serial version. Delta = {}", t_count, delta);
            }
        }
    }
}

/*
 * Creates a 2D particle rect with variable amount of particles and runs the simulation for time measurements.
 * Can be used to compare the performance of the different particle containers.
 */
int main(int argc, char** argv) {
    if (argc == 4) {
        int in_rect_width;
        int in_rect_height;
        int in_rect_depth;
        try {
            in_rect_width = std::stoi(argv[1]);
            in_rect_height = std::stoi(argv[2]);
            in_rect_depth = std::stoi(argv[3]);
        } catch (std::exception& e) {
            std::cout << "Error parsing arguments:" << e.what() << std::endl;
            exit(1);
        }

        if (in_rect_width > 0 && in_rect_height > 0 && in_rect_depth > 0) {
            rect_width = in_rect_width;
            rect_height = in_rect_height;
            rect_depth = in_rect_depth;
        } else {
            std::cout << "Error: Rectangle dimensions must be positive" << std::endl;
            std::cout << "Using standard dimensions: " << rect_width << "x" << rect_height << "x" << rect_depth << std::endl;
        }
    }

    // Create particles
    CuboidSpawner spawner(std::array<double, 3>{0.5, 0.5, 0.5}, {rect_width, rect_height, rect_depth}, spacing, 1, {0, 0, 0}, 0);

    std::vector<Particle> particles;
    int spawned_particles = spawner.spawnParticles(particles);
    std::cout << "Spawned " << spawned_particles << " particles" << std::endl;
    std::cout << "Grid of " << rect_width << "x" << rect_height << "x" << rect_depth << " particles" << std::endl;

    // Execution
    std::vector<int> num_threads = {1, 2, 4, 8, 14, 16, 28, 56};
    for (int t_count : num_threads) {
        omp_set_num_threads(t_count);
        executeSimulation(t_count, particles);
    }

    return 0;
}