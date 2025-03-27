#include <iostream>
#include <fstream>  //Provides file handling classes: ifstream (input), ofstream (output), and fstream (both).
#include <sstream>  //error fixed //Provides stringstream, istringstream, and ostringstream for string manipulation.
#include <vector>
#include <map>
#include <cstdint>  //Defines fixed-width integer types like int8_t, uint16_t, int32_t, uint64_t.
#include <iomanip>  //Provides manipulators like std::setw, std::setprecision, std::fixed, std::hex, etc.
#include "Processor.hpp"
using namespace std;


void InstructionFetch::process(const int i) {
    if (processor->hazard_in_id) {
        return;
    }

    if (processor->getPC() < 4 * processor->getnoofinstructions()) {
        // Update pipeline matrix display
        if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "  MEM ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = "IF/MEM;";
        } else if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "  WB  ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = " IF/WB;";
        } else if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "  EX  ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = " IF/EX;";
        } else {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = "  IF  ;";
        }
    }
    // Fetch instruction at current PC
    uint32_t instruction = processor->getInstruction(processor->getPC());
    processor->getIF_ID().instruction = instruction;
    processor->getIF_ID().pc = processor->getPC();
    processor->getIF_ID().isStall = false;
    processor->setPC(processor->getPC() + 4);

    // If a branch has been taken, flush the IF/ID register
    if (processor->isBranchTaken()) {
        processor->getIF_ID().isStall = true;
        processor->setPC(processor->getBranchTarget());
        processor->setBranch(false, 0);
    }
}

void InstructionDecode::process(const int i) {
    // Get instruction from IF/ID register
    uint32_t instruction = processor->getIF_ID().instruction;
    uint32_t pc = processor->getIF_ID().pc;
    uint32_t opcode = instruction & 0x7F;
    int32_t rs1 = (instruction >> 15) & 0x1F;
    int32_t rs2 = (instruction >> 20) & 0x1F;

    // For I-type instructions, rs2 is not used
    if (opcode == 0b0010011 || opcode == 0b0000011 || opcode == 0b1100111) {
        rs2 = 0;
    }

    int32_t rd = (instruction >> 7) & 0x1F;
    if (processor->getIF_ID().instruction) {
        if (processor->pipelineMatrix[(pc / 4)][i] == "  WB  ;") {
            processor->pipelineMatrix[(pc / 4)][i] = "ID/WB ;";
        } else if (processor->pipelineMatrix[(pc / 4)][i] == "  MEM ;") {
            processor->pipelineMatrix[(pc / 4)][i] = "ID/MEM;";
        } else {
            processor->pipelineMatrix[(pc / 4)][i] = "  ID  ;";
        }
    }

    // Check for hazards
    if (processor->hazard_in_id) {
        // Insert a bubble (NOP) into ID/EX
        processor->getID_EX().wb.regWrite = false;
        processor->getID_EX().wb.memToReg = false;
        processor->getID_EX().mem.branch = false;
        processor->getID_EX().mem.memRead = false;
        processor->getID_EX().mem.memWrite = false;
        processor->getID_EX().mem.jump = false;
        processor->getID_EX().ex.aluOp = 0;
        processor->getID_EX().ex.aluSrc = false;
        processor->getID_EX().rd = 0;
        processor->getID_EX().rs1 = 0;
        processor->getID_EX().rs2 = 0;
        processor->getID_EX().immediate = 0;
        processor->getID_EX().isStall = true;
        // processor->setPC(processor->getPC() - 4);
        return;
    }

    // If the IF/ID stage is stalled, just propagate the stall
    if (processor->getIF_ID().isStall) {
        if (processor->pipelineMatrix.find(pc / 4) != processor->pipelineMatrix.end())
            processor->pipelineMatrix[(pc / 4)][i] = "      ;";
        processor->getID_EX().isStall = true;
        return;
    }
    // Get instruction type and extract immediate
    InstructionType type = getInstructionType(instruction);
    int32_t immediate = extractImmediate(instruction, type);

    // Generate control signals
    ControlSignals signals = generateControlSignals(instruction);

    // Read register values
    int32_t readData1 = processor->getRegister(rs1);
    int32_t readData2 = processor->getRegister(rs2);

    if (processor->getIF_ID().rs1forwarded) {
        readData1 = processor->getIF_ID().rs1data;
    }
    if (processor->getIF_ID().rs2forwarded) {
        readData2 = processor->getIF_ID().rs2data;
    }
    // Extract function code for R-type instructions
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t funct7 = (instruction >> 25) & 0x7F;

    uint32_t branchTarget = static_cast<uint32_t>(static_cast<int32_t>(pc) + immediate);
    if (opcode == 0b1100011) { // Branch instruction opcode
        // Compute branch target using signed arithmetic
        if (funct3 == 0x0) { // BEQ
            if (readData1 == readData2) {
                processor->setBranch(true, branchTarget);
            }
        } else if (funct3 == 0x1) { // BNE
            if (readData1 != readData2) {
                processor->setBranch(true, branchTarget);
            }
        } else if (funct3 == 0x4) { // BLT
            if ((int32_t)readData1 < (int32_t)readData2) {
                processor->setBranch(true, branchTarget);
            }
        } else if (funct3 == 0x5) { // BGE
            if ((int32_t)readData1 >= (int32_t)readData2) {
                processor->setBranch(true, branchTarget);
            }
        } else if (funct3 == 0x6) { // BLTU
            if (readData1 < readData2) {
                processor->setBranch(true, branchTarget);
            }
        } else if (funct3 == 0x7) { // BGEU
            if (readData1 >= readData2) {
                processor->setBranch(true, branchTarget);
            }
        }
    }
    // Handle JAL instruction
    else if (opcode == 0b1101111) { // JAL opcode
        // Save return address (PC + 4) to rd
        if (rd != 0) { // Don't write to x0
            processor->setRegister(rd, pc + 4);
        }
        // Set branch target to PC + immediate
        processor->setBranch(true, branchTarget);
    }
    // Handle JALR instruction
    else if (opcode == 0b1100111 && funct3 == 0x0) { // JALR opcode
        // Save return address (PC + 4) to rd
        if (rd != 0) { // Don't write to x0
            processor->setRegister(rd, pc + 4);
        }
        // Set branch target to rs1 + immediate (with lowest bit cleared)
        processor->setBranch(true, (readData1 + immediate) & ~1);
    }

    // Update ID/EX register
    processor->getID_EX().wb.regWrite = signals.regWrite;
    processor->getID_EX().wb.memToReg = signals.memToReg;
    processor->getID_EX().mem.branch = signals.branch;
    processor->getID_EX().mem.memRead = signals.memRead;
    processor->getID_EX().mem.memWrite = signals.memWrite;
    processor->getID_EX().mem.jump = signals.jump;
    processor->getID_EX().ex.aluOp = signals.aluOp;
    processor->getID_EX().ex.aluSrc = signals.aluSrc;
    processor->getID_EX().pc = pc;
    processor->getID_EX().readData1 = readData1;
    processor->getID_EX().readData2 = readData2;
    processor->getID_EX().immediate = immediate;
    processor->getID_EX().funct3 = funct3;
    processor->getID_EX().funct7 = funct7;
    processor->getID_EX().rd = rd;
    processor->getID_EX().rs1 = rs1;
    processor->getID_EX().rs2 = rs2;
    processor->getID_EX().instruction = instruction;
    processor->getID_EX().isStall = false;
    processor->getIF_ID().hazard.is_hazard = false;
}

void Execute::process(const int i) {
    // Get inputs from ID/EX register
    uint32_t readData1 = processor->getID_EX().readData1;
    uint32_t readData2 = processor->getID_EX().readData2;
    int32_t immediate = processor->getID_EX().immediate;
    uint32_t pc = processor->getID_EX().pc;
    uint32_t funct3 = processor->getID_EX().funct3;
    uint32_t funct7 = processor->getID_EX().funct7;
    uint32_t aluOp = processor->getID_EX().ex.aluOp;
    bool aluSrc = processor->getID_EX().ex.aluSrc;
    uint32_t instruction = processor->getID_EX().instruction;
    uint32_t opcode = instruction & 0x7F;
    uint32_t rs1 = processor->getID_EX().rs1;
    uint32_t rs2 = processor->getID_EX().rs2;

    // If stalled, propagate stall but still update visualization
    if (processor->getID_EX().isStall) {
        processor->getEX_MEM().isStall = true;
        return;
    }

    // Use immediate value if aluSrc is true, otherwise use the forwarded or original rs2 value
    uint32_t input2 = aluSrc ? immediate : readData2;

    int32_t aluResult = 0;
    bool zero = false;

    if (opcode == 0b0010111) { // AUIPC
        // For AUIPC: rd = PC + (immediate) where immediate is already shifted left 12.
        aluResult = pc + immediate;
    } else if (opcode == 0b0110111) { // LUI
        aluResult = immediate;
    } else {
        // Always compute ALU control via getALUControl so that I-type bitwise instructions
        // (andi, ori, xori) use their funct3 to select the proper operation.
        int32_t aluControl = aluSrc ? 2 : getALUControl(aluOp, funct3, funct7);
        aluResult = performALU(aluControl, readData1, input2, zero);
    }

    // Update EX/MEM register
    processor->getEX_MEM().wb = processor->getID_EX().wb;
    processor->getEX_MEM().mem = processor->getID_EX().mem;
    processor->getEX_MEM().zero = zero;
    processor->getEX_MEM().aluResult = aluResult;
    processor->getEX_MEM().readData2 = readData2; // Use forwarded value for memory writes
    processor->getEX_MEM().rd = processor->getID_EX().rd;
    processor->getEX_MEM().instruction = instruction;
    processor->getEX_MEM().isStall = false;
    processor->getEX_MEM().pc = pc;
    processor->getEX_MEM().rs1 = rs1;
    processor->getEX_MEM().rs2 = rs2;
    if (processor->getID_EX().instruction) {
        if (processor->pipelineMatrix[(pc / 4)][i] == "  WB  ;") {
            processor->pipelineMatrix[(pc / 4)][i] = " EX/WB;";
        } else {
            processor->pipelineMatrix[(pc / 4)][i] = "  EX  ;";
        }
    }
}

// Implementation of MemoryAccess::process
void MemoryAccess::process(const int i) {
    bool memRead = processor->getEX_MEM().mem.memRead;
    bool memWrite = processor->getEX_MEM().mem.memWrite;
    int32_t aluResult = processor->getEX_MEM().aluResult;
    uint32_t writeData = processor->getEX_MEM().readData2;
    uint32_t instruction = processor->getEX_MEM().instruction;
    uint32_t pc = processor->getEX_MEM().pc;

    // If stalled, propagate stall
    if (processor->getEX_MEM().isStall) {
        processor->getMEM_WB().isStall = true;
        return;
    }

    // Memory access operations
    int32_t readData = 0;
    if (memRead) {
        readData = processor->getMemory(aluResult);
    }

    if (memWrite) {
        processor->setMemory(aluResult, writeData);
    }

    // Update MEM/WB register
    processor->getMEM_WB().wb = processor->getEX_MEM().wb;
    processor->getMEM_WB().readData = readData;
    processor->getMEM_WB().aluResult = aluResult;
    processor->getMEM_WB().rd = processor->getEX_MEM().rd;
    processor->getMEM_WB().instruction = instruction;
    processor->getMEM_WB().isStall = false;
    processor->getMEM_WB().pc = pc;

    if (processor->getEX_MEM().instruction) {
        processor->pipelineMatrix[(pc / 4)][i] = "  MEM ;";
    }
}

// Implementation of WriteBack::process
void WriteBack::process(const int i) {
    // Get inputs from MEM/WB register
    bool regWrite = processor->getMEM_WB().wb.regWrite;
    bool memToReg = processor->getMEM_WB().wb.memToReg;
    int32_t readData = processor->getMEM_WB().readData;
    int32_t aluResult = processor->getMEM_WB().aluResult;
    uint32_t rd = processor->getMEM_WB().rd;
    uint32_t pc = processor->getMEM_WB().pc;

    // If stalled, do nothing
    if (processor->getMEM_WB().isStall) {
        return;
    }

    // Write back to register file
    if (regWrite && rd != 0) { // Don't write to x0
        int32_t writeData = memToReg ? readData : aluResult;
        processor->setRegister(rd, writeData);
    }

    if (processor->getMEM_WB().instruction) {
        processor->pipelineMatrix[(pc / 4)][i] = "  WB  ;";
    }
}

void Processor::updateForwardingSignals() {
    ForwardingSignals& forwarding = getForwarding();
    forwarding.forwardA = 0;
    forwarding.forwardB = 0;

    // Reset forwarding flags in IF/ID
    getIF_ID().rs1forwarded = false;
    getIF_ID().rs2forwarded = false;

    // Normal forwarding for ID/EX stage
    uint32_t rs1_ex = getID_EX().rs1;
    uint32_t rs2_ex = getID_EX().rs2;

    // Get information from IF/ID register for branch/JALR forwarding
    uint32_t if_id_instruction = getIF_ID().instruction;
    uint32_t if_id_opcode = if_id_instruction & 0x7F;
    uint32_t if_id_rs1 = (if_id_instruction >> 15) & 0x1F;
    uint32_t if_id_rs2 = (if_id_instruction >> 20) & 0x1F;
    bool is_branch = (if_id_opcode == 0b1100011); // BEQ, BNE, etc.
    bool is_jalr = (if_id_opcode == 0b1100111);   // JALR
    bool is_store = (if_id_opcode == 0b0100011);  // Store instruction

    // EX hazard - Forward from EX/MEM to ID/EX
    if (getEX_MEM().wb.regWrite &&
        (getEX_MEM().rd != 0) &&
        (getEX_MEM().rd == rs1_ex)) {
        forwarding.forwardA = 2;
        getID_EX().readData1 = getEX_MEM().aluResult; // Update input1 directly
    }

    if (getEX_MEM().wb.regWrite &&
        (getEX_MEM().rd != 0) &&
        (getEX_MEM().rd == rs2_ex)) {
        forwarding.forwardB = 2;
        getID_EX().readData2 = getEX_MEM().aluResult; // Update input2 directly
    }

    // MEM hazard - Forward from MEM/WB to ID/EX
    if (getMEM_WB().wb.regWrite &&
        (getMEM_WB().rd != 0) &&
        !(getEX_MEM().wb.regWrite && (getEX_MEM().rd != 0) && (getEX_MEM().rd == rs1_ex)) &&
        (getMEM_WB().rd == rs1_ex)) {
        forwarding.forwardA = 1;
        getID_EX().readData1 = getMEM_WB().wb.memToReg ?
            getMEM_WB().readData :
            getMEM_WB().aluResult; // Update input1 directly
    }

    if (getMEM_WB().wb.regWrite &&
        (getMEM_WB().rd != 0) &&
        !(getEX_MEM().wb.regWrite && (getEX_MEM().rd != 0) && (getEX_MEM().rd == rs2_ex)) &&
        (getMEM_WB().rd == rs2_ex)) {
        forwarding.forwardB = 1;
        getID_EX().readData2 = getMEM_WB().wb.memToReg ?
            getMEM_WB().readData :
            getMEM_WB().aluResult; // Update input2 directly
    }

    // Special case: Forward from MEM/WB to EX/MEM for load-store sequence
    // This handles the case where a load is immediately followed by a store that uses the loaded value as store data
    // Example: lw x2, 0(x3) followed by sw x2, 0(x3)
    if (getEX_MEM().mem.memWrite && // Current instruction in EX/MEM is a store
        getMEM_WB().wb.memToReg &&   // Previous instruction in MEM/WB is a load
        getMEM_WB().wb.regWrite &&   // Previous instruction writes to a register
        getMEM_WB().rd != 0 &&       // Previous instruction writes to a non-zero register
        getMEM_WB().rd == getEX_MEM().rs2) { // Store is using the register being loaded as store data

        // Forward the loaded value directly to the store data in EX/MEM
        getEX_MEM().readData2 = getMEM_WB().readData;
    }

    // Forward to IF/ID for branch/JALR instructions
    if (is_branch || is_jalr) {
        // Forward from MEM/WB to IF/ID
        if (getMEM_WB().wb.regWrite && getMEM_WB().rd != 0) {
            int32_t forwarded_value = getMEM_WB().wb.memToReg ?
                getMEM_WB().readData :
                getMEM_WB().aluResult;

            // Update for rs1 if needed
            if (if_id_rs1 != 0 && if_id_rs1 == getMEM_WB().rd) {
                // We need to store this value where the ID stage can use it for branch evaluation
                getIF_ID().rs1data = forwarded_value;
                getIF_ID().rs1forwarded = true;
            }

            // Update for rs2 if needed (for branch instructions)
            if (is_branch && if_id_rs2 != 0 && if_id_rs2 == getMEM_WB().rd) {
                getIF_ID().rs2data = forwarded_value;
                getIF_ID().rs2forwarded = true;
            }
        }

        // Forward from EX/MEM to IF/ID (higher priority)
        if (getEX_MEM().wb.regWrite && getEX_MEM().rd != 0) {
            // Only forward for non-load instructions (load results aren't available yet)
            if (!(getEX_MEM().mem.memRead)) {
                int32_t forwarded_value = getEX_MEM().aluResult;

                // Update for rs1 if needed
                if (if_id_rs1 != 0 && if_id_rs1 == getEX_MEM().rd) {
                    getIF_ID().rs1data = forwarded_value;
                    getIF_ID().rs1forwarded = true;
                }

                // Update for rs2 if needed (for branch instructions)
                if (is_branch && if_id_rs2 != 0 && if_id_rs2 == getEX_MEM().rd) {
                    getIF_ID().rs2data = forwarded_value;
                    getIF_ID().rs2forwarded = true;
                }
            }
        }
    }

    // Forward to IF/ID for store instructions
    if (is_store) {
        // Forward from MEM/WB to IF/ID store
        if (getMEM_WB().wb.regWrite && getMEM_WB().rd != 0) {
            int32_t forwarded_value = getMEM_WB().wb.memToReg ?
                getMEM_WB().readData :
                getMEM_WB().aluResult;

            // Update for rs2 (store data) if needed
            if (if_id_rs2 != 0 && if_id_rs2 == getMEM_WB().rd) {
                getIF_ID().rs2data = forwarded_value;
                getIF_ID().rs2forwarded = true;
            }

            // Also handle base address register if it's being forwarded
            if (if_id_rs1 != 0 && if_id_rs1 == getMEM_WB().rd) {
                getIF_ID().rs1data = forwarded_value;
                getIF_ID().rs1forwarded = true;
            }
        }

        // Forward from EX/MEM to IF/ID store (higher priority)
        if (getEX_MEM().wb.regWrite && getEX_MEM().rd != 0) {
            // Only forward for non-load instructions
            if (!(getEX_MEM().mem.memRead)) {
                int32_t forwarded_value = getEX_MEM().aluResult;

                // Update for rs2 (store data) if needed
                if (if_id_rs2 != 0 && if_id_rs2 == getEX_MEM().rd) {
                    getIF_ID().rs2data = forwarded_value;
                    getIF_ID().rs2forwarded = true;
                }

                // Also handle base address register if it's being forwarded
                if (if_id_rs1 != 0 && if_id_rs1 == getEX_MEM().rd) {
                    getIF_ID().rs1data = forwarded_value;
                    getIF_ID().rs1forwarded = true;
                }
            }
        }
    }

    // Handle the case: lw x2, 0(x3) then sw x4, 0(x2)
    // The checkForHazards() function correctly stalls for this case
    // but we need to forward from MEM/WB to ID/EX after the stall cycle

    // Check if the current instruction in ID/EX is a store and uses a register loaded in the previous cycle
    uint32_t id_ex_opcode = getID_EX().instruction & 0x7F;
    bool id_ex_is_store = (id_ex_opcode == 0b0100011);

    if (id_ex_is_store &&
        getMEM_WB().wb.memToReg && // Previous instruction was a load
        getMEM_WB().wb.regWrite &&
        getMEM_WB().rd != 0 &&
        getMEM_WB().rd == getID_EX().rs1) { // Store uses the loaded value as address base

        // Forward the loaded value to the store address base register
        getID_EX().readData1 = getMEM_WB().readData;
    }
}

bool Processor::checkForHazards() {
    // Get information from IF/ID register
    uint32_t instruction = getIF_ID().instruction;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    uint32_t opcode = instruction & 0x7F;
    
    // For I-type instructions, rs2 is not relevant
    if (opcode == 0b0010011 || opcode == 0b0000011 || opcode == 0b1100111) {
        rs2 = 0;
    }
    
    //lw addi
    // Check for load-use hazard
    if (opcode != 0b0100011 && getID_EX().mem.memRead && getID_EX().rd != 0 && !getID_EX().isStall &&
        ((getID_EX().rd == rs1 && rs1 != 0) || 
         (getID_EX().rd == rs2 && rs2 != 0))) {
        // Signal a hazard - but exclude store instructions (store data doesn't need stall)
        return true;
    }
    
    // Special check for load followed by store that uses the loaded value as address base
    if (opcode == 0b0100011 && // Store instruction
        getID_EX().mem.memRead && // Previous instruction is a load
        getID_EX().rd != 0 && 
        getID_EX().rd == rs1) {  // Store is using the loaded register as address base
        return true;
    }
    
    //addi jalr/addi beq
    // Check for ALU instruction feeding into branch/JALR
    if ((opcode == 0b1100011 || opcode == 0b1100111) && // Branch or JALR
        getID_EX().wb.regWrite && getID_EX().rd != 0 && 
        ((getID_EX().rd == rs1) || (opcode == 0b1100011 && getID_EX().rd == rs2))) {
        // Signal a hazard when an instruction in ID/EX is writing to a register
        // that this branch/JALR in IF/ID needs to read
        return true;
    }
    
    //lw then beq
    // Check for branch hazard - need to check if it's a branch instruction
    if (opcode == 0b1100011) { // Branch instruction opcode (beq, bne, blt, bge, etc.)
        // Only check for hazard if the instruction in EX/MEM is a load instruction
        uint32_t ex_mem_opcode = getEX_MEM().instruction & 0x7F;
        bool is_load = (ex_mem_opcode == 0b0000011); // Load instruction opcode
        
        // Check if any of the registers used in the branch are being written by a load instruction in EX/MEM
        if (!getEX_MEM().isStall && is_load &&
            (getEX_MEM().wb.regWrite && getEX_MEM().rd != 0) && 
            (getEX_MEM().rd == rs1 || getEX_MEM().rd == rs2)) {
            return true; // Branch hazard detected with load instruction
        }
    }
    //lw then jalr
    // Check for JALR hazard - JALR uses rs1 for target address calculation
    if (opcode == 0b1100111) { // JALR instruction opcode
        // Only check for hazard if the instruction in EX/MEM is a load instruction
        uint32_t ex_mem_opcode = getEX_MEM().instruction & 0x7F;
        bool is_load = (ex_mem_opcode == 0b0000011); // Load instruction opcode
        
        // Check if the source register used in JALR is being written by a load instruction in EX/MEM
        if (!getEX_MEM().isStall && is_load &&
            (getEX_MEM().wb.regWrite && getEX_MEM().rd != 0) && 
            (getEX_MEM().rd == rs1)) {
            return true; // JALR hazard detected with load instruction
        }
    }
    
    // JAL doesn't use source registers, so no data hazard possible for JAL
    return false;
}

// Update the cycle method
void Processor::cycle(const int i) {
    // Execute in reverse order to prevent data hazards
    hazard_in_id = false;
    if (checkForHazards()) {
        hazard_in_id = true;
    }
    updateForwardingSignals();
    wbStage->process(i);  // WB stage  
    memStage->process(i); // MEM stage
    exStage->process(i);  // EX stage
    idStage->process(i);  // ID stage
    ifStage->process(i);  // IF stage
}

// Update the run method to initialize visualization properly
void Processor::run(int cycles, const string& inputFile) {
    pipelineTrace.clear();
    currentCycle = 0;

    // Reset pipeline registers
    if_id = IF_ID_Register();
    id_ex = ID_EX_Register();
    ex_mem = EX_MEM_Register();
    mem_wb = MEM_WB_Register();

    // Reset processor state
    pc = 0;
    isBranch = false;
    branchTarget = 0;

    for (int i = 1; i < 32; i++) {
        registers[i] = 0;
    }
    // Run for specified number of cycles
    for (int i = 0; i < cycles; i++) {
        cycle(i);
    }
    print_pipeline(cycles, true, inputFile);
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_file> <cycle_count>" << endl;
        return 1;
    }
    // Parse command line arguments
    string inputFile = argv[1];
    int cycleCount = stoi(argv[2]);

    // Create processor and run simulation
    Processor processor(inputFile, cycleCount);
    processor.run(cycleCount, inputFile);
    return 0;
}