#include "LinkedCellsContainer.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "cells/Cell.h"
#include "io/logger/Logger.h"
#include "particles/containers/linkedcells/boundaries/OutflowBoundaryType.h"
#include "particles/containers/linkedcells/boundaries/PeriodicBoundaryType.h"
#include "particles/containers/linkedcells/boundaries/ReflectiveBoundaryType.h"
#include "physics/pairwiseforces/LennardJonesForce.h"
#include "utils/ArrayUtils.h"

/*
    Methods of the LinkedCellsContainer
*/
LinkedCellsContainer::LinkedCellsContainer(const std::array<double, 3>& _domain_size, double _cutoff_radius,
                                           const std::array<BoundaryCondition, 6>& _boundary_types, int _n)
    : domain_size(_domain_size), cutoff_radius(_cutoff_radius), boundary_types(_boundary_types) {
    domain_num_cells = {std::max(static_cast<int>(std::floor(_domain_size[0] / cutoff_radius)), 1),
                        std::max(static_cast<int>(std::floor(_domain_size[1] / cutoff_radius)), 1),
                        std::max(static_cast<int>(std::floor(_domain_size[2] / cutoff_radius)), 1)};

    cell_size = {_domain_size[0] / domain_num_cells[0], _domain_size[1] / domain_num_cells[1], _domain_size[2] / domain_num_cells[2]};

    // reserve the memory for the cells
    cells.reserve((domain_num_cells[0] + 2) * (domain_num_cells[1] + 2) * (domain_num_cells[2] + 2));

    // create the cells with the correct cell-type and add them to the cells vector and the corresponding cell reference vector
    initCells();

    // add the neighbour references to the cells
    initCellNeighbourReferences();

    // create the iteration orders
    initIterationOrders();

    // reserve the memory for the particles to prevent reallocation during insertion
    particles.reserve(_n);

    Logger::logger->debug("Created LinkedCellsContainer with boundaries [{}, {}, {}] and cutoff radius {}", domain_size[0], domain_size[1],
                          domain_size[2], cutoff_radius);
    Logger::logger->debug("Created LinkedCellsContainer with {} domain cells (of which {} are at the boundary) and {} halo cells",
                          domain_cell_references.size(), boundary_cell_references.size(), halo_cell_references.size());
    Logger::logger->debug("Cells per dimension: [{}, {}, {}]", domain_num_cells[0], domain_num_cells[1], domain_num_cells[2]);
    Logger::logger->debug("Calculated cell size: [{}, {}, {}]", cell_size[0], cell_size[1], cell_size[2]);
}

void LinkedCellsContainer::addParticle(const Particle& p) {
    Cell* cell = particlePosToCell(p.getX());

    if (cell == nullptr) {
        Logger::logger->error("Particle to insert is out of bounds, position: [{}, {}, {}]", p.getX()[0], p.getX()[1], p.getX()[2]);
        throw std::runtime_error("Attempted to insert particle out of bounds");
    }
    // if (cell->getCellType() == Cell::CellType::HALO) {
    //     Logger::logger->warn("Particle to insert is in halo cell. Position: [{}, {}, {}]", p.getX()[0], p.getX()[1], p.getX()[2]);
    //     throw std::runtime_error("Attempted to insert particle into halo cell");
    // }

    size_t old_capacity = particles.capacity();
    particles.push_back(p);

    if (old_capacity != particles.capacity()) {
        updateCellsParticleReferences();
    } else {
        cell->addParticleReference(&particles.back());
    }
}

void LinkedCellsContainer::addParticle(Particle&& p) {
    Cell* cell = particlePosToCell(p.getX());

    if (cell == nullptr) {
        Logger::logger->error("Particle to insert is outside of cells. Position: [{}, {}, {}]", p.getX()[0], p.getX()[1], p.getX()[2]);
        throw std::runtime_error("Attempted to insert particle out of bounds");
    }
    // if (cell->getCellType() == Cell::CellType::HALO) {
    //     Logger::logger->warn("Particle to insert is in halo cell. Position: [{}, {}, {}]", p.getX()[0], p.getX()[1], p.getX()[2]);
    //     throw std::runtime_error("Attempted to insert particle into halo cell");
    // }

    size_t old_capacity = particles.capacity();
    particles.push_back(std::move(p));

    if (old_capacity != particles.capacity()) {
        updateCellsParticleReferences();
    } else {
        cell->addParticleReference(&particles.back());
    }
}

void LinkedCellsContainer::prepareForceCalculation() {
    // update the particle references in the cells in case the particles have moved
    updateCellsParticleReferences();

    // delete particles from halo cells
    OutflowBoundaryType::pre(*this);

    // teleport particles to other side of container
    PeriodicBoundaryType::pre(*this);

    // update the particle references in the cells in case the particles have moved
    updateCellsParticleReferences();
}

void LinkedCellsContainer::applySimpleForces(const std::vector<std::shared_ptr<SimpleForceSource>>& simple_force_sources) {
    for (Particle& p : particles) {
        if (p.isLocked()) continue;
        for (const auto& force_source : simple_force_sources) {
            p.setF(p.getF() + force_source->calculateForce(p));
        }
    }
}

void LinkedCellsContainer::applyPairwiseForces(const std::vector<std::shared_ptr<PairwiseForceSource>>& force_sources) {
    // apply the boundary conditions
    ReflectiveBoundaryType::applyBoundaryConditions(*this);
    size_t original_particle_length = PeriodicBoundaryType::applyBoundaryConditions(*this);

    for (auto& it_order : iteration_orders) {
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
        for (Cell* cell : it_order) {
            if (cell->getParticleReferences().empty()) continue;

            // Calculate cell internal forces
            for (auto it1 = cell->getParticleReferences().begin(); it1 != cell->getParticleReferences().end(); ++it1) {
                Particle* p = *it1;
                // calculate the forces between the particle and the particles in the same cell
                // uses direct sum with newtons third law
                for (auto it2 = (it1 + 1); it2 != cell->getParticleReferences().end(); ++it2) {
                    if (p->isLocked() && (*it2)->isLocked()) continue;
                    Particle* q = *it2;
                    std::array<double, 3> total_force{0, 0, 0};
                    for (auto& force : force_sources) {
                        total_force = total_force + force->calculateForce(*p, *q);
                    }
                    p->addF(total_force);
                    q->subF(total_force);
                }
            }

            // Calculate cell boundary forces
            for (Cell* neighbour_cell : cell->getNeighbourReferences()) {
                if (cell < neighbour_cell) continue;
                if (neighbour_cell->getParticleReferences().empty()) continue;

                for (Particle* p : cell->getParticleReferences()) {
                    for (Particle* neighbour_particle : neighbour_cell->getParticleReferences()) {
                        if (p->isLocked() && neighbour_particle->isLocked()) continue;
                        if (ArrayUtils::L2NormSquared(p->getX() - neighbour_particle->getX()) > cutoff_radius * cutoff_radius) continue;

                        auto total_force = std::array<double, 3>{0, 0, 0};
                        for (const auto& force_source : force_sources) {
                            total_force = total_force + force_source->calculateForce(*p, *neighbour_particle);
                        }
                        p->addF(total_force);
                        neighbour_particle->subF(total_force);
                    }
                }
            }
        }
    }

    // remove the periodic halo particles
    particles.erase(particles.begin() + original_particle_length, particles.end());
    updateCellsParticleReferences();
}

void LinkedCellsContainer::applyTargettedForces(const std::vector<std::shared_ptr<TargettedForceSource>>& force_sources,
                                                double curr_simulation_time) {
    for (const auto& force_source : force_sources) {
        force_source->applyForce(particles, curr_simulation_time);
    }
}

void LinkedCellsContainer::reserve(size_t n) {
    Logger::logger->debug("Reserving space for {} particles", n);

    size_t old_capacity = particles.capacity();
    particles.reserve(n);

    if (old_capacity != particles.capacity()) {
        updateCellsParticleReferences();
    }
}

size_t LinkedCellsContainer::size() const { return particles.size(); }

Particle& LinkedCellsContainer::operator[](int i) { return particles[i]; }

std::vector<Particle>::iterator LinkedCellsContainer::begin() { return particles.begin(); }

std::vector<Particle>::iterator LinkedCellsContainer::end() { return particles.end(); }

std::vector<Particle>::const_iterator LinkedCellsContainer::begin() const { return particles.begin(); }

std::vector<Particle>::const_iterator LinkedCellsContainer::end() const { return particles.end(); }

const std::vector<Particle>& LinkedCellsContainer::getParticles() const { return particles; }

const std::array<double, 3>& LinkedCellsContainer::getDomainSize() const { return domain_size; }

double LinkedCellsContainer::getCutoffRadius() const { return cutoff_radius; }

const std::vector<Cell>& LinkedCellsContainer::getCells() { return cells; }

const std::vector<Cell*>& LinkedCellsContainer::getBoundaryCells() const { return boundary_cell_references; }

const std::array<double, 3>& LinkedCellsContainer::getCellSize() const { return cell_size; }

const std::array<int, 3>& LinkedCellsContainer::getDomainNumCells() const { return domain_num_cells; }

int LinkedCellsContainer::cellCoordToCellIndex(int cx, int cy, int cz) const {
    if (cx < -1 || cx > domain_num_cells[0] || cy < -1 || cy > domain_num_cells[1] || cz < -1 || cz > domain_num_cells[2]) {
        return -1;
    }
    return (cx + 1) * (domain_num_cells[1] + 2) * (domain_num_cells[2] + 2) + (cy + 1) * (domain_num_cells[2] + 2) + (cz + 1);
}

Cell* LinkedCellsContainer::particlePosToCell(const std::array<double, 3>& pos) { return particlePosToCell(pos[0], pos[1], pos[2]); }

Cell* LinkedCellsContainer::particlePosToCell(double x, double y, double z) {
    int cx = static_cast<int>(std::floor(x / cell_size[0]));
    int cy = static_cast<int>(std::floor(y / cell_size[1]));
    int cz = static_cast<int>(std::floor(z / cell_size[2]));

    int cell_index = cellCoordToCellIndex(cx, cy, cz);
    if (cell_index == -1) {
        Logger::logger->error("Particle is outside of cells. Position: [{}, {}, {}]", x, y, z);
        throw std::runtime_error("A particle is outside of the cells");
    }

    return &cells[cell_index];
}

std::string LinkedCellsContainer::boundaryConditionToString(const BoundaryCondition& bc) {
    switch (bc) {
        case BoundaryCondition::OUTFLOW:
            return "Outflow";
        case BoundaryCondition::REFLECTIVE:
            return "Reflective";
        case BoundaryCondition::PERIODIC:
            return "Periodic";
        default:
            return "Unknown";
    }
};

/*
    Private methods of the LinkedCellsContainer
*/

void LinkedCellsContainer::initCells() {
    for (int cx = -1; cx < domain_num_cells[0] + 1; ++cx) {
        for (int cy = -1; cy < domain_num_cells[1] + 1; ++cy) {
            for (int cz = -1; cz < domain_num_cells[2] + 1; ++cz) {
                if (cx < 0 || cx >= domain_num_cells[0] || cy < 0 || cy >= domain_num_cells[1] || cz < 0 || cz >= domain_num_cells[2]) {
                    cells.emplace_back(Cell::CellType::HALO);
                    halo_cell_references.push_back(&cells.back());

                    if (cx == -1) {
                        left_halo_cell_references.push_back(&cells.back());
                    }
                    if (cx == domain_num_cells[0]) {
                        right_halo_cell_references.push_back(&cells.back());
                    }
                    if (cy == -1) {
                        bottom_halo_cell_references.push_back(&cells.back());
                    }
                    if (cy == domain_num_cells[1]) {
                        top_halo_cell_references.push_back(&cells.back());
                    }
                    if (cz == -1) {
                        back_halo_cell_references.push_back(&cells.back());
                    }
                    if (cz == domain_num_cells[2]) {
                        front_halo_cell_references.push_back(&cells.back());
                    }
                } else if (cx == 0 || cx == domain_num_cells[0] - 1 || cy == 0 || cy == domain_num_cells[1] - 1 || cz == 0 ||
                           cz == domain_num_cells[2] - 1) {
                    cells.emplace_back(Cell::CellType::BOUNDARY);
                    boundary_cell_references.push_back(&cells.back());
                    domain_cell_references.push_back(&cells.back());

                    if (cx == 0) {
                        left_boundary_cell_references.push_back(&cells.back());
                    }
                    if (cx == domain_num_cells[0] - 1) {
                        right_boundary_cell_references.push_back(&cells.back());
                    }
                    if (cy == 0) {
                        bottom_boundary_cell_references.push_back(&cells.back());
                    }
                    if (cy == domain_num_cells[1] - 1) {
                        top_boundary_cell_references.push_back(&cells.back());
                    }
                    if (cz == 0) {
                        back_boundary_cell_references.push_back(&cells.back());
                    }
                    if (cz == domain_num_cells[2] - 1) {
                        front_boundary_cell_references.push_back(&cells.back());
                    }
                } else {
                    cells.emplace_back(Cell::CellType::INNER);
                    domain_cell_references.push_back(&cells.back());
                }
            }
        }
    }
}

void LinkedCellsContainer::initCellNeighbourReferences() {
    // Loop through each cell according to their cell coordinates
    for (int cx = -1; cx < domain_num_cells[0] + 1; ++cx) {
        for (int cy = -1; cy < domain_num_cells[1] + 1; ++cy) {
            for (int cz = -1; cz < domain_num_cells[2] + 1; ++cz) {
                Cell& cell = cells.at(cellCoordToCellIndex(cx, cy, cz));

                // Loop through each of the current cells neighbour cells according to their cell coordinates
                // except the current cell itself
                for (int dx = -1; dx < 2; ++dx) {
                    for (int dy = -1; dy < 2; ++dy) {
                        for (int dz = -1; dz < 2; ++dz) {
                            if (dx == 0 && dy == 0 && dz == 0) continue;

                            // Get the cell index of the current neighbour cell
                            int cell_index = cellCoordToCellIndex(cx + dx, cy + dy, cz + dz);

                            // If the neighbour cell would be out of bounds, skip it
                            if (cell_index == -1) continue;

                            // Add the neighbour to the current cells neighbour references
                            Cell& curr_neighbour = cells.at(cell_index);
                            cell.addNeighbourReference(&curr_neighbour);
                        }
                    }
                }
            }
        }
    }
}

void LinkedCellsContainer::initIterationOrders() {
#if PARALLEL_V2
    Logger::logger->warn("Creating iteration orders for parallel v2");
    std::vector<Cell*> iteration_order;

    for (size_t num = 0; num < cells.size(); ++num) {
        iteration_order.push_back(&cells[num]);
    }

    // shuffle the iteration order
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(iteration_order.begin(), iteration_order.end(), g);

    iteration_orders.push_back(iteration_order);
#else
    Logger::logger->warn("Creating iteration orders for parallel v1 (c_18 traversal)");

    std::vector<std::array<int, 3>> start_offsets;

    int d_x = 2;
    int d_y = 3;
    int d_z = 3;

    for (int x = 0; x < d_x; ++x) {
        for (int y = 0; y < d_y; ++y) {
            for (int z = 0; z < d_z; ++z) {
                start_offsets.push_back({x - 1, y - 1, z - 1});
            }
        }
    }

    for (size_t i = 0; i < start_offsets.size(); ++i) {
        std::vector<Cell*> iteration_order;
        const std::array<int, 3>& start_offset = start_offsets[i];

        for (int cx = start_offset[0]; cx <= domain_num_cells[0]; cx += d_x) {
            for (int cy = start_offset[1]; cy <= domain_num_cells[1]; cy += d_y) {
                for (int cz = start_offset[2]; cz <= domain_num_cells[2]; cz += d_z) {
                    iteration_order.push_back(&cells.at(cellCoordToCellIndex(cx, cy, cz)));
                }
            }
        }

        iteration_orders.push_back(iteration_order);
    }
#endif
}

void LinkedCellsContainer::updateCellsParticleReferences() {
    // clear the particle references in the cells
    for (Cell& cell : cells) {
        cell.clearParticleReferences();
    }

    // add the particle references to the cells
    for (Particle& p : particles) {
        Cell* cell = particlePosToCell(p.getX());

        cell->addParticleReference(&p);
    }
}

void LinkedCellsContainer::deleteHaloParticles() {
    for (Cell* cell : halo_cell_references) {
        for (Particle* p : cell->getParticleReferences()) {
            particles.erase(particles.begin() + (p - &particles[0]));
        }
    }
}
