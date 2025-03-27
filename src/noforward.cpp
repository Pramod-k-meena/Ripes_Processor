#include <iostream>
#include <fstream>  //Provides file handling classes: ifstream (input), ofstream (output), and fstream (both).
#include <sstream>  //error fixed //Provides stringstream, istringstream, and ostringstream for string manipulation.
#include <vector>
#include <map>
#include <cstdint>  //Defines fixed-width integer types like int8_t, uint16_t, int32_t, uint64_t.
#include <iomanip>  //Provides manipulators like std::setw, std::setprecision, std::fixed, std::hex, etc.
#include "Processor.hpp"
using namespace std;


// Now implement the methods for all classes
void InstructionFetch::process(const int i) {
    if (processor->getPC() < 4 * processor->getnoofinstructions()) {
        //if already string MM or WB present then please add that string/IF instead of just IF
        if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "   MEM  ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = " IF/MEM ;";
        } else if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "   WB   ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = "  IF/WB ;";
        } else if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "   EX   ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = "  IF/EX ;";
        } else if (processor->pipelineMatrix[((processor->getPC()) / 4)][i] == "  EX/WB ;") {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = "IF/EX/WB;";
        } else {
            processor->pipelineMatrix[((processor->getPC()) / 4)][i] = "   IF   ;";
        }
    }
    if (processor->getIF_ID().hazard.is_hazard) {
        return;
    }
    // Fetch instruction at current PC
    uint32_t instruction = processor->getInstruction(processor->getPC());
    processor->getIF_ID().instruction = instruction;
    processor->getIF_ID().pc = processor->getPC();
    processor->getIF_ID().isStall = false;
    processor->setPC(processor->getPC() + 4);
    // if the branch hs been taken then set ifid register to nop and set pc to branch target
    if (processor->isBranchTaken()) {
        processor->getIF_ID().isStall = true;
        processor->setPC(processor->getBranchTarget());
        processor->setBranch(false, 0);
    }
}

// Implementation of InstructionDecode::process
void InstructionDecode::process(const int i) {
    // Get instruction from IF/ID register
    uint32_t instruction = processor->getIF_ID().instruction;
    uint32_t pc = processor->getIF_ID().pc;
    uint32_t opcode = instruction & 0x7F;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;;
    // For R-type instructions, extract rs2 normally; for I-type, leave rs2 as 0.
    // For I-type instructions (addi, load, jalr) rs2 is not used, so set it to 0.
    if (opcode == 0b0010011 || opcode == 0b0000011 || opcode == 0b1100111) {
        rs2 = 0;
    }
    int32_t rd = (instruction >> 7) & 0x1F;
    if (processor->getIF_ID().instruction) {
        if (processor->pipelineMatrix[(pc / 4)][i] == "   WB   ;") {
            processor->pipelineMatrix[(pc / 4)][i] = " ID/WB ;";
        } else if (processor->pipelineMatrix[(pc / 4)][i] == "   MEM  ;") {
            processor->pipelineMatrix[(pc / 4)][i] = " ID/MEM ;";
        } else {
            processor->pipelineMatrix[(pc / 4)][i] = "   ID   ;";
        }
    }
    // If no instruction (e.g., after branch), just pass stall
    // In InstructionDecode::process, replace the stall section with:
    if (processor->getIF_ID().isStall) {
        if (processor->pipelineMatrix.find(pc / 4) != processor->pipelineMatrix.end())
            processor->pipelineMatrix[(pc / 4)][i] = "        ;";
        processor->getID_EX().isStall = true;
        return;
    }
    if (processor->getID_EX().wb.regWrite && (processor->getID_EX().rd != 0) && (processor->getID_EX().isStall == false) &&
        (((rs1 != 0) && (processor->getID_EX().rd == rs1)) || ((rs2 != 0) && (processor->getID_EX().rd == rs2)))) {
        processor->getIF_ID().hazard.is_hazard = true;
        processor->getID_EX().isStall = true;
        return;
    }
    if (processor->getEX_MEM().wb.regWrite && (processor->getEX_MEM().rd != 0) && (processor->getEX_MEM().isStall == false) &&
        (((rs1 != 0) && (processor->getEX_MEM().rd == rs1)) || ((rs2 != 0) && (processor->getEX_MEM().rd == rs2)))) {
        processor->getIF_ID().hazard.is_hazard = true;
        return;
    }
    if (processor->getMEM_WB().wb.regWrite && (processor->getMEM_WB().rd != 0) && (processor->getMEM_WB().isStall == false) &&
        (((rs1 != 0) && (processor->getMEM_WB().rd == rs1)) || ((rs2 != 0) && (processor->getMEM_WB().rd == rs2)))) {
        processor->getIF_ID().hazard.is_hazard = true;
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

// Implementation of Execute::process
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

    // If stalled, propagate stall
    if (processor->getID_EX().isStall) {
        processor->getEX_MEM().isStall = true;
        return;
    }
    //added by kaju
    int32_t aluResult = 0;
    bool zero = false;
    if (opcode == 0b0010111) { // AUIPC
        // For AUIPC: rd = PC + (immediate) where immediate is already shifted left 12.
        aluResult = pc + immediate;
    } else if (opcode == 0b0110111) { // LUI
        aluResult = immediate;
    } else {
        uint32_t input1 = readData1;
        uint32_t input2 = aluSrc ? immediate : readData2;
        // Always compute ALU control via getALUControl so that I-type bitwise instructions
        // (andi, ori, xori) use their funct3 to select the proper operation.
        int32_t aluControl = aluSrc ? 2 : getALUControl(aluOp, funct3, funct7);
        aluResult = performALU(aluControl, input1, input2, zero);
    }
    //added by kaju

    // Update EX/MEM register
    processor->getEX_MEM().wb = processor->getID_EX().wb;
    processor->getEX_MEM().mem = processor->getID_EX().mem;
    // processor->getEX_MEM().branchTarget = branchTarget;
    processor->getEX_MEM().zero = zero;
    processor->getEX_MEM().aluResult = aluResult;
    processor->getEX_MEM().readData2 = readData2;
    processor->getEX_MEM().rd = processor->getID_EX().rd;
    processor->getEX_MEM().instruction = instruction;
    processor->getEX_MEM().isStall = false;
    processor->getEX_MEM().pc = pc;
    if (processor->getID_EX().instruction) {
        if (processor->pipelineMatrix[(pc / 4)][i] == "   WB   ;") {
            processor->pipelineMatrix[(pc / 4)][i] = "  EX/WB ;";
        }else {
            processor->pipelineMatrix[(pc / 4)][i] = "   EX   ;";
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
        processor->pipelineMatrix[(pc / 4)][i] = "   MEM  ;";
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
        // processor->pipelineMatrix[]=processor->getMEM_WB().instructionStr;
        // processor.pipelineMatrix[instructiono like ist instrcution 0]="WB  ;";
        return;
    }

    // Write back to register file
    if (regWrite && rd != 0) { // Don't write to x0
        int32_t writeData = memToReg ? readData : aluResult;
        // cout << rd << " data:" << writeData << " " << memToReg << " " << readData << " " << aluResult << endl;
        processor->setRegister(rd, writeData);
    }
    if (processor->getMEM_WB().instruction) {
        processor->pipelineMatrix[(pc / 4)][i] = "   WB   ;";
    }
}

// Update the cycle method
void Processor::cycle(const int i) {
    // Execute in reverse order to prevent data hazards
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
    // registers[1] = 2147483632; // x1 = 0x7FFFFFFF
    // registers[2] = 268435456;
    // Run for specified number of cycles
    for (int i = 0; i < cycles; i++) {
        cycle(i);
    }
    print_pipeline(cycles, false, inputFile);
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
