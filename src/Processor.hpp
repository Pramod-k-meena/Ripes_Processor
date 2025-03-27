#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

using namespace std;

// Base class for pipeline stages
class PipelineStage {
    public:
        virtual void process(const int i = 0) = 0;
        virtual ~PipelineStage() {}
    };
    
class Processor;


// Enum to represent different instruction types in RISC-V
enum class InstructionType {
    R_TYPE, // Register-Register operations
    I_TYPE, // Immediate and Load operations
    S_TYPE, // Store operations
    B_TYPE, // Branch operations
    U_TYPE, // Upper immediate operations
    J_TYPE, // Jump operations
    UNKNOWN // For unrecognized instructions
};

// Struct to hold control signals generated during ID stage
struct ControlSignals {
    bool regWrite = false; // Whether to write to register file
    bool memRead = false; // Whether to read from memory
    bool memWrite = false; // Whether to write to memory
    bool memToReg = false; // Whether to write memory data or ALU result to register
    bool aluSrc = false; // Whether ALU's second operand is register or immediate
    bool branch = false; // Whether instruction is a branch
    bool jump = false; // Whether instruction is a jump
    int aluOp = 0; // What operation ALU should perform
};

struct hazard_detection {
    bool is_hazard = false;
};

// Instruction Fetch stage class
class InstructionFetch : public PipelineStage {
private:
    Processor* processor;
public:
    InstructionFetch(Processor* proc) : processor(proc) {}
    void process(const int i) override;
};

// Instruction Decode stage class
class InstructionDecode : public PipelineStage {
private:
    Processor* processor;

    // Helper methods
    InstructionType getInstructionType(uint32_t instruction);
    int32_t extractImmediate(uint32_t instruction, InstructionType type);
    ControlSignals generateControlSignals(uint32_t instruction);

public:
    InstructionDecode(Processor* proc) : processor(proc) {}
    void process(const int i) override;
};

// Execute stage class
class Execute : public PipelineStage {
private:
    Processor* processor;

    // Helper methods
    uint32_t getALUControl(uint32_t aluOp, uint32_t funct3, uint32_t funct7);
    uint32_t performALU(uint32_t aluControl, uint32_t input1, uint32_t input2, bool& zero);

public:
    Execute(Processor* proc) : processor(proc) {}
    void process(const int i) override;
};

// Memory Access stage class
class MemoryAccess : public PipelineStage {
private:
    Processor* processor;
public:
    MemoryAccess(Processor* proc) : processor(proc) {}
    void process(const int i) override;
};

// Write Back stage class
class WriteBack : public PipelineStage {
private:
    Processor* processor;
public:
    WriteBack(Processor* proc) : processor(proc) {}
    void process(const int i) override;
};

// Struct for WB control signals passed between pipeline registers
struct WBControl {
    bool regWrite = false;
    bool memToReg = false;
};

// Struct for MEM control signals passed between pipeline registers
struct MEMControl {
    bool branch = false;
    bool memWrite = false;
    bool memRead = false;
    bool jump = false;
};
struct EXControl {
    int aluOp = 0;
    bool aluSrc = false;
};

// Definition for ForwardingSignals (stub)
struct ForwardingSignals {
    bool forwardA = false;
    bool forwardB = false;
};

// Pipeline registers definitions available to both header and cpp
struct IF_ID_Register {
    int32_t rs1data = 0;
    int32_t rs2data = 0;
    bool rs1forwarded = false;
    bool rs2forwarded = false;
    uint32_t rs2 = 0;
    uint32_t rd = 0;
    hazard_detection hazard; // Placeholder
    uint32_t pc = 0;
    uint32_t instruction = 0;
    bool isStall = false;
};

struct ID_EX_Register {
    WBControl wb;   // Placeholder
    MEMControl mem; // Placeholder
    EXControl ex;   // Placeholder
    uint32_t pc = 0;
    int32_t readData1 = 0;
    int32_t readData2 = 0;
    int32_t immediate = 0;
    uint32_t funct3 = 0;
    uint32_t funct7 = 0;
    uint32_t rd = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    uint32_t instruction = 0;
    bool isStall = false;
};

struct EX_MEM_Register {
    uint32_t pc = 0;
    WBControl wb;   // Placeholder
    MEMControl mem; // Placeholder
    uint32_t branchTarget;
    bool zero = false;
    int32_t aluResult = 0;
    int32_t readData2 = 0;
    uint32_t rd = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    uint32_t instruction = 0;
    bool isStall = false;
};

struct MEM_WB_Register {
    uint32_t pc = 0;
    WBControl wb; // Placeholder
    uint32_t readData = 0;
    int32_t aluResult = 0;
    uint32_t rd = 0;
    uint32_t instruction = 0;
    bool isStall = false;
};

// Class representing a RISC-V processor with forwarding
class Processor {
private:
    int32_t registers[32] = { 0 }; // RISC-V has 32 registers
    map<uint32_t, uint32_t> memory; // Memory is sparse, so using a map
    uint32_t pc = 0; // Program counter

    // Pipeline registers
    IF_ID_Register if_id;
    ID_EX_Register id_ex;
    EX_MEM_Register ex_mem;
    MEM_WB_Register mem_wb;

    // Forwarding unit
    ForwardingSignals forwarding;

    // Storage for instructions loaded from file
    vector<pair<uint32_t, string>> instructionLines; // Raw instruction lines from file
    map<uint32_t, uint32_t> instructionMemory; // Decoded instructions

    bool isBranch = false; // Flag to indicate if current instruction is a branch
    uint32_t branchTarget = 0; // Target address if branch is taken

    struct PipelineEntry {
        string instruction;    // Instruction text (e.g., "addi")
        vector<string> stages; // Stages at each cycle (IF, ID, EX, etc.)
    };
    vector<PipelineEntry> pipelineTrace;
    int currentCycle = 0;

public:
    // Pipeline stages
    InstructionFetch* ifStage = nullptr;
    InstructionDecode* idStage = nullptr;
    Execute* exStage = nullptr;
    MemoryAccess* memStage = nullptr;
    WriteBack* wbStage = nullptr;
    map<uint32_t, vector<string>> pipelineMatrix;
    // Constructor takes a filename to load instructions from
    Processor(const string& filename, const int cyclecount);
    ~Processor();
    bool hazard_in_id = false;
    uint32_t getnoofinstructions() { return instructionLines.size(); }
    void loadInstructions(const string& filename, const int cyclecount);
    void run(int cycles, const string& inputFile);
    void cycle(const int i);

    uint32_t getPC() const { return pc; }
    void setPC(uint32_t newPC) { pc = newPC; }

    int32_t getRegister(int index) const;
    void setRegister(int index, int32_t value);
    uint32_t getMemory(uint32_t address) const;
    void setMemory(uint32_t address, uint32_t value);
    uint32_t getInstruction(uint32_t address) const;

    // Get/set methods for pipeline registers
    IF_ID_Register& getIF_ID() { return if_id; }
    ID_EX_Register& getID_EX() { return id_ex; }
    EX_MEM_Register& getEX_MEM() { return ex_mem; }
    MEM_WB_Register& getMEM_WB() { return mem_wb; }
    ForwardingSignals& getForwarding() { return forwarding; }

    // Update forwarding signals based on current pipeline state
    void updateForwardingSignals();

    // Check for hazards that require stalling
    bool checkForHazards();

    // Methods for handling branches
    void setBranch(bool taken, uint32_t target);
    bool isBranchTaken() const { return isBranch; }
    uint32_t getBranchTarget() const { return branchTarget; }

    void print_pipeline(int cycles, bool forwardingEnabled, const string& inputFile);
};

#endif // PROCESSOR_HPP