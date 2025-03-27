#include <iostream>
#include <fstream>  //Provides file handling classes: ifstream (input), ofstream (output), and fstream (both).
#include <sstream>  //error fixed //Provides stringstream, istringstream, and ostringstream for string manipulation.
#include <vector>
#include <map>
#include <cstdint>  //Defines fixed-width integer types like int8_t, uint16_t, int32_t, uint64_t.
#include <iomanip>  //Provides manipulators like std::setw, std::setprecision, std::fixed, std::hex, etc.
#include "Processor.hpp"

// Processor constructor implementation
Processor::Processor(const string& filename, const int cyclecount) {
    registers[0] = 0; // x0 is hardwired to 0
    loadInstructions(filename, cyclecount);

    // Initialize pipeline stages
    ifStage = new InstructionFetch(this);
    idStage = new InstructionDecode(this);
    exStage = new Execute(this);
    memStage = new MemoryAccess(this);
    wbStage = new WriteBack(this);
}

// Processor destructor implementation
Processor::~Processor() {
    delete ifStage;
    delete idStage;
    delete exStage;
    delete memStage;
    delete wbStage;
}

// Now implement the Processor methods
void Processor::setBranch(bool taken, uint32_t target) {
    isBranch = taken;
    branchTarget = target;
}

int32_t Processor::getRegister(int index) const {
    if (index == 0) return 0; // x0 is hardwired to 0
    return registers[index];
}

void Processor::setRegister(int index, int32_t value) {
    if (index != 0) { // Cannot modify x0
        registers[index] = value;
    }
}

uint32_t Processor::getMemory(uint32_t address) const {
    auto it = memory.find(address);
    if (it != memory.end()) {
        return it->second;
    }
    return 0; // Return 0 for uninitialized memory
}

void Processor::setMemory(uint32_t address, uint32_t value) {
    memory[address] = value;
}

uint32_t Processor::getInstruction(uint32_t address) const {
    auto it = instructionMemory.find(address);
    if (it != instructionMemory.end()) {
        return it->second;
    }
    return 0; // Return 0 for non-existent instruction
}

// Implementation of InstructionDecode helper methods
InstructionType InstructionDecode::getInstructionType(uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F; // Extract opcode (bits 0-6)

    switch (opcode) {
    case 0b0110011: // R-type
        return InstructionType::R_TYPE;
    case 0b0010011: // I-type ALU
    case 0b0000011: // I-type Load
    case 0b1100111: // JALR
        return InstructionType::I_TYPE;
    case 0b0100011: // S-type
        return InstructionType::S_TYPE;
    case 0b1100011: // B-type
        return InstructionType::B_TYPE;
    case 0b0110111: // LUI
    case 0b0010111: // AUIPC
        return InstructionType::U_TYPE;
    case 0b1101111: // JAL
        return InstructionType::J_TYPE;
    default:
        return InstructionType::UNKNOWN;
    }
}

int32_t InstructionDecode::extractImmediate(uint32_t instruction, InstructionType type) {
    int32_t imm = 0;

    switch (type) {
    case InstructionType::I_TYPE:
        // imm = (instruction >> 20) & 0xFFF;
        // if (imm & 0x800) imm |= 0xFFFFF000;
        imm = ((int32_t)instruction) >> 20;
        break;
    case InstructionType::S_TYPE:
        // S-type immediate: bits 31-25 and 11-7
        imm = ((instruction >> 25) & 0x7F) << 5;
        imm |= (instruction >> 7) & 0x1F;
        // Sign extension
        if (imm & 0x800) imm |= 0xFFFFF000;
        break;
    case InstructionType::B_TYPE:
        // B-type immediate: bits 31, 7, 30-25, 11-8
        imm = ((instruction >> 31) & 0x1) << 12;
        imm |= ((instruction >> 7) & 0x1) << 11;
        imm |= ((instruction >> 25) & 0x3F) << 5;
        imm |= ((instruction >> 8) & 0xF) << 1;
        // Sign extension
        if (imm & 0x1000) imm |= 0xFFFFE000;
        break;
    case InstructionType::U_TYPE:
        imm = ((instruction >> 12) & 0xFFFFF) << 12;
        break;
    case InstructionType::J_TYPE:
        // J-type immediate: imm[20|10:1|11|19:12]
        imm = (((instruction >> 31) & 0x1) << 20) |    // imm[20]
            (((instruction >> 21) & 0x3FF) << 1) |    // imm[10:1]
            (((instruction >> 20) & 0x1) << 11) |     // imm[11]
            (((instruction >> 12) & 0xFF) << 12);     // imm[19:12]
        // Sign–extend the 21–bit immediate:
        if (imm & (1 << 20))
            imm |= ~((1 << 21) - 1);
        break;
    default:
        imm = 0;
    }
    return imm;
}

ControlSignals InstructionDecode::generateControlSignals(uint32_t instruction) {
    ControlSignals signals;
    uint32_t opcode = instruction & 0x7F;

    switch (opcode) {
    case 0b0110011: // R-type
        signals.regWrite = true;
        signals.aluOp = 2;
        break;
    case 0b0010011: // I-type ALU
        signals.regWrite = true;
        signals.aluSrc = true;
        signals.aluOp = 2;
        break;
    case 0b0000011: // I-type Load
        signals.regWrite = true;
        signals.memRead = true;
        signals.memToReg = true;
        signals.aluSrc = true;
        signals.aluOp = 0;
        break;
    case 0b0100011: // S-type
        signals.memWrite = true;
        signals.aluSrc = true;
        signals.aluOp = 0;
        break;
    case 0b1100011: // B-type
        signals.branch = true;
        signals.aluOp = 1;
        break;
    case 0b0010111: // AUIPC
        signals.regWrite = true;
        signals.aluSrc = true;
        // Use a custom aluOp value (4) to signal AUIPC in Execute stage.
        signals.aluOp = 4;
        break;
    case 0b1101111: // JAL
    case 0b1100111: // JALR
        signals.regWrite = true;
        signals.jump = true;
        break;
    case 0b0110111: // LUI
        signals.regWrite = true;
        signals.aluSrc = true;
        // Use a custom aluOp value (e.g. 3) to signal LUI in Execute stage.
        signals.aluOp = 3;
        break;
    default:
        // Unknown or unsupported instruction
        break;
    }
    return signals;
}

// Update getALUControl in the Execute class:
uint32_t Execute::getALUControl(uint32_t aluOp, uint32_t funct3, uint32_t funct7) {
    if (aluOp == 0) {
        return 2; // add for load/store
    } else if (aluOp == 1) {
        return 6; // subtract for branch
    } else {
        // Check for M-extension instructions first.
        if (funct7 == 0x01) {
            if (funct3 == 0x0)
                return 11; // mul
            else if (funct3 == 0x4)
                return 12; // div
            else if (funct3 == 0x6)
                return 13; // rem
        }
        // R-type / I-type ALU instructions
        if (funct3 == 0) {
            if (funct7 == 0x00)
                return 2; // add
            else if (funct7 == 32)
                return 6; // subtract
        } else if (funct3 == 7) {
            return 0; // AND
        } else if (funct3 == 6) {
            return 1; // OR
        } else if (funct3 == 1) {
            return 7; // SLL
        } else if (funct3 == 5) {
            return 8; // SRL/SRA
        } else if (funct3 == 2) {
            return 9; // SLT
        } else if (funct3 == 3) {
            return 10; // SLTU
        } else if (funct3 == 4) {
            return 3; // XOR
        }
    }
    return 15; // Default (should not reach here)
}

// Update performALU in the Execute class to handle the new ALU control codes:
uint32_t Execute::performALU(uint32_t aluControl, uint32_t input1, uint32_t input2, bool& zero) {
    uint32_t result = 0;
    switch (aluControl) {
    case 0: // AND
        result = input1 & input2; break;
    case 1: // OR
        result = input1 | input2; break;
    case 2: // ADD
        result = input1 + input2; break;
    case 3: // XOR
        result = input1 ^ input2; break;
    case 6: // SUB
        result = input1 - input2; break;
    case 7: // SLL
        result = input1 << (input2 & 0x1F); break;
    case 8: // SRL/SRA
        // Using logical shift; for arithmetic shifts, additional checks are needed.
        result = input1 >> (input2 & 0x1F); break;
    case 9: // SLT
        result = ((int32_t)input1 < (int32_t)input2) ? 1 : 0; break;
    case 10: // SLTU
        result = (input1 < input2) ? 1 : 0; break;
    case 11: // MUL (M-extension)
        result = (int32_t)input1 * (int32_t)input2; break;
    case 12: // DIV (M-extension)
        result = (input2 == 0) ? -1 : (int32_t)input1 / (int32_t)input2; break;
    case 13: // REM (M-extension)
        result = (input2 == 0) ? input1 : (int32_t)input1 % (int32_t)input2; break;
    default:
        result = 0;
    }
    zero = (result == 0);
    return result;
}

void Processor::print_pipeline(int cycles, bool forwardingEnabled, const string& inputFile) {
    // Extract the base name from inputFile (simple extraction assuming no directories in inputFile)
    size_t pos = inputFile.find_last_of("/\\");
    string baseFilename = (pos == string::npos) ? inputFile : inputFile.substr(pos + 1);
    // Remove extension if present
    pos = baseFilename.find_last_of(".");
    if (pos != string::npos)
        baseFilename = baseFilename.substr(0, pos);

    // Construct output file name based on forwarding flag
    string outputFileName = "../outputfiles/" + baseFilename;
    outputFileName += (forwardingEnabled ? "_forward_out.txt" : "_noforward_out.txt");

    // Open the output file stream
    ofstream outFile(outputFileName);
    if (!outFile) {
        cerr << "Error opening file: " << outputFileName << endl;
        return;
    }

    // Print cycle header into file
    outFile << "Cycle Count      :";
    for (int i = 0; i < cycles; i++) {
        if (i < 10) {
            outFile << "   " << i << "   ";
        } else if (i < 100) {
            outFile << "  " << i << "   ";
        } else {
            outFile << "  " << i << "  ";
        }
    }
    outFile << endl;
    // Print pipeline matrix 
    for (const auto& entry : pipelineMatrix) {
        // Use the instruction string from instructionLines map
        string instrStr = instructionLines[entry.first].second;
        // Ensure fixed width (17 characters in this example)
        outFile << left << setw(17) << instrStr << ":";
        string prev = "";
        for (const auto& stage : entry.second) {
            if (prev != "      ;" && prev == stage) {
                outFile << "  -   ;";
            } else {
                outFile << stage;
            }
            prev = stage;
        }
        outFile << endl;
    }
    outFile.close();
}

void Processor::loadInstructions(const string& filename, const int cyclecount) {
    ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        exit(1);
    }

    string line;
    uint32_t address = 0;
    while (getline(inputFile, line)) {
        if (line.empty()) continue;
        // Parse instruction line
        istringstream iss(line);
        string hexCode;
        iss >> hexCode;
        string fullInstruction;
        getline(iss, fullInstruction); // Get the remainder of the line
        // Trim leading whitespace
        size_t start = fullInstruction.find_first_not_of(" \t");
        if (start != string::npos) {
            fullInstruction = fullInstruction.substr(start);
        }
        // Remove content enclosed in '<' and '>' if it exists
        size_t anglePos = fullInstruction.find('<');
        if (anglePos != string::npos) {
            size_t endPos = fullInstruction.find('>', anglePos);
            if (endPos != string::npos) {
                fullInstruction.erase(anglePos, endPos - anglePos + 1);
                // Optionally trim again if needed
                size_t newStart = fullInstruction.find_first_not_of(" \t");
                if (newStart != string::npos) {
                    fullInstruction = fullInstruction.substr(newStart);
                }
            }
        }
        // Convert hex to binary
        uint32_t binInstruction = stoul(hexCode, nullptr, 16);
        // Store in memory
        instructionMemory[address] = binInstruction;
        pipelineMatrix[(address / 4)] = vector<string>(cyclecount, "      ;");
        // Store the full instruction string
        instructionLines.push_back(make_pair(address, fullInstruction));
        address += 4; // Instructions are 4 bytes each
    }
    inputFile.close();
}