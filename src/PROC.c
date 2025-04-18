#include <limits.h>
#include <stdio.h>	/* fprintf(), printf() */
#include <stdlib.h>	/* atoi() */
#include <stdint.h>	/* uint32_t */
#include <unistd.h>

#include "RegFile.h"
#include "Syscall.h"
#include "utils/heap.h"
#include "elf_reader/elf_reader.h"

typedef enum Opcode {
	OP_UNSPECIFIED = 0,
	OP_BRANCH_COMPARISON = 1,
	OP_JUMP = 2,
	OP_JAL = 3,
	OP_BEQ = 4,
	OP_BNE = 5,
	OP_BLEZ = 6,
	OP_BGTZ = 7,
	OP_ADDI = 8,
	OP_ADDIU = 9,
	OP_SLTI = 10,
	OP_SLTIU = 11,
	OP_ANDI = 12,
	OP_ORI = 13,
	OP_XORI = 14,
	OP_LUI = 15,
	OP_LB = 32,
	OP_LH = 33,
	OP_LWL = 34,
	OP_LW = 35,
	OP_LBU = 36,
	OP_LHU = 37,
	OP_LWR = 38,
	OP_SB = 40,
	OP_SH = 41,
	OP_SWL = 42,
	OP_SW = 43,
	OP_SWR = 46,
} Opcode;

typedef enum Funct {
	FUNCT_SLL = 0,
	FUNCT_SRL = 2,
	FUNCT_SRA = 3,
	FUNCT_SLLV = 4,
	FUNCT_SRLV = 6,
	FUNCT_SRAV = 7,
	FUNCT_JR = 8,
	FUNCT_JALR = 9,
	FUNCT_SYSCALL = 12,
	FUNCT_BREAK = 13,
	FUNCT_MFHI = 16,
	FUNCT_MTHI = 17,
	FUNCT_MFLO = 18,
	FUNCT_MTLO = 19,
	FUNCT_MULT = 24,
	FUNCT_MULTU = 25,
	FUNCT_DIV = 26,
	FUNCT_DIVU = 27,
	FUNCT_ADD = 32,
	FUNCT_ADDU = 33,
	FUNCT_SUB = 34,
	FUNCT_SUBU = 35,
	FUNCT_AND = 36,
	FUNCT_OR = 37,
	FUNCT_XOR = 38,
	FUNCT_NOR = 39,
	FUNCT_SLT = 42,
	FUNCT_SLTU = 43,
} Funct;

uint8_t getOpcode(uint32_t instruction) {
	return instruction >> 26;
}

uint8_t getRS(uint32_t instruction) {
	return (instruction << 6) >> 27;	
}

uint8_t getRT(uint32_t instruction) {
	return (instruction << 11) >> 27;
}

uint8_t getRD(uint32_t instruction) {
	return (instruction << 16) >> 27;
}

uint8_t getShamt(uint32_t instruction) {
	return (instruction << 21) >> 27;
}

uint8_t getFunct(uint32_t instruction) {
	return (instruction << 26) >> 26;
}

int32_t getImmediate(uint32_t instruction) {
	return (instruction << 16) >> 16;
}

uint32_t getAddress(uint32_t instruction) {
	return (instruction << 6) >> 6;
}

uint32_t getCode(uint32_t instruction) {
	return (instruction << 6) >> 12;
}

bool additionWillOverflow(int32_t a, int32_t b) {
	return (a >= 0 && b > (INT_MAX - a)) || (a < 0 && b < (INT_MIN - a));
}

void printInstruction(uint32_t instruction) {
	uint8_t opcode = getOpcode(instruction);
	uint8_t rs = getRS(instruction);
	uint8_t rt = getRT(instruction);
	uint8_t rd = getRD(instruction);
	uint8_t shamt = getShamt(instruction);
	uint8_t funct = getFunct(instruction);
	int32_t immediate = getImmediate(instruction);
	uint32_t address = getAddress(instruction);
	printf("full instruction: %d, opcode: %d, rs: %d, rt: %d, rd: %d, shamt: %d, funct: %d, immediate: %d, address: %d\n",
			instruction, opcode, rs, rt, rd, shamt, funct, immediate, address);
}

int32_t getRegValue(uint8_t n) {
	if (n > (sizeof(RegFile) / sizeof(int32_t))) {
		fprintf(stderr, "RegFile index out of bounds: %d\n", n);
		exit(1);
	}
	return RegFile[n];
}

void setRegValue(uint8_t n, int32_t value) {
	if (n == 0) return;
	if (n > (sizeof(RegFile) / sizeof(int32_t))) {
		fprintf(stderr, "RegFile index out of bounds: %d\n", n);
		exit(1);
	}
	RegFile[n] = value;
}

void test() {
	uint32_t instruction = 2112297087;
	printf("%d\n", getOpcode(instruction));
	printf("%d\n", getRS(instruction));
	printf("%d\n", getRT(instruction));
	printf("%d\n", getRD(instruction));
	printf("%d\n", getShamt(instruction));
	printf("%d\n", getFunct(instruction));
}

int main(int argc, char * argv[]) {
	if (argc == 1) {
		test();
		return 0;
	}

	/*
	 * This variable will store the maximum
	 * number of instructions to run before
	 * forcibly terminating the program. It
	 * is set via a command line argument.
	 */
	uint32_t MaxInstructions;

	/*
	 * This variable will store the address
	 * of the next instruction to be fetched
	 * from the instruction memory.
	 */
	uint32_t ProgramCounter;

	/*
	 * This variable will store the instruction
	 * once it is fetched from instruction memory.
	 */
	uint32_t CurrentInstruction;

	//IF THE USER HAS NOT SPECIFIED ENOUGH COMMAND LINE ARUGMENTS
	if(argc < 3){

		//PRINT ERROR AND TERMINATE
		fprintf(stderr, "ERROR: Input argument missing!\n");
		fprintf(stderr, "Expected: file-name, max-instructions\n");
		return -1;

	}

     	//CONVERT MAX INSTRUCTIONS FROM STRING TO INTEGER	
	MaxInstructions = atoi(argv[2]);	

	//Open file pointers & initialize Heap & Regsiters
	initHeap();
	initFDT();
	initRegFile(0);

	//LOAD ELF FILE INTO MEMORY AND STORE EXIT STATUS
	int status = LoadOSMemory(argv[1]);

	//IF LOADING FILE RETURNED NEGATIVE EXIT STATUS
	if(status < 0){ 
		
		//PRINT ERROR AND TERMINATE
		fprintf(stderr, "ERROR: Unable to open file at %s!\n", argv[1]);
		return status; 
	
	}

	printf("\n ----- BOOT Sequence ----- \n");
	printf("Initializing sp=0x%08x; gp=0x%08x; start=0x%08x\n", exec.GSP, exec.GP, exec.GPC_START);

	RegFile[28] = exec.GP;
	RegFile[29] = exec.GSP;
	RegFile[31] = exec.GPC_START;

	printRegFile();

	printf("\n ----- Execute Program ----- \n");
	printf("Max Instruction to run = %d \n",MaxInstructions);
	fflush(stdout);
	ProgramCounter = exec.GPC_START;
	
	/***************************/
	/* ADD YOUR VARIABLES HERE */
	/***************************/
	

	uint32_t jumpAddress = 0;
	int jumpState = 0;
	int i;
	for (i = 0; i < MaxInstructions; i++) {

		//FETCH THE INSTRUCTION AT 'ProgramCounter'		
		CurrentInstruction = readWord(ProgramCounter, false);

		//Print contents of the register file after each instruction
		
		printf("\nBegin cycle %d\n", i);
		printRegFile();//only suggested for Debug, comment this line to reduce output
		
		/********************************/
		/* ADD YOUR IMPLEMENTATION HERE */
		/********************************/
		
		uint8_t rs = getRS(CurrentInstruction);
		uint8_t rt = getRT(CurrentInstruction);
		uint8_t rd = getRD(CurrentInstruction);
		uint8_t shamt = getShamt(CurrentInstruction);
		uint8_t funct = getFunct(CurrentInstruction);
		int32_t immediate = getImmediate(CurrentInstruction);
		uint32_t address = getAddress(CurrentInstruction);
		uint32_t code = getCode(CurrentInstruction);
		
		printInstruction(CurrentInstruction);
		
		if (CurrentInstruction == 0) goto end;

		switch (getOpcode(CurrentInstruction)) {
		case OP_UNSPECIFIED:
		{
			switch (funct) {
			case FUNCT_JR:
			{
				int32_t addr = getRegValue(rs);
				jumpAddress = addr << 2;
				jumpState = 2;
				break;
			}
			case FUNCT_SYSCALL:
			{
				SyscallExe(code);
				break;
			}
			case FUNCT_ADD:
			{
				int32_t left = getRegValue(rt);
				int32_t right = getRegValue(rs);
				// check for overflow
				if (additionWillOverflow(left, right)) {
					// tood: raise exception
					fprintf(stderr, "Overflow");
					return 1;
				}
				setRegValue(rs, left + right);
				break;
			}
			case FUNCT_ADDU:
			{
				int32_t left = getRegValue(rt);
				int32_t right = getRegValue(rs);
				setRegValue(rs, left + right);
				break;
			}
			case FUNCT_SUB:
			{
				int32_t left = getRegValue(rt);
				int32_t right = getRegValue(rs);
				// check for overflow
				if (additionWillOverflow(left, -right)) {
					// tood: raise exception
					fprintf(stderr, "Overflow");
					return 1;
				}
				setRegValue(rs, left - right);
				break;
			}
			case FUNCT_SUBU:
			{
				int32_t left = getRegValue(rt);
				int32_t right = getRegValue(rs);
				setRegValue(rs, left - right);
				break;
			}
			case FUNCT_SLL:
			{
				int32_t initialValue = getRegValue(rt);
				setRegValue(rd, initialValue << shamt);
				break;
			}
			case FUNCT_MFHI:
			{
				int32_t value = getRegValue(32);
				setRegValue(rd, value);
				break;
			}
			case FUNCT_MTHI:
			{
				int32_t value = getRegValue(rs);
				setRegValue(32, value);
				break;
			}
			case FUNCT_MFLO:
			{
				int32_t value = getRegValue(33);
				setRegValue(rd, value);
				break;
			}
			case FUNCT_MTLO:
			{
				int32_t value = getRegValue(rs);
				setRegValue(33, value);
				break;
			}
			default:
			{
				// todo: remove
				fprintf(stderr, "Unknown Funct: %d\n", funct);
				return 1;
			}
			}
			break;
		}
		case OP_JUMP:
		{
			jumpAddress = address << 2;
			jumpState = 2;
			break;
		}
		case OP_BEQ:
		{
			int32_t a = getRegValue(rs);
			int32_t b = getRegValue(rt);
			if (a == b) {
				jumpAddress = address << 2;
				jumpState = 2;
				setRegValue(31, ProgramCounter);
				break;
			}
		}
		case OP_ADDI:
		{
			int32_t left = getRegValue(rs);
			if (additionWillOverflow(left, immediate)) {
				// todo: raise exception
				fprintf(stderr, "Overflow");
				return 1;
			}
			setRegValue(rt, left + immediate);
			break;
		} 
		case OP_ADDIU:
		{
			int32_t left = getRegValue(rs);
			setRegValue(rt, left + immediate);
			break;
		}
		case OP_SLTIU:
		{
			break;
		}
		case OP_ANDI:
		{
			int32_t a = getRegValue(rs);
			setRegValue(rt, a & immediate);
			break;
		}
		case OP_ORI:
		{
			int32_t a = getRegValue(rs);
			setRegValue(rt, a | immediate);
			break;
		}
		case OP_XORI:
		{
			int32_t a = getRegValue(rs);
			setRegValue(rt, a ^ immediate);
			break;
		}
		case OP_LUI:
		{
			int32_t value = getRegValue(rs);
			int32_t upperImmediate = immediate << 16;
			setRegValue(rt, value | upperImmediate);
			break;
		}
		case OP_LW:
		{
			int32_t addr = rt + immediate;
			uint32_t value = readWord(addr, false);
			setRegValue(rs, value);
			break;
		}
		// todo: remove
		default:
			fprintf(stderr, "Unknown Opcode: %d\n", getOpcode(CurrentInstruction));
			return 1;
		}
		
	end:
		ProgramCounter += 4;
		if (jumpState == 2) {
			--jumpState;
		} else if (jumpState == 1) {
			ProgramCounter = jumpAddress;
			--jumpState;
		}
	}
	//Print the final contents of the register file
	printRegFile();
	//Close file pointers & free allocated Memory
	closeFDT();
	CleanUp();

	return 0;
}
