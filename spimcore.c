#import <math.h>
#include "spimcore.h"

#define MEMSIZE (65536 >> 2)
#define REGSIZE 32
#define BUFSIZE 256

#define PCINIT 0x4000
#define SPINIT 0xFFFC
#define GPINIT 0xC000

static unsigned Mem[MEMSIZE];
static unsigned Reg[REGSIZE + 4];

#define MEM(addr) (Mem[addr >> 2])

#define PC (Reg[REGSIZE + 0])
#define Status (Reg[REGSIZE + 1])
#define LO (Reg[REGSIZE + 2])
#define HI (Reg[REGSIZE + 3])

const char RegName[REGSIZE + 4][6] = {
	"$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
	"$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", 
	"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7", 
	"$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
	"$pc", "$stat", "$lo", "$hi" };

#define NREG(name) (*Nreg(name))

const char RedirNull[] = "";
const char RedirPrefix[] = ">";

static char Buf[BUFSIZE];
static int Halt = 0;
static FILE *FP;
static char *Redir = (char *) RedirNull;

/*** DATAPATH Signals ***/
// names of instruction sections
unsigned instruction;
unsigned op,	// instruction [31-26]
	r1,	// instruction [25-21]
	r2,	// instruction [20-16]
	r3,	// instruction [15-11]
	funct,	// instruction [5-0]
	offset,	// instruction [15-0]
	jsec;	// instruction [25-0]

// control signals
struct_controls controls;

// Register output
unsigned data1,data2;

// sign extend
unsigned extended_value;

// ALU result
unsigned ALUresult;
char Zero;

// data read from Memory
unsigned memdata;

/******/


unsigned *Nreg(char *name)
{
	int i;

	for (i = 0; i < REGSIZE + 4; i++)
	{
		if (strcmp(name, RegName[i]) == 0)
			return &Reg[i];
		if (strcmp(name, RegName[i] + 1) == 0)
			return &Reg[i];
	}
	return NULL;
}

void Init(void)
{
	memset(Reg, 0, (REGSIZE + 4) * sizeof(unsigned));
	NREG("pc") = PCINIT;
	NREG("sp") = SPINIT;
	NREG("gp") = GPINIT;
}


void DisplayControlSignals(void)
{
	fprintf(stdout, "\tControl Signals: %0x%0x%0x%0x%03x%0x%0x%0x%0x\n", 
			controls.RegDst, 
			controls.Jump, 
			controls.Branch, 
			controls.MemRead, 
			controls.MemtoReg, 
			controls.ALUOp, 
			controls.MemWrite, 
			controls.ALUSrc, 
			controls.RegWrite);
}



void Step(void)
{
	/* fetch instruction from memory */
	Halt = instruction_fetch(PC,Mem,&instruction);

	if(!Halt)
	{
		/* partition the instruction */
		instruction_partition(instruction,&op,&r1,&r2,&r3,&funct,&offset,&jsec);

		/* instruction decode */
		Halt = instruction_decode(op,&controls);
	}

	if(!Halt)
	{
		/* read_register */
		read_register(r1,r2,Reg,&data1,&data2);

		/* sign_extend */
		sign_extend(offset,&extended_value);

		/* ALU */
		Halt = ALU_operations(data1,data2,extended_value,funct,controls.ALUOp,controls.ALUSrc,&ALUresult,&Zero);
	}

	if(!Halt)
	{
		/* read/write memory */
		Halt = rw_memory(ALUresult,data2,controls.MemWrite,controls.MemRead,&memdata,Mem);
	}

	if(!Halt)
	{
		/* write to register */
		write_register(r2,r3,memdata,ALUresult,controls.RegWrite,controls.RegDst,controls.MemtoReg,Reg);

		/* PC update */
		PC_update(jsec,extended_value,controls.Branch,controls.Jump,Zero,&PC);
	}
}

void DumpReg(void)
{
	int i;
	char bb[] = "     ";

	for (i = 0; i < REGSIZE + 4; i++)
	{
		fprintf(stdout, "%s %s%s %08x%s",
			(i % 4 == 0) ? Redir : "",
			RegName[i], bb + strlen(RegName[i]) * sizeof(char),
			Reg[i], (i % 4 == 3) ? "\n" : "     ");
	}
}

// Dump Memory Content where the addresses are in decimal format
void DumpMem(int from, int to)
{
	int i, mt, ma;

	(to < from) && (to = from);
	if (from == to)
	{
		fprintf(stdout, "%s %05d        %08x\n", Redir, from, Mem[from]);
	}
	else
	{
		mt = Mem[ma = from];
		for (i = from + 1; i <= to; i++)
		{
			if (i == to || Mem[i] != mt)
			{
				if (i == ma + 1)
					fprintf(stdout, "%s %05d        %08x\n",
						Redir, ma, mt);
				else
					fprintf(stdout, "%s %05d-%05d  %08x\n",
						Redir, ma, i - 1, mt);
				(i != to) && (mt = Mem[ma = i]);
			}
		}
	}
}


// Dump Memory Content in Hex format
void DumpMemHex(int from, int to)
{
	int i, mt, ma;

	(to < from) && (to = from);
	if (from == to)
	{
		fprintf(stdout, "%s %05x        %08x\n", Redir, from*4, Mem[from]);
	}
	else
	{
		mt = Mem[ma = from];
		for (i = from + 1; i <= to; i++)
		{
			if (i == to || Mem[i] != mt)
			{
				if (i == ma + 1)
					fprintf(stdout, "%s %05x        %08x\n",
						Redir, ma*4, mt);
				else
					fprintf(stdout, "%s %05x-%05x  %08x\n",
						Redir, ma*4, (i - 1)*4, mt);
				(i != to) && (mt = Mem[ma = i]);
			}
		}
	}
}



void DumpHex(int from, int to)
{
	int i, j;

	if (to < from)
	{
		for (i = from, j = 0; i >= to; i--, j++)
		{
			if (j % 4 == 0)
				fprintf(stdout, "%s %04x  ", Redir, (i << 2) + 3);
			fprintf(stdout, " %08x%s", Mem[i], (j % 4 == 3) ? "\n" : "");
		}
	}
	else
	{
		for (i = from, j = 0; i <= to; i++, j++)
		{
			if (j % 4 == 0)
				fprintf(stdout, "%s %04x  ", Redir, i << 2);
			fprintf(stdout, " %08x%s", Mem[i], (j % 4 == 3) ? "\n" : "");
		}
	}
	if (j % 4 != 0)
		fputc('\n', stdout);
}

void Loop(void)
{
	char *tp;
	int sc;

	Init();
	for (;;)
	{
		fprintf(stdout, "\n%s cmd: ", Redir);
		Buf[0] = '\0';
		if (fgets(Buf, BUFSIZE, stdin) == NULL)
			continue;
		if ((tp = strtok(Buf, " ,.\t\n\r")) == NULL)
			continue;
		fputc('\n', stdout);
		switch (*tp)
		{
			case 'g': case 'G':
				DisplayControlSignals();
				break;
			case 'r': case 'R':
				DumpReg();
				break;
			case 'm': case 'M':
				if ((tp = strtok(NULL, " ,.\t\n\r")) == NULL)
				{
					DumpMemHex(0, MEMSIZE);
				}
				else
				{
					sc = (int) strtoul(tp, (char **) NULL, 10);
					if ((tp = strtok(NULL, " ,.\t\n\r")) == NULL)
					{
						DumpMemHex(sc, MEMSIZE);
					}
					else
					{
						DumpMemHex(sc, (int) strtoul(tp, (char **) NULL, 10));
					}
				}
				break;
			case 's': case 'S':
				if ((tp = strtok(NULL, " ,.\t\n\r")) == NULL)
					sc = 1;
				else
					sc = (int) strtoul(tp, (char **) NULL, 10);
				while (sc-- > 0 && !Halt)
					Step();
				fprintf(stdout, "%s step\n", Redir);
				break;
			case 'c': case 'C':
				while (!Halt)
					Step();
				fprintf(stdout, "%s cont\n", Redir);
				break;
			case 'h': case 'H':
				fprintf(stdout, "%s %s\n", Redir, Halt ? "true" : "false");
				break;
			case 'p': case 'P':
				rewind(FP);
				sc = 0;
				while (!feof(FP))
				{
					if (fgets(Buf, BUFSIZE, FP))
						fprintf(stdout, "%s % 5d  %s", Redir, sc++, Buf);
				}
				break;
			case 'i': case 'I':
				fprintf(stdout, "%s %d\n", Redir, MEMSIZE);
				break;
			case 'd': case 'D':
				if ((tp = strtok(NULL, " ,.\t\n\r")) == NULL)
				{
					fprintf(stdout, "%s invalid cmd\n", Redir);
					break;
				}
				sc = (int) strtoul(tp, (char **) NULL, 10);
				if ((tp = strtok(NULL, " ,.\t\n\r")) == NULL)
				{
					fprintf(stdout, "%sinvalid cmd\n", Redir);
					break;
				}
				DumpHex(sc, (int) strtoul(tp, (char **) NULL, 10));
				break;
			case 'x': case 'X': case 'q': case 'Q':
				fprintf(stdout, "%s quit\n", Redir);
				if (Redir == (char *) RedirPrefix)
				{
					fprintf(stdout, "%s%s\n", Redir, Redir);
				}
				return;
			default:
				fprintf(stdout, "%s invalid cmd\n", Redir);
				break;
		}
		if (Redir == (char *) RedirPrefix)
		{
			fprintf(stdout, "%s%s\n", Redir, Redir);
		}
	}
}

int main(int argc, char **argv)
{
	int i;
	unsigned long t;

	setvbuf(stdout, (char *) NULL, _IOLBF, 0);
	if (argc != 2 && argc != 3)
	{
		fprintf(stderr, "syntax: %s input_file [-r]\n", argv[0]);
		return 1;
	}
	if (*argv[1] == '-')
	{
		fprintf(stderr, "syntax: %s input_file [-r]\n", argv[0]);
		return 1;
	}
	if ((FP = fopen(argv[1], "r")) == NULL)
	{
		fprintf(stderr, "%s: cannot open input file %s\n", argv[0], argv[1]);
		return 1;
	}
	if (argc == 3)
	{
		if (strcmp(argv[2], "-r") == 0)
		{
			Redir = (char *) RedirPrefix;
			fprintf(stdout, "%s\n", argv[0]);
		}
		else
		{
			fprintf(stderr, "syntax: %s input_file [-r]\n", argv[0]);
			return 1;
		}
	}for(int i=0;i<REGSIZE +4;i++){
		printf("%d, ", Reg[i] );
	}
	printf("\n");
	memset(Mem, 0, MEMSIZE * sizeof(unsigned));
	for (i = PCINIT; !feof(FP); i += 4)
	{
		if (fgets(Buf, BUFSIZE, FP) == NULL)
		{
			if (feof(FP))
				break;
			fprintf(stderr, "%s: file %s reading error\n", argv[0], argv[1]);
			return 1;
		}
		if (sscanf(Buf, "%lx", &t) != 1)
		{
			fprintf(stderr, "%s: file %s error in line %d, continue...\n",
				argv[0], argv[1], i - PCINIT + 1);
			MEM(i) = 0;
		}
		else
		{
			MEM(i) = strtoul(Buf, (char **) NULL, 16);
			unsigned n =  MEM(i);
			printf("instruction: %d\n", MEM(i));
			
			unsigned binaryInstruction[31];


 				for (int c = 31; c >= 0; c--)
  				{
   					 int k = n>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
 
				  printf("Binary instruction: ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }
				  printf("\n");
				  unsigned r1Mask = 65011712;

 				// for (int c = 31; c >= 0; c--)
  			// 	{
   		// 			 int k = r1Mask>> c;
 
    	// 			if (k & 1)
     //  					binaryInstruction[31-c] = 1;
    	// 			else
     //  				binaryInstruction[31-c] = 0;
  			// 	}
 
				 //  printf("Mask              : ");
				 //  for(int i=0;i<32;i++){
				 //  	printf("%d", binaryInstruction[i]);
				 //  }
				 //  printf("\n");



				  unsigned actualr1 = r1Mask & n;
				  // for (int c = 31; c >= 0; c--)
  				// {
   			// 		 int k = actualr1>> c;
 
    		// 		if (k & 1)
      // 					binaryInstruction[31-c] = 1;
    		// 		else
      // 				binaryInstruction[31-c] = 0;
  				// }
 
				  // printf("And               : ");
				  // for(int i=0;i<32;i++){
				  // 	printf("%d", binaryInstruction[i]);
				  // }
				  printf("\n");

				  actualr1 = actualr1 >> 21;
				   for (int c = 31; c >= 0; c--)
  				{
   					 int k = actualr1>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
 
				  
				printf("R1                : ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }
				  printf("\n");




				  unsigned r2Mask = 2031616;

				  //  for (int c = 31; c >= 0; c--)
  				// {
   			// 		 int k = r2Mask>> c;
 
    		// 		if (k & 1)
      // 					binaryInstruction[31-c] = 1;
    		// 		else
      // 				binaryInstruction[31-c] = 0;
  				// }
  				
  				//  printf("Mask              : ");
				  // for(int i=0;i<32;i++){
				  // 	printf("%d", binaryInstruction[i]);
				  // }
				  //printf("\n");



				  unsigned actualr2 = r2Mask & n;
  				 actualr2 = actualr2 >> 16;
				   for (int c = 31; c >= 0; c--)
  				{
   					 int k = actualr2>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
  				printf("R2                : ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }




				  /*

				  //OP
				  printf("\n op being");
				  for(int i=0;i<6;i++){
				  	printf("%d", binaryInstruction[i]);
				  }
				  printf("\n");
				  printf("op in decimal: ");
				  	unsigned opNum = 0;
					for(int i=0;i<6;i++){
						//printf("\n****%d****\n",binaryInstruction[5-i]);
						if(binaryInstruction[5-i] == 1){
							//printf("\nmeep\n");

							opNum += pow(2,i);
						}
					}
					printf("%d\n", opNum);


					//r1
					printf("\n r1 being");
				  for(int i=6;i<11;i++){
				  	printf("%d", binaryInstruction[i]);
				  }
				  printf("\n");
				  printf("r1 in decimal: ");
				  	unsigned r1Num = 0;
					for(int i=6;i<11;i++){
						//printf("\n****%d****\n",binaryInstruction[5-i]);
						if(binaryInstruction[10-i+6] == 1){
							//printf("\nmeep\n");

							r1Num += pow(2,i-6);
						}
					}
					printf("%d\n", r1Num);

					//r2
					printf("\n r2 being");
				  for(int i=10;i<15;i++){
				  	printf("%d", binaryInstruction[i]);
				  }
				  printf("\n");
				  printf("r2 in decimal: ");
				  	unsigned r2Num = 0;
					for(int i=10;i<15;i++){
						//printf("\n****%d****\n",binaryInstruction[5-i]);
						if(binaryInstruction[14-i+10] == 1){
							//printf("\nmeep\n");

							r2Num += pow(2,i-10);
						}
					}
					printf("%d\n", r2Num);

					*/
					printf("\n");


					unsigned r3Mask = 63488;

					unsigned actualr3 = r3Mask & n;
  				 	actualr3 = actualr3 >> 11;

				   for (int c = 31; c >= 0; c--)
  				{
   					 int k = actualr3>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
  				printf("R3                : ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }



				  printf("\n");


					unsigned functMask = 63;

					unsigned actualFunct = functMask & n;
  				 	

				   for (int c = 31; c >= 0; c--)
  				{
   					 int k = actualFunct>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
  				printf("FU                : ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }



				  printf("\n");
				  unsigned offsetMask = 65535;
				  unsigned actualOffset = offsetMask & n;

				  for (int c = 31; c >= 0; c--)
  				{
   					 int k = actualOffset>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
  				printf("Off               : ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }



				  printf("\n");
				  unsigned jsecMask = 67108863;
				  unsigned actualJsec = jsecMask & n;

				  for (int c = 31; c >= 0; c--)
  				{
   					 int k = actualJsec>> c;
 
    				if (k & 1)
      					binaryInstruction[31-c] = 1;
    				else
      				binaryInstruction[31-c] = 0;
  				}
  				printf("Jsec              : ");
				  for(int i=0;i<32;i++){
				  	printf("%d", binaryInstruction[i]);
				  }





					printf("\n");
					printf("op %i\n", n >>26 );
	// unsigned r1Mask = 65011712;
	 actualr1 = r1Mask & n;
	actualr1 = actualr1 >> 21;
	printf("r1 %i\n", actualr1);
	printf("r2 %i\n", actualr2);
	printf("r3 %i\n", actualr3);
	printf("funct %i\n\n", actualFunct);


	//unsigned r2Mask = 2031616;
	//unsigned actualr2 = r2Mask & n;
	actualr2 = actualr2 >> 16;

		}
	}
	Loop();
	fclose(FP);
	return 0;
}
