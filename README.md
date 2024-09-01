ASM7000
=======

Tiny assembler for TMS-7000 / PIC-7000 series
---------------------------------------------

Version v0.3.0-alpha+DEV

Work in progress. This is already viable to assemble the source code
of the CTS256A-AL2 chip from G.I. without error and without any difference
wrt. the binary image (https://www.github.com/GmEsoft/CTS256A-AL2).


Current limitations:
--------------------

- [x] Fields must be separated by a tab - no spaces allowed;
- [ ] Macros not supported;
- [x] Conditional assembly not supported;
- [ ] No support for linkable object files;
- [x] No full support for expressions in constants;
- [x] Decimal and hexadecimal literals supported, but not binary literals;
- [ ] Generates only a binary core image file, no support yet for hexadecimal output files (Intel HEX, etc.);
- [x] Symbols not allowed for registers `Rn` and ports `Pn`;
- [x] Code (mnemonics and pre-defined symbols) must be in upper case.;
- [x] No support for `INCLUDE file`;
- [x] No support for functions;
- [ ] No support for functions with 2 or more args.


History
-------

### v0.3.0-alpha+dev:
- new Macro class;
- new in-line macro REPT.

### v0.3.0-alpha:
- new Parser class, supporting new operators, parentheses and user-defined functions;
- support operators `DUP`, `>>`, `<<`, `&`, `|`;
- fix parsing of expressions `X-Y-Z` evaluated `X-(Y-Z)`;
- new Help text;
- String arguments;
- `INCLUDE/COPY files`;
- fix quote processing in `split()`;
- new Log class for messages;
- added compatibility warnings for extensions wrt. to the original syntax;
- new option `-NC`: no compatibility warnings;
- new option `-ND-`: enable debug messages;
- new option `-NW`: no warnings;
- symbols can be EQUated to `Rnn` and `Pnn`;
- display total # of warnings;
- new `IF` / `ELSE` / `ENDIF` and synonyms;
- user-defined functions `name FUNCTION/FUNC arg,expression`;
- new `SAVE` / `RESTORE` options;
- new `LISTING ON/OFF`: enable/disable listing;
- new `DB nn,'text',...` / `DS nnnn` / `DB nnnn DUP(nn)` / `DW nnnn,...`
- fix `NOP` op-code;
- fix `DJNZ A/B,offset` wrong offset value;
- fix `TRAP n` op-codes (reversed);
- new `ERROR` / `WARNING` / `INFO` / `DEBUG` messages;
- new `ASSERT_EQUAL x,y`: raise an error if `x` is not equal to `y`.

### v0.2.1-alpha:
- allow white spaces in arguments field.
- display wrong arg types in `Bad arg(s)` error messages.

### v0.2.0-alpha:
- convert tokens to uppercase taking quotes into account;
- parse binary strings;
- parse expressions containing + or - (todo: unary '-');
- parse char literals;
- ignore ':' after labels;
- allow symbols for regs and ports;
- allow 'H' and 'B' as suffixes for hex and binary literals;
- fix (B) handling (error in "%10" parsing);
- add "-NE" to disable error listing to stderr;
- allow space and ':' as field separators;
- synonym ORG = AORG;
- synonym DB = BYTE/TEXT (todo: DB "text");
- synonym DW = DATA;
- fix DJNZ.

### v0.1.0-alpha: 
- initial commit


To compile the assembler
------------------------

With Microsoft Visual Studio, execute the following commands to produce the executable `ASM7000.exe`:
````
call /path/to/vcvars32.bat
cl /EHsc ASM7000.cpp
````

To run the assembler
--------------------

Command syntax:
````
ASM7000 [options] -i:InputFile[.asm] -o:OutputFile[.cim] [-l:Listing[.lst]]
         -I:inputfile[.asm[   input source file
         -O:outputfile[.cim]  output object file
         -L:listing[.lst]     listing file
Options: -NC  no compatibility warning
         -ND- enable debug output
         -NE  no output to stderr
         -NH  no header in listing
         -NN  no line numbers in listing
         -NW  no warning
````

Assembler syntax
----------------

'*' following a directive or synonym denotes an extension or a deviation to the original assembler syntax.

### Directives:
- `      $IF   condition`: Start of a conditional assembly block. Synonyms: `IF`* and `COND`*.
- `      $ELSE`: Start of the ELSE clause in the conditional assembly block. 
  Synonym: `ELSE`*.
- `      $ENDIF`: End the conditional assembly block. Synonyms: `ENDIF`* and `ENDC`*.
- `      COPY filename`: Insert the contents of the given filename. Synonyms: `INCLUDE`* and `GET`*.
- `      SAVE`*: Save the current values of the option flags.
- `      RESTORE`*: Restore the saved values of the option flags.
- `      CPU  name`*: CPU type (ignored).
- `      PAGE ON/OFF`*: Page flag (currently unhandled).
- `      LISTING ON/OFF`*: Listing flag (currently unhandled).
- `name  FUNCTION [args,...],expr`*: Function definition. Formal arguments in `args,...` and the evaluated 
  expression in `expr`. Synonym: `FUNC`*.
- `name  MACRO [args,...]`*: Macro definition. Formal arguments in `args,...` (not yet handled).
- `      ENDM`*: End of macro definition. (not yet handled).
- `      REPT times`*: Repeated block macro definition.
- `      IRP  #var,params`*: Parameters iterator block macro definition. (not yet handled).
- `      IRPC #str,times`*: String iterator block macro definition. (not yet handled).
- `      ERROR 'message'`*: Generates an error message.
- `      WARNING 'message'`*: Generates a warning message.
- `      INFO 'message'`*: Generates an informational message.
- `      DEBUG 'message'`*: Generates a debug message (printed only if `-nd-` is specified in the
  command line).
- `      ASSERT_EQUAL x,y`: raises an error if x and y are not equal.


### Pseudo-Ops:
- `[lbl]  AORG nnnn`: Set the location pointer to `nnnn`. Synonym: `ORG`*.
- `lbl    EQU  expr`: Define the label `lbl` equal to the value of `expr`.
- `[lbl]  BYTE nn[,nn...]`: Define a block of bytes `nn[,nn...]`.
- `[lbl]  DATA nnnn[,nnnn...]`: Define a block of 16-bit words`nn[,nn...]`. MSB first. Synonym: `DW`*.
- `[lbl]  TEXT [-]'text string'`: Define a text string. If `-` precedes the string, the last value
  is negated (not yet handled).
- `[lbl]  DB nn[,nn...][,'text string'...]`*: Define a block of bytes `nn[,nn...]` or text strings.
- `[lbl]  DB n DUP(byte)`*: Define a of bytes `byte` repeated `n` times.
- `[lbl]  DS n`*: Bump the location counter by `n` bytes.

### Arguments:
- `Rnn`: Processor registers. May be aliased using an `EQU` pseudo-op: `FLAGS EQU R10`; `OR %>01,FLAGS`.
- `Pnn`: Processor I/O ports. May be aliased using an `EQU` pseudo-op: `APORT EQU P4`; `ANDP %>FE,APORT`.
- `%nn`: Immediate values: `MOVD %>F000,R3`. Can be an expression.
- `@nn`: Direct long address: `CALL @ENCODE`. Short relative jumps don't use that notation: `JMP LOOP`. 
  Can be an expression.
- `@nn(B)`: Direct long address, indexed by B: `LDA @SCT1TB(B)`. Can be an expression.
- `*nn`: Indirect address via registers pair: `LDA *RULPTR`.  Can be an expression.
- `*nn(B)`: Indirect address via registers pair, indexed by B: `LDA *TABPTR(B)`.  Can be an expression.

### Literals:
- `nnn`: Decimal value.
- `>nn`: Hexadecimal value.
- `'c'`: Character value.
- `$`: The value of the location counter.
- `nnH`*: Hexadecimal value.
- `bbbbbbbbB`*: Binary value.
- `DATE`*: The current date as a `TEXT`/`DB` string `MM/DD/YY` (or depending on a specified format, tbd).
- `TIME`*: The current time as a `TEXT`/`DB` string `HH:MM:SS` (or depending on a specified format, tbd).

### Expressions:
Operators are evaluated left to right unless a sub-expression is enclosed in parentheses.
- `nn + expr`: Addition.
- `nn - expr`: Subtraction.
- `nn & expr`*: Bitwise AND.
- `nn | expr`*: Bitwise OR.
- `nn << expr`*: Left shift.
- `nn >> expr`*: Right shift.
- `func(expr)`*: Evaluation of the user-defined function `func` on the parameter value of `expr`.

(to be expanded)


Sample session:
---------------

Run the assembler on the source file `CTS256A.asm`, creating the object binary file `CTS256A.bin` 
and the listing file `CTS256A.lst`:

````
ASM7000 -i:CTS256A.asm -o:CTS256A.cim -l:CTS256A.lst
** TMS-7000 tiny assembler - v0.1.0-alpha - (C) 2024 GmEsoft, All rights reserved. **

Pass: 1
Pass: 2
    0 TOTAL ERROR(S)
````

Listing `CTS256A.lst`:

````
** TMS-7000 tiny assembler - v0.1.0-alpha - (C) 2024 GmEsoft, All rights reserved. **

	Sat Aug 17 20:34:55 2024

	Assembly of  : CTS256A.asm
	Output file  : CTS256A.cim
	Listing file : CTS256A.lst

    1:                  ;** TMS-7000(tm) DISASSEMBLER V1.10beta3 - (c) 2015-20 GmEsoft, All rights reserved. **
    2:                  ;
    3:                  ;	Tue Dec 27 08:39:46 2022
    4:                  ;
    5:                  ;	Disassembly of : CTS256A.BIN
    6:                  ;	Equates file   : CTS256A.EQU
    7:                  ;	Screening file : CTS256A.SCR
    8:                  
    9:                  
   10:                  ;==============================================================================
   11:                  
   12:  F000            	AORG	>F000
   13:                  
   14:                  ;==============================================================================
   15:                  
   16:                  	; Main entry point
   17:  F000  523A      CTS256	MOV	%>3A,B
   18:  F002  0D        	LDSP			; Init stack pointer 3B-XX
   19:  F003  8820002D  	MOVD	%SP0256,R45	; R45 := $2000 - Memory-mapped parallel output to SP0256A-AL2
   20:  F007  A2AA00    	MOVP	%>AA,P0		; P0 = IOCNT0 := 1010 1010
   21:                  				; 	Full Expansion;
   22:                  				;	Clear INT1, INT2 and INT3 flags
   23:  F00A  A20A10    	MOVP	%>0A,P16	; P16 = IOCNT1 := 0000 1010
   24:                  				;	Clear INT4 and INT5 flags
   25:  F00D  9104      	MOVP	P4,B		; Read P4 = APORT
   26:  F00F  5307      	AND	%>07,B		; Get Serial mode
(...)
 1500:                  ;==============================================================================
 1501:                  
 1502:                  	; Index of rules tables
 1503:  FFBC  F78C      TABRUL	DATA	RLPNCT
 1504:  FFBD            TABRU1	EQU	$-1
 1505:  FFBE  F7CCF882  	DATA	RULESA,RULESB,RULESC,RULESD
              F8CCF919
 1506:  FFC6  F976FA30  	DATA	RULESE,RULESF,RULESG,RULESH
              FA44FA8A
 1507:  FFCE  FAB4FB4C  	DATA	RULESI,RULESJ,RULESK,RULESL
              FB55FB66
 1508:  FFD6  FB87FB99  	DATA	RULESM,RULESN,RULESO,RULESP
              FBDFFCE1
 1509:  FFDE  FD0DFD29  	DATA	RULESQ,RULESR,RULESS,RULEST
              FD3DFDC2
 1510:  FFE6  FE8EFECF  	DATA	RULESU,RULESV,RULESW,RULESX
              FEDCFF2C
 1511:  FFEE  FF3AFF84  	DATA	RULESY,RULESZ,RULNUM,>FFFF
              FF8EFFFF
 1512:                  
 1513:                  ;==============================================================================
 1514:                  	; Interrupt vectors
 1515:  FFF6  F1BCF1C1  	DATA	INT4,INT3,>FFFF,INT1
              FFFFF385
 1516:  FFFE  F000      	DATA	CTS256
 1517:                  
 1518:                  ;==============================================================================
 1519:  0200            PARLINP	EQU	>0200		; Memory-mapped parallel input from Host
 1520:  2000            SP0256	EQU	>2000		; Memory-mapped parallel output to SP0256A-AL2
 1521:  3000            CTSXRAM	EQU	>3000		; CTS-256A External RAM origin
 1522:                  ;==============================================================================
 1523:                  
 1524:  0000            	END	CTS256
    0 TOTAL ERROR(S)

````

Copyright notice
----------------

Microchip, Inc. holds the copyrights to the CTS256A-AL2 ROM Image.
Microchip retains the intellectual property rights to the algorithms and data the emulated device CTS256A-AL2 contains.


To do next
----------

- [x] Allow lower case;
- [x] Allow spaces as field separators;
- [x] Allow symbols for `Rn` and `Pn`;
- [x] Allow expressions;
- [ ] Generate Intel HEX format output;
- [x] Support `INCLUDE` files.
- [x] parse binary strings;
- [x] parse expressions containing + or - (todo: unary '-');
- [x] parse char literals;
- [x] ignore ':' after labels;
- [x] allow 'H' and 'B' as suffixes for hex and binary literals;
- [x] synonym ORG = AORG;
- [x] synonym DB = BYTE/TEXT (todo: DB "text");
- [x] synonym DW = DATA;
- [x] Refactor/streamline the parser;
- [ ] Add DATE and TIME string constants;
- [x] Conditional assembly not supported;
- [ ] Encapsulate parts of code in C++ classes (in progress).

Known issues
------------
- [x] parsing of expressions `X-Y-Z` evaluated `X-(Y-Z)`
- [x] arguments field containing white space, eg: `mov %10, count`
- [x] `DB "string"` not handled
- [x] `DB count DUP (x)` not handled
- [x] `hi(x)` & `lo(x)` not handled (functions)
- [ ] functions with 2 or more args
- [ ] `DS` not generating filling zeros in CIM format
- [ ] unary ops not working


GPLv3 License
-------------

Created by Michel Bernard (michel_bernard@hotmail.com) -
<http://www.github.com/GmEsoft/ASM7000>

Copyright (c) 2024 Michel Bernard. All rights reserved.

This file is part of ASM7000.

ASM7000 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ASM7000 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ASM7000.  If not, see <https://www.gnu.org/licenses/>.
