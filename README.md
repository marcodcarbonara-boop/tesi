# SteeringTN-NTN

Repository containing the custom modules developed for the ns-3 simulation of integrated **TN (Terrestrial Network)** and **NTN (Non-Terrestrial Network)** systems.

## Installation

Clone the repository:

```bash
git clone git@github.com:EnZy46/SteeringTN-NTN.git
```

## ns-3 Project Configuration

Download a compatible version of ns-3.

Copy the contents of the `scratch` folder from this repository into the `scratch` folder of the ns-3 project.

Copy the contents of the `src` folder from this repository into the `src` folder of the ns-3 project.

From the main ns-3 directory, run:

```bash
./ns3 configure
```

Build the project:

```bash
./ns3 build
```
## Attention

Before compiling the project, update the absolute paths present in the custom modules inside the `src` folder according to the local ns-3 installation path.

## Execution

To run a simulation:

```bash
./ns3 run scratch/main
```

## Output

Simulation results are saved in the output files generated during execution and include the statistics required for network performance analysis.

## Requirements

- ns-3
- C++
- CMake
- Linux
