# RISC-V Pipeline Hazard Detection Unit Simulator

This repository contains an object-oriented C++ implementation of a RISC-V processor simulator. The simulator supports two variants: one with forwarding and one without forwarding. It is designed to mimic a 5-stage pipeline with the following stages:

- **Instruction Fetch (IF)**
- **Instruction Decode (ID)**
- **Execute (EX)**
- **Memory Operations (MEM)**
- **Write Back (WB)**

The simulator is based on the RIPES processor examples and includes hazard detection and forwarding logic to handle data dependencies and optimize pipeline performance.

## Overview

The processor simulator handles common pipeline hazards, such as:
- **Load-Use Hazards:** For example, when a load instruction is followed immediately by an instruction that uses the loaded value.
- **Branch Hazards:** When branch instructions depend on data that is being loaded or computed.
- **Store Hazards:** When a store instruction uses a register value that is still being loaded as the base address.
- **Jump Hazards:** When a JALR instruction depends on a register value that is being loaded or computed.

By analyzing instructions across different stages, the hazard detection unit identifies data hazards and issues stalls when necessary. In the case of the forwarding processor, multiple forwarding paths ensure that the most recent data is used for computation and branch resolution.

## Key Features

### Hazard Detection Unit

- **Load-Use Hazards:** Inserts a stall when a load is immediately followed by an instruction that requires the loaded value.
- **Branch Hazards:** Detects cases where branch instructions (e.g., `beq`) need a value that is still being loaded or computed. Examples include:
  - `lw x2, 0(x3)` followed by `beq x2, x4, 4` (requires 2 stalls)
  - `addi x2, x3, 1` followed by `beq x2, x3, 4` (requires 1 stall)
- **Store Hazards:** Manages hazards where a store instruction depends on a loaded register used as an address. For instance, `lw x2, 0(x3)` followed by `sw x4, 0(x2)` requires a stall, while `lw x2, 0(x3)` followed by `sw x2, 0(x5)` does not.
- **Jump Hazards:** Implements hazard detection for JALR instructions similar to branch hazard handling.

### Forwarding Paths

The forwarding logic minimizes pipeline stalls by transferring data directly between pipeline stages:

- **EX/MEM to ID/EX:** Forwards ALU output from the EX/MEM stage to the inputs in the ID/EX stage.
- **MEM/WB to ID/EX:** Forwards either the ALU result or loaded data from MEM/WB to ID/EX.
- **MEM/WB to EX/MEM:** Specifically used to forward loaded data to the store data path in EX/MEM.
- **EX/MEM to IF/ID and MEM/WB to IF/ID:** For early branch resolution and store address calculation, ensuring that the latest computed value is used in branch/jump comparisons.

Forwarding priority is given to the most recent instruction result:
- **EX/MEM** forwarding takes precedence over **MEM/WB** forwarding.
- In branch instructions within IF/ID, the most recent values are used to evaluate branch conditions.

### Supported Instruction Sequences

The simulator correctly handles the following sequences:
- **ALU to ALU Forwarding:**  
  `addi x2, x1, 5` followed by `add x3, x2, x4`
- **Load to ALU Forwarding:**  
  `lw x2, 0(x3)` followed by `addi x3, x2, x1` (with stall if necessary)
- **ALU to Branch Forwarding:**  
  `addi x2, x1, 5` followed by `beq x2, x3, LABEL`
- **Load to Branch Forwarding:**  
  `lw x2, 0(x3)` followed by branch instructions such as `beq x2, x4, 4`
- **Load to Store Forwarding:**  
  `lw x2, 0(x3)` followed by `sw x2, 0(x5)`

## Implementation Details

- **Reverse Order Execution:**  
  The simulation is executed in reverse order of the pipeline stages so that pipeline register data is not overwritten within the same cycle.

- **Hazard Comparison Timing:**  
  In the forwarding variant, hazard comparisons and data forwarding are performed at the start of the cycle. This ensures that the values in the pipeline registers have not been overwritten by subsequent cycle data.

- **Register Value Management:** 
  Registers are simulated following the approach used in the RIPES program. This ensures that instructions such as beq, bne, jal, and others correctly access and update register values for accurate execution.

- **inputfiles/**  
  Contains RISC-V instruction files (e.g., `strlen.txt`, `stringcopy.txt`, `strncpy.txt`, and additional self made test cases.) with the machine code and instruction strings (2nd and 3rd column).

## Usage

To run the simulator from the command line, navigate to the `src/` folder and use the following commands:

For the non-forwarding processor:
```sh
./noforward ../inputfiles/filename.txt cyclecount
```

For the forwarding processor:
```sh
./forward ../inputfiles/filename.txt cyclecount
```

Replace `filename.txt` with the desired input file and `cyclecount` with the number of cycles you want to simulate.

## Extensions and Future Work

The current implementation can be further extended by:
- Adding support for pseudo instructions.
- Optimizing the hazard detection logic to reduce unnecessary stalls.
- Enhancing the decode function to support a wider range of machine codes.
- Integrating additional ALU operations and control signals.

## Acknowledgements

- The design and implementation have been informed by the RIPES simulator for RISC-V.
- Additional guidance and support were taken from online resources, including example RISC-V assembly programs available [here](https://marz.utk.edu/my-courses/cosc230/book/example-risc-v-assembly-programs/).
