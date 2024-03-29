# Molecular Dynamics Simulation

[![CodeQL](https://github.com/ManuelLerchner/MolSim-WS23-24/actions/workflows/codeql.yml/badge.svg)](https://github.com/ManuelLerchner/MolSim-WS23-24/actions/workflows/codeql.yml)
[![Tests](https://github.com/ManuelLerchner/MolSim-WS23-24/actions/workflows/tests.yml/badge.svg)](https://github.com/ManuelLerchner/MolSim-WS23-24/actions/workflows/tests.yml)
[![Pages](https://github.com/ManuelLerchner/MolSim-WS23-24/actions/workflows/deploy-pages.yml/badge.svg)](https://github.com/ManuelLerchner/MolSim-WS23-24/actions/workflows/deploy-pages.yml)

This repo contains the code for the practical course **PSE: Molecular Dynamics** by group C in WS 2023/24.

## Group Members and Supervisors

**Group Members:**

- Manuel Lerchner
- Tobias Eppacher
- Daniel Safyan

**All Contributors:**

<!-- markdownlint-disable MD033 -->
<a href="https://github.com/ManuelLerchner/MolSim-WS23-24/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=ManuelLerchner/MolSim-WS23-24" alt="Contributors"/>
</a>

## Simulation Process

The simulation is designed in a modular way. The following diagram shows the main components of the simulation and how they interact with each other.

![Simulation Overview](./docs/images/simulation_overview.svg)

- **Recursive Subsimulations:** The simulations can be run recursively when loading initial particle configurations from files. This allows to simulate complex systems with multiple particle types and different initial conditions inside a single simulation.
- **Interceptor Pattern:** The simulation is designed to be easily extensible. This is achieved by using the interceptor pattern. This allows to add new features to the simulation without having to change the core simulation code. A Intercepor is a class that implements a certain interface and is registered in the simulation. The simulation then calls the interceptor at certain points during the simulation.
- **Flexible Input File Format:** The simulation can be specified via a XML file. This allows to specify all the simulation parameters at once and makes it easy to run the same simulation multiple times with different parameters. The XML file format can be looked up in the [Input File Formats](@ref InputFileFormats) section.

## Submissions and Presentations

All the submissions for the individual sheets, the presentation slides and the project documentation is automatically deployed to [GitHub Pages](https://manuellerchner.github.io/MolSim-WS23-24) and can be accessed via the following links:

- The doxygen documentation of the `master` branch can be found in [docs](https://manuellerchner.github.io/MolSim-WS23-24/docs/).
- The submission files and the videos of the `presentations` branch can be found in [submissions](https://manuellerchner.github.io/MolSim-WS23-24/submissions/).

<div align="center">
  <img src="https://github.com/ManuelLerchner/MolSim-WS23-24/assets/54124311/4f3c0021-379d-4041-9fa9-154dc8e7bb6f"/>
</div>

## Tools

### Build tools and versions

- Tested with `gcc 13.1.0`
- Tested with `CMake 3.28.0`
- Tested with `make 4.3`

### Dependencies

- Doxygen 1.10.0: `sudo apt install doxygen` (optional, only needed for documentation)
  - Graphviz: `sudo apt install graphviz` (optional, only needed for drawing UML diagrams in doxygen)
- Libxerces 3.2.3: `sudo apt install libxerces-c-dev`
- Boost Program Options: `sudo apt-get install libboost-program-options-dev`
- cmake-format: `sudo apt install cmake-format-13` (optional, only needed for formatting cmake files)

## Build

### Build the project

In this section we describe how to build the project. You can use the following options to configure the build process:

1. Create and enter into the build directory: `mkdir -p build && cd build`
2. Configure the project with cmake:
   - Standard: `cmake ..`
   - With Doxygen support: `cmake .. -D BUILD_DOC_DOXYGEN=ON`
   - Use second parallelization method: `cmake .. -D PARALLEL_V2_OPT=ON` (for more information about parallelization, see [Parallelization](@ref Parallelization))
3. Build the project
   - Compile project and tests: `make -j`
   - Compile just the project: `make -j MolSim`
   - Compile the tests: `make -j tests`
   - Compile benchmarks: `make -j benchmarks`

>*Hint: The `-j<int>` option enables parallel compilation on the given amount of cores, e.g. `-j4` for 4 cores, if no number is given the maximum amount of cores is used*

### Build the documentation

- Make sure the project is built **with** doxygen enabled.

- Enter the `build` directory after building the project.

- Run `make doc_doxygen` to build the documentation.

- The output can be found in `build/docs/html/index.html`.

- The documentation of the `master` branch can be found [here](https://manuellerchner.github.io/MolSim-WS23-24/docs/).

## Run

### Run the program

- Enter the `build/project` directory after building the project.

- Run `./MolSim -f <FILENAME>` to run the program. `<FILENAME>` is the path to the input file. For more information on the possible input file formats see [Input File Formats](@ref InputFileFormats).

  - Excecute `./MolSim --help` to get a detailed list of all options, parameters and their default values.

  - An example run could look like this: `./MolSim -f ../../body_collision.xml`
  
  - Further information about the possible input file formats can be found in the `/docs` directory.

  - If OpenMP is available, the program will by default use the maximum amount of threads available on the system. This can be changed by setting the `OMP_NUM_THREADS` environment variable in front off the program call. For example: `OMP_NUM_THREADS=4 ./MolSim -f ../../body_collision.xml`

### Run the tests

- Enter the `build/tests` directory after building the tests.

- Run `ctest` or `./tests` to run the tests.

### Run the benchmarks

- Enter the `build/benchmarks` directory after building the benchmarks.

- Execute one of the benchmarks. For example: `./2DParticleRect`
