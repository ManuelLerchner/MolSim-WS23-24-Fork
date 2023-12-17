#pragma once
#include <chrono>

#include "particles/Particle.h"
#include "simulation/interceptors/SimulationInterceptor.h"

class ProgressBarInterceptor : public SimulationInterceptor {
   public:
    /**
     * @brief Construct a new Progress Bar Interceptor object
     */
    explicit ProgressBarInterceptor(Simulation& simulation);

    /**
     * @brief This function initalized the start time of the simulation
     * and the previous time point
     */
    void onSimulationStart() override;

    /**
     * @brief This function is called on every nth iteration. It prints a progress
     * bar to the console and updates the previous time point.
     *
     * @param iteration The current iteration
     */
    void operator()(size_t iteration) override;

    /**
     * @brief This function is empty as the progress bar doesnt need to do anything
     * at the end of the simulation
     *
     * @param iteration The current iteration
     */
    void onSimulationEnd(size_t iteration) override;

    /**
     * @brief The string representation of this interceptor
     *
     * @return std::string
     *
     * This is used to write the final summary of the Interceptors to the
     * console.
     */
    explicit operator std::string() const override;

   private:
    size_t expected_iterations;
    std::chrono::high_resolution_clock::time_point t_start;
    std::chrono::high_resolution_clock::time_point t_end;
    std::chrono::high_resolution_clock::time_point t_prev;
};