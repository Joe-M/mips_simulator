//Joseph Maag
//CDA

#include "spimcore.h"
#import <math.h>


/* ALU */
/* 10 Points */
void ALU(unsigned A,unsigned B,char ALUControl,unsigned *ALUresult,char *Zero)
{
	//The follwoing ifs and elses peform an action depending on the control value. The binary values are commented for clarity 
	if(ALUControl == 0){
		//000
		*ALUresult = A + B;

		

	}else if(ALUControl == 1){
		//001
		*ALUresult = A - B;
	}else if(ALUControl == 2){
		//010
		if(A<B)
			*ALUresult = 1;
		else
			*ALUresult = 0;
	}else if(ALUControl == 3){
		//011
		if(A<B)
			*ALUresult = 1;
		else
			*ALUresult = 0;
	}else if(ALUControl == 4){
		//100
		*ALUresult = A & B;
	}else if(ALUControl == 5){
		//101
		*ALUresult = A | B;
	}else if(ALUControl == 6){
		//110
		//Shifts, or extends, the binary value by 16 bits
		B = B << 16;
	}else if(ALUControl == 7){
		//111
		*ALUresult = ~A;
	}


}

/* instruction fetch */
/* 10 Points */
int instruction_fetch(unsigned PC,unsigned *Mem,unsigned *instruction)
{
	if(PC % 4 !=0){
		//%4 checks if the word is aligned since all word addresses should be multiples of 4.
		//If it's not aligned, return 1 since it's not a valid address
		return 1;
	}
	unsigned instructiona = Mem[PC >> 2];
	//PC >> 2 is similar to muliplying by 4 to get the actual address in the array of memory
	*instruction = instructiona;
	//assign the loaded instruction to the instrcution value
	return 0;
	

}


/* instruction partition */
/* 10 Points */
void instruction_partition(unsigned instruction, unsigned *op, unsigned *r1,unsigned *r2, unsigned *r3, unsigned *funct, unsigned *offset, unsigned *jsec)
{


	//All values use a binary mask to get specific binary values at specific areas in the word. 
	//Example: a mask for the op value is 11111100000000000000000000000000. If you '&'this with the word, the resulting number 
	//will only have 1's where both the instruction and the mask have a 1.. meaning the first 6 binary values. This way, we pull just that first section from the instruction. 

	*op = instruction >>26;
	unsigned r1Mask = 65011712;
	//All mask value are decimal versions of specific binary numbers made specifically to get each value.
	//65011712 == 11111100000000000000000000000000
	unsigned actualr1 = r1Mask & instruction;
	actualr1 = actualr1 >> 21;
	*r1 = actualr1;

	unsigned r2Mask = 2031616;
	unsigned actualr2 = r2Mask & instruction;
	actualr2 = actualr2 >> 16;
	*r2 = actualr2;

	unsigned r3Mask = 63488;
	unsigned actualr3 = r3Mask & instruction;
  	actualr3 = actualr3 >> 11;
  	*r3 = actualr3;

  	unsigned functMask = 63;
	unsigned actualFunct = functMask & instruction;
	*funct = actualFunct;
  
  	unsigned offsetMask = 65535;
	unsigned actualOffset = offsetMask & instruction;
	*offset = actualOffset;

	unsigned jsecMask = 67108863;
	unsigned actualJsec = jsecMask & instruction;
	*jsec = actualJsec;

}



/* instruction decode */
/* 15 Points */
int instruction_decode(unsigned op,struct_controls *controls)
{
	//the op value is used to determine the operation. Each operation has a specific number that defines it
	//the switch-case function tests the value to find a valid operation that matches, then assigns the control values based on that.
	switch (op){
		case 8:
		{
			//addi
			controls->ALUOp = 0;//000
			controls->RegDst = 0;
			controls->Jump = 0;
			controls->Branch = 0;
			controls->MemRead = 0;
			controls->MemtoReg = 0;
			controls->MemWrite = 0;
			controls->ALUSrc = 1;
			controls->RegWrite = 1;
			break;
		}

		case 0:{
			//add/rtype values
			controls->ALUOp =7;//111
			controls->RegDst = 1;
			controls->Jump = 0;
			controls->Branch = 0;
			controls->MemRead = 0;
			controls->MemtoReg = 0;
			controls->MemWrite = 0;
			controls->ALUSrc = 1;
			controls->RegWrite = 1;
			break;
		}

		case 34:{
			
			//sub
			controls->ALUOp =1;
			//r2 is source?
			controls->RegDst = 1;
			controls->Jump = 0;
			controls->Branch = 0;
			controls->MemRead = 0;
			controls->MemtoReg = 0;
			controls->MemWrite = 1;
			controls->ALUSrc = 1;
			controls->RegWrite = 1;
			break;
		}

		case 43:{
			//sw
			controls->ALUOp =0;
			
			controls->RegDst = 0;
			controls->Jump = 0;
			controls->Branch = 0;
			controls->MemRead = 0;
			controls->MemtoReg = 0;
			controls->MemWrite = 1;
			controls->ALUSrc = 1;
			controls->RegWrite = 0;
			break;
		}
		case 35:{
			//lw
			controls->ALUOp =0;
			
			controls->RegDst = 0;
			controls->Jump = 0;
			controls->Branch = 0;
			controls->MemRead = 1;
			controls->MemtoReg = 1;
			controls->MemWrite = 0;
			controls->ALUSrc = 1;
			controls->RegWrite = 1;
			break;
		}
		case 4:{
			//branch
			controls->ALUOp =1;
			
			controls->RegDst = 2;
			controls->Jump = 0;
			controls->Branch = 1;
			controls->MemRead = 0;
			controls->MemtoReg = 2;
			controls->MemWrite = 0;
			controls->ALUSrc = 0;
			controls->RegWrite = 0;
			break;
		}
		case 2:{
			//break
			controls->ALUOp =0;
			
			controls->RegDst = 0;
			controls->Jump = 1;
			controls->Branch = 0;
			controls->MemRead = 0;
			controls->MemtoReg = 0;
			controls->MemWrite = 1;
			controls->ALUSrc = 0;
			controls->RegWrite = 0;
			break;

		}case 15:{
			//takes care of the lui case, aka shift
			controls->ALUOp =6;
			
			controls->RegDst = 0;
			controls->Jump = 0;
			controls->Branch = 0;
			controls->MemRead = 0;
			controls->MemtoReg = 0;
			controls->MemWrite = 0;
			controls->ALUSrc = 1;
			controls->RegWrite = 1;
			break;

		}
		default:
			return 1;

	}

	return 0;

}

/* Read Register */
/* 5 Points */
void read_register(unsigned r1,unsigned r2,unsigned *Reg,unsigned *data1,unsigned *data2)
{
	r1 = Reg[r1];
	r2 = Reg[r2];

	//Simply pulls the values out of "memory" from their address
	*data1 = r1;
	*data2 = r2;

}


/* Sign Extend */
/* 10 Points */
void sign_extend(unsigned offset,unsigned *extended_value)
{
	unsigned maskForSign = 4294901760;
	unsigned maskForUnsigned = 65535;
	
	unsigned sign = offset>>15;
	//The "sign" number is the offset "pushed" or decrememnted by 15 bits to detect if further 1's extend beyond the 16 bits
	//If so, its defintely a signed number, and that means its negative
	if (sign == 1)
	{
		//if its negative and signed, use a mask that will "extend" the value as a signed value (with 1's extending)
		*extended_value = offset | maskForSign;
	}else
	*extended_value = maskForUnsigned & offset;
	//else if it's not signed, just use a regular mask with no sign to extend the value as a positive, unsigned numbers

}

/* ALU operations */
/* 10 Points */
int ALU_operations(unsigned data1,unsigned data2,unsigned extended_value,unsigned funct,char ALUOp,char ALUSrc,unsigned *ALUresult,char *Zero)
{
	//This will find the specific r-type operations based on the 'function' parameter of each instruction
	switch(ALUOp){
		case 7://000
		{


			switch(funct){
				case 32:{
					//add
					ALUOp = 0;//000
					break;
				}
				case 42:{
					//slt
					ALUOp = 2;//010
					break;
				}
				case 43:{
					//sltu
					ALUOp = 3; //011
					break;
				}
				case 34:{
					//sub
					ALUOp = 1;//001
					break;
				}
				case 6:{
					//shift
					ALUOp = 1;//001
					break;
				}
				case 36:{
					//and
					ALUOp = 4;//001
					break;
				}
				case 37:{
					//or
					ALUOp = 5;//001
					break;
				}
				default:
					return 1;
					//not valid?
				
			}
			break;
		}
		



	}
	if(ALUSrc == 1){
			//ALUSrc is asserted when a constant will be used. So the second data value will be assigned an extended version of
			//the 16 bit constant , so it can be used in the ALU
                data2 = extended_value;
        }


	ALU(data1, data2, ALUOp, ALUresult,Zero);

	return 0 ;



}

/* Read / Write Memory */
/* 10 Points */
int rw_memory(unsigned ALUresult,unsigned data2,char MemWrite,char MemRead,unsigned *memdata,unsigned *Mem)
{
	//Simply assigns a value to the memory array is MemWrite is asserted, and loads a value from the array
	//if memread is asserted. ALUresult>>2 the proper aligned version of the data address
	if (MemRead == 1)
	{
		*memdata = Mem[ALUresult>>2];
	}
	if (MemWrite == 1)
	{
		Mem[ALUresult >> 2] = data2;
	}
	return 0;

}


/* Write Register */
/* 10 Points */
void write_register(unsigned r2,unsigned r3,unsigned memdata,unsigned ALUresult,char RegWrite,char RegDst,char MemtoReg,unsigned *Reg)
{
	//Writers to registers based on control values. r3 and r2 are used as "addresses" to direct data to the proper register
	//MemToreg helps determine what value will be stored, whether its from memory or the ALU

	if(RegWrite == 1){
		if(MemtoReg == 1){
			//data is coming from memory
			if(RegDst == 1){
				//depending on whether the the data is from a constant or not will determine where in the word the destination register is specified
				//In this case it would be r3/rd group of bits
				Reg[r3] = memdata;
			}else{
				//in this case it would be rt/r2
				Reg[r2] = memdata;
			}
		}else{
			//same, but the data will com from the ALU
			if(RegDst == 1){
				Reg[r3] = ALUresult;
			}else{
				Reg[r2] = ALUresult;
			}
		}
	
	}

}

/* PC update */
/* 10 Points */
void PC_update(unsigned jsec,unsigned extended_value,char Branch,char Jump,char Zero,unsigned *PC)
{
		*PC += 4;
		//standard PC increment by 4
		if(Jump == 1){
			//This is entered when a jump i required
			unsigned withMask = *PC & 4026531840;
			//Simplify to the 4 highest bits to get the base of the new address for PC
			jsec = jsec << 2;
			//jsec is mulitplyed by 4 for the aligned value
			*PC = withMask | jsec;
			//Both are combined to generate the proper PC for the jumped to address
		}
		else if(Branch == 1 ){
			//A branch will simply increment the PC to get to the proper address
			*PC += extended_value << 2;
			// add and * 4
		}

}


