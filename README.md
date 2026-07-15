# SteeringTN-NTN

Repository containing the custom modules developed for the ns-3 simulation of integrated **TN (Terrestrial Network)** and **NTN (Non-Terrestrial Network)** systems.

## Installation

Clone the repository:
git clone git@github.com:EnZy46/SteeringTN-NTN.git


## ns-3 Project Configuration

Download a compatible version of ns-3.

Copy the contents of the `scratch` folder from the repository into the `scratch` folder of the ns-3 project.

Copy the contents of the `src` folder from the repository into the `src` folder of the ns-3 project.

From the main ns-3 directory, run:
./ns3 configure

Build the project:
./ns3 build


## Execution

To run a simulation:
./ns3 run scratch/main



## Output

Simulation results are saved in the output files generated during execution and include the statistics required for network performance analysis.

## Requirements

- ns-3
- C++
- CMake
- Linux
