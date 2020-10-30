#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 255
#define MAX_LENGTH 20
#define true    1
#define false   0

struct OPC {
    char name[MAX_LENGTH];   // Mnemonic
    int op;              // opcode
    int length;              // opcode length
    char xe;                 // for sic/xe instruction
};

struct SYM {
    char name[MAX_LENGTH];
    int value;
    int base;
};

struct OPC OPTAB[] = 
{
  // opcode
  {"ADD",0x18,3,0},   {"ADDF",0x58,3,1},  {"ADDR",0x90,2,1},  {"AND",0x40,3,0},
  {"CLEAR",0xB4,2,1}, {"COMP",0x28,3,0},  {"COMPF",0x88,3,1}, {"COMPR",0xA0,2,1},
  {"DIV",0x24,3,0},   {"DIVF",0x64,3,1},  {"DIVR",0x9C,2,1},  {"FIX",0xC4,1,1},
  {"FLOAT",0xC0,1,1}, {"HIO",0xF4,1,1},   {"J",0x3C,3,0},     {"JEQ",0x30,3,0},
  {"JGT",0x34,3,0},   {"JLT",0x38,3,0},   {"JSUB",0x48,3,0},  {"LDA",0x00,3,0},
  {"LDB",0x68,3,1},   {"LDCH",0x50,3,0},  {"LDF",0x70,3,1},   {"LDL",0x08,3,0},
  {"LDS",0x6C,3,1},   {"LDT",0x74,3,1},   {"LDX",0x04,3,0},   {"LPS",0xD0,3,1},
  {"MUL",0x20,3,0},   {"MULF",0x60,3,1},  {"MULR",0x98,2,1},  {"NORM",0xC8,1,1},
  {"OR",0x44,3,0},    {"RD",0xD8,3,0},    {"RMO",0xAC,2,1},   {"RSUB",0x4C,3,0},
  {"SHIFTL",0xA4,2,1},{"SHIFTR",0xA8,2,1},{"SIO",0xF0,1,1},   {"SSK",0xEC,3,1},
  {"STA",0x0C,3,0},   {"STB",0x78,3,1},   {"STCH",0x54,3,0},  {"STF",0x80,3,1}, 
  {"STI",0xD4,3,1},   {"STL",0x14,3,0},   {"STS",0x7C,3,1},   {"STSW",0xE8,3,0},
  {"STT",0x84,3,1},   {"STX",0x10,3,0},   {"SUB",0x1C,3,0},   {"SUBF",0x5C,3,1},
  {"SUBR",0x94,2,1},  {"SVC",0xB0,2,1},   {"TD",0xE0,3,0},    {"TIO",0xF8,1,1}, 
  {"TIX",0x2C,3,0},   {"TIXR",0xB8,2,1},  {"WD",0xDC,3,0 },
  // directive
  {"BASE",0,0,0},     {"BYTE",1,0,0},     {"END",2,0,0}, 
  {"RESB",3,0,0},     {"RESW",4,0,0},     {"START",5,0,0},    {"WORD",6,0,0},
};

FILE *inputFile;
FILE *intermediate;
FILE *intermediate2;
FILE *listing;
FILE *objectFile;
struct SYM SYMTAB[50];
int symlen = 0;
int start_add = 0;
int loc = 0;
int program_length = 0;

int search(char temp[], int *tokens_length) {
    int len = sizeof(OPTAB) / sizeof(struct OPC);
    int format4 = 0;
    char temp_token[MAX_LENGTH];
    if (strncmp(temp, "+", strlen("+")) == 0) {
        int i = 0;
        format4 = 1;
        while(true) {
            temp_token[i] = temp[i + 1];
            if (temp_token[i] == '\0') {
                break;
            }
            i++;
        }
    }
    else {
        strcpy(temp_token, temp);
    }

    for (int i = 0; i < len; i++) {
        if (strcmp(temp_token, OPTAB[i].name) == 0) {
            if (format4 == 1) {
                *tokens_length = 4;
            } 
            else {
                *tokens_length = OPTAB[i].length;
            }
            return OPTAB[i].op;
        }
    }
    return -1;
}

int searchSYMTAB(char temp[])
{
    if (strncmp(temp, "+", strlen("+")) == 0) {
        return 1;
    }
    for (int i = 0; i < symlen; i++) {
        if (strcmp(temp, SYMTAB[i].name) == 0) {
            return SYMTAB[i].value;
        }
    }
    return -1;
}

int parse(char *line, int*nr_tokens, char *tokens[])
{
    char *curr = line;
    int token_started = false;
    *nr_tokens = 0;

    while (*curr != '\0') {
        if (isspace(*curr)) {
            *curr = '\0';
            token_started = false;
        } else {
            if (!token_started) {
                tokens[*nr_tokens] = curr;
                *nr_tokens += 1;
                token_started = true;
            }
        }
        curr++;
    }
    return (*nr_tokens > 0);
}

int parse_comma(char *tokens, int *num_operand, char *operand[])
{
    char *curr = tokens;
    int token_started = false;
    *num_operand = 0;

    while (*curr != '\0') {
        if (!isalnum(*curr)) {
            *curr = '\0';
            token_started = false;
        } else {
            if (!token_started) {
                operand[*num_operand] = curr;
                *num_operand += 1;
                token_started = true;
            }
        }
        curr++;
    }
    return (*num_operand > 0);
}

int digit_number(int op_length)
{
    int cnt = 0;
    while (true)
    {
        op_length /= 16;
        cnt++;
        if (!op_length) break;
    }
    return cnt;
}

int is_digit(char temp[])
{
    int i = 0;
    while (temp[i] != '\0') {
        if (!isdigit(temp[i])) {
            return 0;
            break;
        }
        i++;
    }
    return 1;
}

int pass1(char filename[])
{
    char *path;
    char line[MAX_LINE_LENGTH] = { '\0' };
    intermediate = fopen("intermediatefile.txt", "w");    // make intermediate file


    /* Print each line */
    int nr_tokens = 0;
    char *tokens[MAX_LINE_LENGTH] = { NULL };

    while(true) {
        int tokens_length = 0;
        fgets(line, MAX_LINE_LENGTH, inputFile);
        if (parse(line, &nr_tokens, tokens) == 0) {
            // only tab -> skip
        }
        else if (strncmp(tokens[0], ".", strlen(".")) == 0) {
            // comment -> skip
        }
        else {
            if (search(tokens[0], &tokens_length) == 5) {
                // start file without header name
                loc = atoi(tokens[1]);
                start_add = loc;
                fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, "\t", tokens[0], tokens[1]);
                break;
            } else if (search(tokens[1], &tokens_length) == 5) {
                loc = atoi(tokens[2]);
                start_add = loc;
                fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], tokens[2]);
                break;
            } else {
                loc = 0;
                start_add = loc;
                goto NEXT;
            }
        }
    }

    /* Get each line until there are none left */
    char line_temp[MAX_LINE_LENGTH];
    strcpy(line_temp, line);
    while (fgets(line, MAX_LINE_LENGTH, inputFile))
    {
NEXT:   
        strcpy(line_temp, line);
        if (parse(line, &nr_tokens, tokens) == 0) {
            // only tab -> skip
        }
        else if (strncmp(tokens[0], ".", strlen(".")) == 0) {
            // comment -> skip
        }
        else if (strncmp(tokens[0], "END", strlen("END")) == 0 && (strlen(tokens[0]) == strlen("END"))) {
            fprintf(intermediate, "                   %-10s %-10s %-10s  \n", tokens[0], tokens[1], "\t");
            break;
        }
        else {
            /* start line with label */
            int tokens_length = 0;
            if (search(tokens[0], &tokens_length) < 0) {
                if (searchSYMTAB(tokens[0]) > 0) {  // duplicate symbol
                    fprintf(intermediate, "ERROR: duplicate symbol - %s", line_temp);
                    continue;
                }
                else {                              // insert (label, loccer, base) into SYMTAB
                    strcpy(SYMTAB[symlen].name, tokens[0]);
                    SYMTAB[symlen].value = loc;
                    SYMTAB[symlen].base = 0;
                    symlen++;
                }
                int tokens_op = search(tokens[1], &tokens_length);
                if (tokens_op >= 0) {
                    if (nr_tokens == 2) {
                        tokens[2] = "\t";
                    }
                    if (tokens_length == 0) {
                        if (strncmp(tokens[1], "BASE", strlen("BASE")) == 0) {
                            fprintf(intermediate, "                   %-10s %-10s %-10s  \n", tokens[0], tokens[1], tokens[2]);
                        }
                        else {
                            fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], tokens[2]);
                            if (strncmp(tokens[1], "WORD", strlen("WORD")) == 0) {
                                loc += 3;
                            }
                            else if (strncmp(tokens[1], "RESW", strlen("RESW")) == 0) {
                                loc += atoi(tokens[2]) * 3;
                            }
                            else if (strncmp(tokens[1], "RESB", strlen("RESB")) == 0) {
                                loc += atoi(tokens[2]);
                            }
                            else if (strncmp(tokens[1], "BYTE", strlen("BYTE")) == 0) {
                                if (strncmp(tokens[2], "C", strlen("C")) == 0) {
                                    loc += strlen(tokens[2]) - 3;
                                }
                                else if (strncmp(tokens[2], "X", strlen("X")) == 0) {
                                    int temp_x = strlen(tokens[2]) - 3;
                                    if (temp_x & 0x01) {    // odd
                                        loc += (temp_x + 1) / 2;
                                    }
                                    else {                  // even
                                        loc += temp_x / 2;
                                    }
                                }
                            }
                        }
                    }
                    else if (tokens_length == 1) {
                        fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], tokens[2]);
                        loc += 1;
                    }
                    else if (tokens_length == 2) {
                        fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], tokens[2]);
                        loc += 2;
                    }
                    else if (tokens_length == 3) {
                        if (strncmp(tokens[1], "RSUB", strlen("RSUB")) == 0) {
                            fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], "\t");
                        }
                        else {
                            fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], tokens[2]);
                        }
                        loc += 3;
                    }
                    else if (tokens_length == 4) {
                        if (strncmp(tokens[1], "+RSUB", strlen("+RSUB")) == 0) {
                            fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], "\t");
                        }
                        else {
                            fprintf(intermediate, "%04X    %-10s %-10s %-10s  \n", loc, tokens[0], tokens[1], tokens[2]);
                        }
                        loc += 4;
                    }
                }
                else {
                    fprintf(intermediate, "ERROR: invalid operation code - %s", line_temp);
                    continue;
                }
            }
            /* start line without label */ 
            else {
                int tokens_op = search(tokens[0], &tokens_length);
                if (tokens_op >= 0) {
                    if (nr_tokens == 1) {
                        tokens[1] = "\t";
                    }
                    if (tokens_length == 0) {
                        if (strncmp(tokens[0], "BASE", strlen("BASE")) == 0) {
                            fprintf(intermediate, "   	    	       %-10s %-10s  \n", tokens[0], tokens[1]);
                        }
                        else {
                            fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], tokens[1]);
                            if (strncmp(tokens[0], "WORD", strlen("WORD")) == 0) {
                                loc += 3;
                            }
                            else if (strncmp(tokens[0], "RESW", strlen("RESW")) == 0) {
                                loc += atoi(tokens[1]) * 3;
                            }
                            else if (strncmp(tokens[0], "RESB", strlen("RESB")) == 0) {
                                loc += atoi(tokens[1]);
                            }
                            else if (strncmp(tokens[0], "BYTE", strlen("BYTE")) == 0) {
                                if (strncmp(tokens[1], "C", strlen("C")) == 0) {
                                    loc += strlen(tokens[1]) - 3;
                                }
                                else if (strncmp(tokens[1], "X", strlen("X")) == 0) {
                                    int temp_x = strlen(tokens[1]) - 3;
                                    if (temp_x & 0x01) {    // odd
                                        loc += (temp_x + 1) / 2;
                                    }
                                    else {                  // even
                                        loc += temp_x / 2;
                                    }
                                }
                            }
                        }
                    }
                    else if (tokens_length == 1) {
                        fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], tokens[1]);
                        loc += 1;
                    }
                    else if (tokens_length == 2) {
                        fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], tokens[1]);
                        loc += 2;
                    }
                    else if (tokens_length == 3) {
                        if (strncmp(tokens[0], "RSUB", strlen("RSUB")) == 0) {
                            fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], "\t");
                        }
                        else {
                            fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], tokens[1]);
                        }
                        loc += 3;
                    }
                    else if (tokens_length == 4) {
                        if (strncmp(tokens[0], "+RSUB", strlen("+RSUB")) == 0) {
                            fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], "\t");
                        }
                        else {
                            fprintf(intermediate, "%04X    	       %-10s %-10s  \n", loc, tokens[0], tokens[1]);
                        }
                        loc += 4;
                    }
                }
                else {
                    fprintf(intermediate, "ERROR: invalid operation code - %s", line_temp);
                    continue;
                }
            }
        }
    }
}

int pass2(FILE *intermediate, char filename[])
{
    int opcode = 0;
    int memctr = 0;

    intermediate2 = fopen("intermediatefile.txt", "r");  // open imm
    listing = fopen("assembly_listing_file.txt", "w");
    objectFile = fopen(filename, "w");

    fprintf(listing, "Loc         Source Code                 Object Code\n");

    int nr_tokens = 0;
    char *tokens[MAX_LINE_LENGTH] = { NULL };
    char line[MAX_LINE_LENGTH] = { '\0' };

    int tokens_length = 0;
    fgets(line, MAX_LINE_LENGTH, intermediate2);
    parse(line, &nr_tokens, tokens);

    if (strncmp(tokens[1], "START", strlen("START")) == 0) {
        // start file without header name
        fprintf(listing, "%04X    %-10s %-10s %-10s  \n", atoi(tokens[0]), "\t", tokens[1], tokens[2]);
        fprintf(objectFile, "H%-6s%6d%6d\n", "\0", start_add, loc);
    } 
    else if (strncmp(tokens[2], "START", strlen("START")) == 0) {
        fprintf(listing, "%04X    %-10s %-10s %-10s  \n", atoi(tokens[0]), tokens[1], tokens[2], tokens[3]);
        fprintf(objectFile, "H%-6s%06X%06X\n",tokens[1], start_add, loc);
    }

    int text_record = 0;
    char text[70] = { 0 };
    int locctr = start_add;
    int cp = 0;
    int pc = start_add;
    int b_register = 0;
    char modify[MAX_LINE_LENGTH] = { '\0' };

    while(fgets(line, MAX_LINE_LENGTH, intermediate2))
    {
        char line_temp[MAX_LINE_LENGTH];
        strcpy(line_temp, line);
        parse(line, &nr_tokens, tokens);
        if (strncmp(tokens[0], "END", strlen("END")) == 0 && (strlen(tokens[0]) == strlen("END"))) {
            fprintf(listing, "                   %-10s %-10s %-10s  \n", tokens[0], tokens[1], "\0");
            if (text_record != 0) {
                fprintf(objectFile, "%02X%s\n", text_record/2, text);
            }
            fprintf(objectFile, "%s", modify);
            fprintf(objectFile, "E%06X\n", start_add);
            break;
        }
        else if (strncmp(tokens[0], "ERROR:", strlen("ERROR:")) == 0) {
            fprintf(listing, "%s", line); 
        }
        else {
            int tokens_length = 0;
            if ((strncmp(tokens[0], "BASE", strlen("BASE")) == 0) && strlen(tokens[0]) == strlen("BASE")) {
                fprintf(listing, "                   %-10s %-10s\n", tokens[0], tokens[1]);
            }
            else {
                if (text_record == 0 && (strcmp(tokens[2], "RESW") != 0) && (strcmp(tokens[2], "RESB") != 0)) {
                    locctr = (int)strtol(tokens[0], NULL, 16);
                    fprintf(objectFile, "T%06X", locctr);
                }
                else if (cp == 1) {
                    fprintf(objectFile, "T%06X", locctr);
                    cp = 0;
                    locctr = (int)strtol(tokens[0], NULL, 16);
                } 
                else {
                    locctr = (int)strtol(tokens[0], NULL, 16);
                }
            }
            int op_length = search(tokens[1], &tokens_length);
            int num_operand = 0;
            char *operand[2] = { NULL };
            if (op_length >= 0) {
                // no symbol, start with opcode
                if (tokens_length == 1) {
                    fprintf(listing, "%04X    %-10s %-10s %-10s %02X\n", locctr, "\0", tokens[1], "\0", op_length);
                    pc += 1;
                }
                else if (tokens_length == 2) {
                    pc += 2;
                    parse_comma(tokens[2], &num_operand, operand);
                    if (strcmp(operand[0], "A") == 0 && (strlen(operand[0]) == strlen("A"))) {
                        op_length = op_length << 8;
                    } else if (strcmp(operand[0], "X") == 0 && (strlen(operand[0]) == strlen("X"))) {
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "L") == 0 && (strlen(operand[0]) == strlen("L"))) {
                        op_length = op_length << 4;
                        op_length += 2;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "B") == 0 && (strlen(operand[0]) == strlen("B"))) {
                        op_length = op_length << 4;
                        op_length += 3;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "S") == 0 && (strlen(operand[0]) == strlen("S"))) {
                        op_length = op_length << 4;
                        op_length += 4;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "T") == 0 && (strlen(operand[0]) == strlen("T"))) {
                        op_length = op_length << 4;
                        op_length += 5;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "F") == 0 && (strlen(operand[0]) == strlen("F"))) {
                        op_length = op_length << 4;
                        op_length += 6;
                        op_length = op_length << 4;
                    } else {
                        op_length = op_length << 4;
                        op_length += (int)strtol(operand[0], NULL, 16);
                        op_length = op_length << 4;
                    }

                    if (num_operand == 2) {
                        if ((strcmp(operand[1], "A") == 0) && (strlen(operand[1]) == strlen("A"))) {
                            op_length += 0;
                        } else if (strcmp(operand[1], "X") == 0 && (strlen(operand[1]) == strlen("X"))) {
                            op_length += 1;
                        } else if (strcmp(operand[1], "L") == 0 && (strlen(operand[1]) == strlen("L"))) {
                            op_length += 2;
                        } else if (strcmp(operand[1], "B") == 0 && (strlen(operand[1]) == strlen("B"))) {
                            op_length += 3;
                        } else if (strcmp(operand[1], "S") == 0 && (strlen(operand[1]) == strlen("S"))) {
                            op_length += 4;
                        } else if (strcmp(operand[1], "T") == 0 && (strlen(operand[1]) == strlen("T"))) {
                            op_length += 5;
                        } else if (strcmp(operand[1], "F") == 0 && (strlen(operand[1]) == strlen("F"))) {
                            op_length += 6;
                        } else {
                            op_length += (int)strtol(operand[1], NULL, 16);
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-s,%-8s %04X\n", locctr, "\0", tokens[1], operand[0], operand[1], op_length);
                    }
                    if (num_operand == 1) {
                        fprintf(listing, "%04X    %-10s %-10s %-10s %04X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                    }
                }
                else if (tokens_length == 3) {
                    pc += 3;
                    int i = 0;
                    char temp_token[MAX_LENGTH];
                    strcpy(temp_token, tokens[2]);
                    parse_comma(temp_token, &num_operand, operand);
                    if (strncmp(tokens[1], "RSUB", strlen("RSUB")) == 0) {
                        op_length += 3;
                        op_length = op_length << 16;
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], "\0", op_length);
                    }
                    else if (strncmp(tokens[2], "#", strlen("#")) == 0) {
                        // immediate addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[2][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[2][i+1];
                        }
                        
                        if (is_digit(temp)) {
                            int k = 0;
                            for (int j = 0; j < i; j++) {
                                k *= 10;
                                k = k + (temp[j] - '0');
                            }
                            if (strcmp(tokens[1], "LDB") == 0) {
                                b_register = k;
                            }
                            op_length += 1;
                            op_length = op_length << 4;
                            op_length = op_length << 12;
                            op_length += k;
                            fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                        }
                        else {
                            int symvalue = searchSYMTAB(temp);
                            if (symvalue < 0) {
                                fprintf(listing, "Error: undefined symbol -%s", line_temp);
                                continue;
                            }
                            int check_range = 0;
                            if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                                check_range = 1;
                            }
                            else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                                check_range = 1;
                            }
                            if (check_range == 1) {
                                // pc relative
                                op_length += 1;
                                op_length = op_length << 4;
                                op_length += 2;
                                op_length = op_length << 12;
                                if ((symvalue - pc) < 0) {
                                    op_length += ((symvalue - pc) & 0xFFF);
                                }
                                else {
                                    op_length += (symvalue - pc) & 0xFFF;
                                }
                            }
                            else if ((symvalue >= b_register) && (symvalue - b_register) <= 4095)
                            {
                                op_length += 1;
                                op_length = op_length << 4;
                                op_length += 4;
                                op_length = op_length << 12;
                                if ((symvalue - b_register) < 0) {
                                    op_length += ((symvalue - b_register) & 0xFFF);
                                }
                                else {
                                    op_length += (symvalue - b_register) & 0xFFF;
                                }
                            }
                            else {
                                fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                                continue;
                            }
                            if (strcmp(tokens[1], "LDB") == 0) {
                                b_register = symvalue;
                            }
                            fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                        }
                    }
                    else if (strncmp(tokens[2], "@", strlen("@")) == 0) {
                        // indirect addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[2][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[2][i+1];
                        }
                        int symvalue = searchSYMTAB(temp);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        int check_range = 0;
                        if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                            check_range = 1;
                        }
                        else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                            check_range = 1;
                        }
                        if (check_range == 1) {
                            // pc relative
                            op_length += 2;
                            op_length = op_length << 4;
                            op_length += 2;
                            op_length = op_length << 12;
                            if ((symvalue - pc) < 0) {
                                op_length += ((symvalue - pc) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - pc) & 0xFFF;
                            }
                        }
                        else if ((symvalue >= b_register)&& (symvalue - b_register) <= 4095)
                        {
                            op_length += 2;
                            op_length = op_length << 4;
                            op_length += 4;
                            op_length = op_length << 12;
                            if ((symvalue - b_register) < 0) {
                                op_length += ((symvalue - b_register) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - b_register) & 0xFFF;
                            }
                        }
                        else {
                            fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                            continue;
                        }
                        if (strcmp(tokens[1], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                    }
                    else if (num_operand == 2) {
                        int symvalue = searchSYMTAB(operand[0]);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        int check_range = 0;
                        if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                            check_range = 1;
                        }
                        else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                            check_range = 1;
                        }
                        if (check_range == 1) {
                            // pc relative
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 10;
                            op_length = op_length << 12;
                            if ((symvalue - pc) < 0) {
                                op_length += ((symvalue - pc) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - pc) & 0xFFF;
                            }
                        }
                        else if ((symvalue >= b_register ) && (symvalue - b_register) <= 4095)
                        {
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 12;
                            op_length = op_length << 12;
                            if ((symvalue - b_register) < 0) {
                                op_length += ((symvalue - b_register) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - b_register) & 0xFFF;
                            }
                        }
                        else {
                            fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                            continue;
                        }
                        if (strcmp(tokens[1], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                        // index addressing

                    }
                    else {
                        int symvalue = searchSYMTAB(tokens[2]);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        int check_range = 0;
                        if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                            check_range = 1;
                        }
                        else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                            check_range = 1;
                        }
                        if (check_range == 1) {
                            // pc relative
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 2;
                            op_length = op_length << 12;
                            if ((symvalue - pc) < 0) {
                                op_length += ((symvalue - pc) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - pc) & 0xFFF;
                            }
                        }
                        else if ((symvalue >= b_register) && (symvalue - b_register) <= 4095)
                        {
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 4;
                            op_length = op_length << 12;
                            if ((symvalue - b_register) < 0) {
                                op_length += ((symvalue - b_register) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - b_register) & 0xFFF;
                            }
                        }
                        else {
                            fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                            continue;
                        }
                        if (strcmp(tokens[1], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                    }
                }
                else if (tokens_length == 4) {
                    pc += 4;
                    int i = 0;
                    if (strncmp(tokens[1], "+RSUB", strlen("+RSUB")) == 0) {
                        op_length += 3;
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 20;
                        fprintf(listing, "%04X    %-10s %-10s %-10s %08X\n", locctr, "\0", tokens[1], "\0", op_length);
                    }
                    else if (strncmp(tokens[2], "#", strlen("#")) == 0) {
                        // immediate addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[2][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[2][i+1];
                        }
                        
                        if (is_digit(temp)) {
                            int k = 0;
                            for (int j = 0; j < i; j++) {
                                k *= 10;
                                k = k + (temp[j] - '0');
                            }
                            if (strcmp(tokens[1], "LDB") == 0) {
                                b_register = k;
                            }
                            op_length += 1;
                            op_length = op_length << 4;
                            op_length += 1;
                            op_length = op_length << 20;
                            op_length += k;
                            fprintf(listing, "%04X    %-10s %-10s %-10s %08X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                        }
                        else {
                            int symvalue = searchSYMTAB(temp);
                            if (symvalue < 0) {
                                fprintf(listing, "Error: undefined symbol -%s", line_temp);
                                continue;
                            }
                            if (strcmp(tokens[1], "LDB") == 0) {
                                b_register = symvalue;
                            }
                            op_length += 1;
                            op_length = op_length << 4;
                            op_length += 1;
                            op_length = op_length << 20;
                            op_length += symvalue;
                            fprintf(listing, "%04X    %-10s %-10s %-10s %08X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                        }
                    }
                    else if (strncmp(tokens[2], "@", strlen("@")) == 0) {
                        // indirect addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[2][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[2][i+1];
                        }
                        int symvalue = searchSYMTAB(temp);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        op_length += 2;
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 20;
                        op_length += symvalue;
                        if (strcmp(tokens[1], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);
                        int modify_loc = locctr + 1;
                        char modi_char[11];
                        modi_char[0] = 'M';
                        for (i = 0; ; i++) {
                            if (modify_loc == 0)
                                break;
                            if ((modify_loc % 16) > 9) {
                                modi_char[i + 1] = (modify_loc % 16) - 10 + 'A';
                            }
                            else {
                                modi_char[i + 1] = (modify_loc % 16) + '0';
                            }
                            modify_loc /= 16;
                        }
                        for (i = i + 1; i < 7; i++) {
                            modi_char[i] = '0';
                        }
                        for (i = 0; i < 3; i++) {
                            char change = modi_char[i + 1];
                            modi_char[i + 1] = modi_char[6 -i];
                            modi_char[6 -i] = change;
                        }
                        modi_char[7] = '0';
                        modi_char[8] = '5';
                        modi_char[9] = '\n';
                        modi_char[10] = '\0';
                        strcat(modify, modi_char);
                    }
                    else {
                        int symvalue = searchSYMTAB(tokens[2]);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        op_length += 3;
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 20;
                        op_length += symvalue;
                        if (strcmp(tokens[1], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, "\0", tokens[1], tokens[2], op_length);

                        int modify_loc = locctr + 1;
                        char modi_char[11];
                        modi_char[0] = 'M';
                        for (i = 0; ; i++) {
                            if (modify_loc == 0)
                                break;
                            if ((modify_loc % 16) > 9) {
                                modi_char[i + 1] = (modify_loc % 16) - 10 + 'A';
                            }
                            else {
                                modi_char[i + 1] = (modify_loc % 16) + '0';
                            }
                            modify_loc /= 16;
                        }
                        for (i = i + 1; i < 7; i++) {
                            modi_char[i] = '0';
                        }
                        for (i = 0; i < 3; i++) {
                            char change = modi_char[i + 1];
                            modi_char[i + 1] = modi_char[6 -i];
                            modi_char[6 -i] = change;
                        }
                        modi_char[7] = '0';
                        modi_char[8] = '5';
                        modi_char[9] = '\n';
                        modi_char[10] = '\0';
                        strcat(modify, modi_char);
                    }
                }
            }
            else {
                // start with symbol
                op_length = search(tokens[2], &tokens_length);
                if (tokens_length == 0) {
                    if ((strncmp(tokens[2], "RESW", strlen("RESW")) == 0 ||
                        strncmp(tokens[2], "RESB", strlen("RESB")) == 0) &&
                        (strlen(tokens[2]) == strlen("RESW"))) {
                        fprintf(listing, "%04X    %-10s %-10s %-10s\n", locctr, tokens[1], tokens[2], tokens[3]);
                        if (text_record != 0) {
                            fprintf(objectFile, "%02X%s\n", text_record/2, text);
                            text_record = 0;
                            for (int i = 0; i < 70; i++) {
                                text[i] = '\0';
                            }
                        }
                        if (strncmp(tokens[2], "RESB", strlen("RESB")) == 0) {
                            pc += atoi(tokens[3]);
                        }
                        else {
                            pc += (atoi(tokens[3]) * 3);
                        }
                    } 
                    else if ((strncmp (tokens[2], "WORD", strlen("WORD")) == 0) && (strlen(tokens[2]) == strlen("WORD"))) {
                        int temp_opnum = (int)strtol(tokens[3], NULL, 16);
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], temp_opnum);
                        text_record += 6;
                        char temp_opcode[7];
                        int i = 0;
                        for (i = 0; i < digit_number(temp_opnum); i++) {
                            if ((temp_opnum%16) > 9) {
                                temp_opcode[i] = (temp_opnum % 16) + 'A' - 10;
                            }
                            else {
                                temp_opcode[i] = (temp_opnum % 16) + '0';
                            }
                            op_length /= 16;
                        }
                        for (; i < 6; i++) {
                            temp_opcode[i] = '0';
                        }
                        for (i = 0; i < 6; i++) {
                            char change = temp_opcode[i];
                            temp_opcode[i] = temp_opcode[5-i];
                            temp_opcode[5-i] = change;
                        }
                        temp_opcode[6] = '\0';

                        if (text_record <= 60) {
                            strcat(text, temp_opcode);
                            if (text_record == 60) {
                                fprintf(objectFile, "%02X%s\n", 30, text);
                                text_record = 0;
                                for (i = 0; i < 70; i++) {
                                    text[i] = '\0';
                                }
                            }
                        }
                        else if (text_record > 60) {
                            text_record -= 6;
                            fprintf(objectFile, "%02X%s\n", text_record/2, text);
                            text_record = 0;
                            text_record += 6;
                            for (i = 0; i < 70; i++) {
                                text[i] = '\0';
                            }
                            strcat(text, temp_opcode);
                            cp = 1;
                        }
                        pc += 3;
                    } 
                    else if ((strncmp (tokens[2], "BYTE", strlen("BYTE")) == 0) && (strlen(tokens[2]) == strlen("BYTE"))) {   
                        char temp_char[MAX_LINE_LENGTH];
                        int i = 0;
                        for (i = 2; ; i++) {
                            if (tokens[3][i] == '\'') {
                                temp_char[i-2] = '\0';
                                i = i - 2;
                                break;
                            }
                            temp_char[i-2] = tokens[3][i];
                        }
                        if (strncmp(tokens[3], "X", strlen("X")) == 0) {
                            fprintf(listing, "%04X    %-10s %-10s %-10s %-s\n", locctr, tokens[1], tokens[2], tokens[3], temp_char);
                            text_record += i;
                            pc += (i / 2);
                            if (text_record <= 60) {
                                strcat(text, temp_char);
                                if (text_record == 60) {
                                    fprintf(objectFile, "%02X%s\n", 30, text);
                                    text_record = 0;
                                    for (i = 0; i < 70; i++) {
                                        text[i] = '\0';
                                    }
                                }
                            }
                            else if (text_record > 60) {
                                text_record -= i;
                                fprintf(objectFile, "%02X%s\n", text_record/2, text);
                                text_record = 0;
                                text_record += i;
                                for (i = 0; i < 70; i++) {
                                    text[i] = '\0';
                                }
                                strcat(text, temp_char);
                                cp = 1;
                            }
                        } 
                        else if (strncmp(tokens[3], "C", strlen("C")) == 0) {
                            long long int temp = 0;
                            for (int j = 0; j < i; j++) {
                                temp = temp << 8;
                                temp += temp_char[j];
                            }
                            fprintf(listing, "%04X    %-10s %-10s %-10s %-llX\n", locctr, tokens[1], tokens[2], tokens[3], temp);
                            text_record += digit_number(temp);
                            pc += digit_number(temp) / 2;
                            for (i = 0; ; i++) {
                                if (temp == 0) break;
                                if ((temp % 16) > 9) {
                                    temp_char[i] = temp % 16 + 'A' - 10;
                                } 
                                else {
                                    temp_char[i] = temp % 16 + '0';
                                }
                                temp /= 16;
                            }
                            for (int j = 0; j < (i/2); j++) {
                                char change = temp_char[j];
                                temp_char[j] = temp_char[i - 1 - j];
                                temp_char[i - 1 - j] = change;
                            }
                            temp_char[i] = '\0';
                            if (text_record <= 60) {
                                strcat(text, temp_char);
                                if (text_record == 60) {
                                    fprintf(objectFile, "%02X%s\n", 30, text);
                                    text_record = 0;
                                    for (i = 0; i < 70; i++) {
                                        text[i] = '\0';
                                    }
                                }
                            }
                            else if (text_record > 60) {
                                text_record -= digit_number(temp);
                                fprintf(objectFile, "%02X%s\n", text_record/2, text);
                                text_record = 0;
                                text_record += digit_number(temp);
                                for (i = 0; i < 70; i++) {
                                    text[i] = '\0';
                                }
                                strcat(text, temp_char);
                                cp = 1;
                            }
                        }
                    }
                    continue;
                }
                else if (tokens_length == 1) {
                    pc += 1;
                    fprintf(listing, "%04X    %-10s %-10s %-10s %02X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                }
                else if (tokens_length == 2) {
                    pc += 2;
                    parse_comma(tokens[3], &num_operand, operand);
                    if (strcmp(operand[0], "A") == 0 && (strlen(operand[0]) == strlen("A"))) {
                        op_length = op_length << 8;
                    } else if (strcmp(operand[0], "X") == 0 && (strlen(operand[0]) == strlen("X"))) {
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "L") == 0 && (strlen(operand[0]) == strlen("L"))) {
                        op_length = op_length << 4;
                        op_length += 2;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "B") == 0 && (strlen(operand[0]) == strlen("B"))) {
                        op_length = op_length << 4;
                        op_length += 3;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "S") == 0 && (strlen(operand[0]) == strlen("S"))) {
                        op_length = op_length << 4;
                        op_length += 4;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "T") == 0 && (strlen(operand[0]) == strlen("T"))) {
                        op_length = op_length << 4;
                        op_length += 5;
                        op_length = op_length << 4;
                    } else if (strcmp(operand[0], "F") == 0 && (strlen(operand[0]) == strlen("F"))) {
                        op_length = op_length << 4;
                        op_length += 6;
                        op_length = op_length << 4;
                    } else {
                        op_length = op_length << 4;
                        op_length += (int)strtol(operand[0], NULL, 16);
                        op_length = op_length << 4;
                    }

                    if (num_operand == 2) {
                        if ((strcmp(operand[1], "A") == 0) && (strlen(operand[1]) == strlen("A"))) {
                            op_length += 0;
                        } else if (strcmp(operand[1], "X") == 0 && (strlen(operand[1]) == strlen("X"))) {
                            op_length += 1;
                        } else if (strcmp(operand[1], "L") == 0 && (strlen(operand[1]) == strlen("L"))) {
                            op_length += 2;
                        } else if (strcmp(operand[1], "B") == 0 && (strlen(operand[1]) == strlen("B"))) {
                            op_length += 3;
                        } else if (strcmp(operand[1], "S") == 0 && (strlen(operand[1]) == strlen("S"))) {
                            op_length += 4;
                        } else if (strcmp(operand[1], "T") == 0 && (strlen(operand[1]) == strlen("T"))) {
                            op_length += 5;
                        } else if (strcmp(operand[1], "F") == 0 && (strlen(operand[1]) == strlen("F"))) {
                            op_length += 6;
                        } else {
                            op_length += (int)strtol(operand[1], NULL, 16);;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-s,%-8s %04X\n", locctr, tokens[1], tokens[2], operand[0], operand[1], op_length);
                    }
                    if (num_operand == 1) {
                        fprintf(listing, "%04X    %-10s %-10s %-10s %04X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                    }
                }
                else if (tokens_length == 3) {
                    pc += 3;
                    int i = 0;
                    char temp_token[MAX_LENGTH];
                    strcpy(temp_token, tokens[3]);
                    parse_comma(temp_token, &num_operand, operand);
                    if (strncmp(tokens[2], "RSUB", strlen("RSUB")) == 0) {
                        op_length += 3;
                        op_length = op_length << 16;
                        fprintf(listing, "%04X    %-10s %-10s %-10s %-X\n", locctr, tokens[1], tokens[2], "\0", op_length);
                    }
                    else if (strcmp(tokens[3], "#") == 0) {
                        // immediate addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[3][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[3][i+1];
                        }
                        
                        if (is_digit(temp)) {
                            int k = 0;
                            for (int j = 0; j < i; j++) {
                                k *= 10;
                                k = k + (temp[j] - '0');
                            }
                            if (strcmp(tokens[2], "LDB") == 0) {
                                b_register = k;
                            }
                            op_length += 1;
                            op_length = op_length << 4;
                            op_length = op_length << 12;
                            op_length += k;
                            fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                        }
                        else {
                            int symvalue = searchSYMTAB(temp);
                            if (symvalue < 0) {
                                fprintf(listing, "Error: undefined symbol -%s", line_temp);
                                continue;
                            }
                            int check_range = 0;
                            if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                                check_range = 1;
                            }
                            else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                                check_range = 1;
                            }
                            if (check_range == 1) {
                                // pc relative
                                op_length += 1;
                                op_length = op_length << 4;
                                op_length += 2;
                                op_length = op_length << 12;
                                if ((symvalue - pc) < 0) {
                                    op_length += ((symvalue - pc) & 0xFFF);
                                }
                                else {
                                    op_length += (symvalue - pc) & 0xFFF;
                                }
                            }
                            else if ((symvalue >= b_register) && (symvalue - b_register) <= 4095)
                            {
                                op_length += 1;
                                op_length = op_length << 4;
                                op_length += 4;
                                op_length = op_length << 12;
                                if ((symvalue - b_register) < 0) {
                                    op_length += ((symvalue - b_register) & 0xFFF);
                                }
                                else {
                                    op_length += (symvalue - b_register) & 0xFFF;
                                }
                            }
                            else {
                                fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                                continue;
                            }
                            if (strcmp(tokens[2], "LDB") == 0) {
                                b_register = symvalue;
                            }
                            fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                        }
                    }
                    else if (strcmp(tokens[3], "@") == 0) {
                        // indirect addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[3][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[3][i+1];
                        }
                        int symvalue = searchSYMTAB(temp);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        int check_range = 0;
                        if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                            check_range = 1;
                        }
                        else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                            check_range = 1;
                        }
                        if (check_range == 1) {
                            // pc relative
                            op_length += 2;
                            op_length = op_length << 4;
                            op_length += 2;
                            op_length = op_length << 12;
                            if ((symvalue - pc) < 0) {
                                op_length += ((symvalue - pc) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - pc) & 0xFFF;
                            }
                        }
                        else if ((symvalue >= b_register)&& (symvalue - b_register) <= 4095)
                        {
                            op_length += 2;
                            op_length = op_length << 4;
                            op_length += 4;
                            op_length = op_length << 12;
                            if ((symvalue - b_register) < 0) {
                                op_length += ((symvalue - b_register) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - b_register) & 0xFFF;
                            }
                        }
                        else {
                            fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                            continue;
                        }
                        if (strcmp(tokens[2], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                    }
                    else if (num_operand == 2) {
                        // index addressing
                        int symvalue = searchSYMTAB(operand[0]);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        int check_range = 0;
                        if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                            check_range = 1;
                        }
                        else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                            check_range = 1;
                        }
                        if (check_range == 1) {
                            // pc relative
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 10;
                            op_length = op_length << 12;
                            if ((symvalue - pc) < 0) {
                                op_length += ((symvalue - pc) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - pc) & 0xFFF;
                            }
                        }
                        else if ((symvalue >= b_register ) && (symvalue - b_register) <= 4095)
                        {
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 12;
                            op_length = op_length << 12;
                            if ((symvalue - b_register) < 0) {
                                op_length += ((symvalue - b_register) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - b_register) & 0xFFF;
                            }
                        }
                        else {
                            fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                            continue;
                        }
                        if (strcmp(tokens[2], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);

                    }
                    else {
                        int symvalue = searchSYMTAB(tokens[3]);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        int check_range = 0;
                        if ((symvalue > pc) && (symvalue - pc) <= 2047) {
                            check_range = 1;
                        }
                        else if ((symvalue <= pc) && (pc - symvalue) <= 2048) {
                            check_range = 1;
                        }
                        if (check_range == 1) {
                            // pc relative
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 2;
                            op_length = op_length << 12;
                            if ((symvalue - pc) < 0) {
                                op_length += ((symvalue - pc) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - pc) & 0xFFF;
                            }
                        }
                        else if ((symvalue >= b_register) && (symvalue - b_register) <= 4095)
                        {
                            op_length += 3;
                            op_length = op_length << 4;
                            op_length += 4;
                            op_length = op_length << 12;
                            if ((symvalue - b_register) < 0) {
                                op_length += ((symvalue - b_register) & 0xFFF);
                            }
                            else {
                                op_length += (symvalue - b_register) & 0xFFF;
                            }
                        }
                        else {
                            fprintf(listing, "Error: displacement is invalid in both relative modes -%s", line_temp);
                            continue;
                        }
                        if (strcmp(tokens[2], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                    }
                }
                else if (tokens_length == 4) {
                    pc += 4;
                    int i = 0;
                    if (strncmp(tokens[2], "+RSUB", strlen("+RSUB")) == 0) {
                        op_length += 3;
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 20;
                        fprintf(listing, "%04X    %-10s %-10s %-10s %-X\n", locctr, "\0", tokens[1], "\0", op_length);
                    }
                    else if (strcmp(tokens[3], "#") == 0) {
                        // immediate addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[3][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[3][i+1];
                        }
                        
                        if (is_digit(temp)) {
                            int k = 0;
                            for (int j = 0; j < i; j++) {
                                k *= 10;
                                k = k + (temp[j] - '0');
                            }
                            if (strcmp(tokens[2], "LDB") == 0) {
                                b_register = k;
                            }
                            op_length += 1;
                            op_length = op_length << 4;
                            op_length += 1;
                            op_length = op_length << 20;
                            op_length += k;
                            fprintf(listing, "%04X    %-10s %-10s %-10s %08X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                        }
                        else {
                            int symvalue = searchSYMTAB(temp);
                            if (symvalue < 0) {
                                fprintf(listing, "Error: undefined symbol -%s", line_temp);
                                continue;
                            }
                            if (strcmp(tokens[2], "LDB") == 0) {
                                b_register = symvalue;
                            }
                            op_length += 1;
                            op_length = op_length << 4;
                            op_length += 1;
                            op_length = op_length << 20;
                            op_length += symvalue;
                            fprintf(listing, "%04X    %-10s %-10s %-10s %08X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                        }
                    }
                    else if (strcmp(tokens[3], "@") == 0) {
                        // indirect addressing
                        char temp[MAX_LENGTH] = { '\0' };
                        for (i = 0; ; i++) {
                            if (tokens[3][i+1] == '\0') {
                                temp[i] = '\0';
                                break;
                            }
                            temp[i] = tokens[3][i+1];
                        }
                        int symvalue = searchSYMTAB(temp);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        op_length += 2;
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 20;
                        op_length += symvalue;
                        if (strcmp(tokens[2], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                        int modify_loc = locctr + 1;
                        char modi_char[11];
                        modi_char[0] = 'M';
                        for (i = 0; ; i++) {
                            if (modify_loc == 0)
                                break;
                            if ((modify_loc % 16) > 9) {
                                modi_char[i + 1] = (modify_loc % 16) - 10 + 'A';
                            }
                            else {
                                modi_char[i + 1] = (modify_loc % 16) + '0';
                            }
                            modify_loc /= 16;
                        }
                        for (i = i + 1; i < 7; i++) {
                            modi_char[i] = '0';
                        }
                        for (i = 0; i < 3; i++) {
                            char change = modi_char[i + 1];
                            modi_char[i + 1] = modi_char[6 -i];
                            modi_char[6 -i] = change;
                        }
                        modi_char[7] = '0';
                        modi_char[8] = '5';
                        modi_char[9] = '\n';
                        modi_char[10] = '\0';
                        strcat(modify, modi_char);
                    }
                    else {
                        int symvalue = searchSYMTAB(tokens[3]);
                        if (symvalue < 0) {
                            fprintf(listing, "Error: undefined symbol -%s", line_temp);
                            continue;
                        }
                        op_length += 3;
                        op_length = op_length << 4;
                        op_length += 1;
                        op_length = op_length << 20;
                        op_length += symvalue;
                        if (strcmp(tokens[2], "LDB") == 0) {
                            b_register = symvalue;
                        }
                        fprintf(listing, "%04X    %-10s %-10s %-10s %06X\n", locctr, tokens[1], tokens[2], tokens[3], op_length);
                        int modify_loc = locctr + 1;
                        char modi_char[11];
                        modi_char[0] = 'M';
                        for (i = 0; ; i++) {
                            if (modify_loc == 0)
                                break;
                            if ((modify_loc % 16) > 9) {
                                modi_char[i + 1] = (modify_loc % 16) - 10 + 'A';
                            }
                            else {
                                modi_char[i + 1] = (modify_loc % 16) + '0';
                            }
                            modify_loc /= 16;
                        }
                        for (i = i + 1; i < 7; i++) {
                            modi_char[i] = '0';
                        }
                        for (i = 0; i < 3; i++) {
                            char change = modi_char[i + 1];
                            modi_char[i + 1] = modi_char[6 -i];
                            modi_char[6 -i] = change;
                        }
                        modi_char[7] = '0';
                        modi_char[8] = '5';
                        modi_char[9] = '\n';
                        modi_char[10] = '\0';
                        strcat(modify, modi_char);
                    }
                }
            }
            text_record += (tokens_length * 2);
            char temp_opcode[tokens_length * 2];
            int i = 0;
            for (i = 0; ; i++) {
                if (op_length == 0) {
                    break;
                }
                if ((op_length % 16) > 9) {
                    temp_opcode[i] = (op_length % 16) - 10 + 'A';
                }
                else {
                    temp_opcode[i] = (op_length % 16) + '0';
                }
                op_length /= 16;
            }
            for (; i < tokens_length * 2; i++) {
                temp_opcode[i] = '0';
            }
            for (i = 0; i < tokens_length; i++) {
                char change = temp_opcode[i];
                temp_opcode[i] = temp_opcode[tokens_length*2 - 1 -i];
                temp_opcode[tokens_length*2 - 1 -i] = change;
            }
            temp_opcode[tokens_length*2] = '\0';
            // printf("tokens_length: %d //// %s: %s", tokens_length, temp_opcode,line_temp);

            if (text_record <= 60) {
                strcat(text, temp_opcode);
                if (text_record == 60) {
                    fprintf(objectFile, "%02X%s\n", 30, text);
                    text_record = 0;
                    for (i = 0; i < 70; i++) {
                        text[i] = '\0';
                        printf("text: %s\n", text);
                    }
                }
            }
            else if (text_record > 60) {
                text_record -= (tokens_length * 2);
                fprintf(objectFile, "%02X%s\n", text_record/2, text);
                text_record = 0;
                text_record += (tokens_length * 2);
                for (i = 0; i < 70; i++) {
                    text[i] = '\0';
                }
                strcat(text, temp_opcode);
                cp = 1;
            }
        }
    }
}

int main(int argc, char *argv[]) 
{
    char *path; // to store open file path
    char *obj_name; // to store output filename
    char line[MAX_LINE_LENGTH] = { 0 };
    unsigned int line_count = 0;

    if (argc < 1)
        return EXIT_FAILURE;
    path = argv[1];
    obj_name = argv[2];

    /* Open file */
    inputFile = fopen(path, "r");

    if (!inputFile)
    {
        perror(path);
        return EXIT_FAILURE;
    }
    pass1(path);

    fclose(inputFile);
    fclose(intermediate);

    pass2(intermediate, obj_name);

}