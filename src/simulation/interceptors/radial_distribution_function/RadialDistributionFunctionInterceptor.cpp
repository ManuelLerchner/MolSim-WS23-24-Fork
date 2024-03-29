#include "RadialDistributionFunctionInterceptor.h"

#include <chrono>

#include "simulation/SimulationParams.h"
#include "utils/ArrayUtils.h"

void RadialDistributionFunctionInterceptor::onSimulationStart(Simulation& simulation) {
    bool append = simulation.params.start_iteration != 0;

    csv_writer = std::make_unique<CSVWriter>(simulation.params.output_dir_path / "statistics" / "radial_distribution_function.csv", append);

    csv_writer->initialize({"iteration", "bin_index (w= " + std::to_string(bin_width) + ")", "samples", "local_density"});

    auto expected_iterations = static_cast<size_t>(std::ceil(simulation.params.end_time / simulation.params.delta_t));
    SimulationInterceptor::every_nth_iteration = std::max(1, static_cast<int>(sample_every_x_percent * expected_iterations / 100));

    if (simulation.params.start_iteration == 0) {
        saveCurrentRadialDistribution(0, simulation);
    }
}

void RadialDistributionFunctionInterceptor::operator()(size_t iteration, Simulation& simulation) {
    saveCurrentRadialDistribution(iteration, simulation);
}

void RadialDistributionFunctionInterceptor::onSimulationEnd(size_t iteration, Simulation& simulation) {
    saveCurrentRadialDistribution(iteration, simulation);
}

void RadialDistributionFunctionInterceptor::logSummary(int depth) const {
    std::string indent = std::string(depth * 2, ' ');

    Logger::logger->info("{}╟┤{}RadialDistributionFunction: {}", indent, ansi_orange_bold, ansi_end);
    Logger::logger->info("{}║   ┌Bin length: {}", indent, bin_width);
    Logger::logger->info("{}║   └Sample every x percent: {}", indent, sample_every_x_percent);
}

RadialDistributionFunctionInterceptor::operator std::string() const {
    return fmt::format("RadialDistributionFunction: bin_width={}, sample_every_x_percent={}", bin_width, sample_every_x_percent);
}

double RadialDistributionFunctionInterceptor::calculateLocalDensity(size_t N, size_t bin_index) const {
    double bin_start = bin_index * bin_width;
    double bin_end = (bin_index + 1) * bin_width;

    double bin_volume = 4.0 / 3.0 * M_PI * (std::pow(bin_end, 3) - std::pow(bin_start, 3));

    return N / bin_volume;
}

void RadialDistributionFunctionInterceptor::saveCurrentRadialDistribution(size_t iteration, Simulation& simulation) {
    std::map<size_t, size_t> samples_per_bin_index;

    for (auto it1 = simulation.particle_container->begin(); it1 != simulation.particle_container->end(); it1++) {
        auto& particle = *it1;
        for (auto it2 = it1 + 1; it2 != simulation.particle_container->end(); it2++) {
            auto& other_particle = *it2;

            if (particle == other_particle) continue;

            double distance = ArrayUtils::L2Norm(particle.getX() - other_particle.getX());

            size_t bin_index = std::floor(distance / bin_width);
            samples_per_bin_index[bin_index]++;
        }
    }

    if (samples_per_bin_index.rbegin() != samples_per_bin_index.rend()) {
        for (size_t i = 0; i <= samples_per_bin_index.rbegin()->first; i++) {
            if (samples_per_bin_index.find(i) == samples_per_bin_index.end()) {
                samples_per_bin_index[i] = 0;
            }
        }
    }

    for (auto& [bin_index, samples] : samples_per_bin_index) {
        csv_writer->writeRow({iteration, bin_index, samples, calculateLocalDensity(samples, bin_index)});
    }
}