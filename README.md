ASM7000
=======

Tiny assembler for TMS-7000 / PIC-7000 series
---------------------------------------------

Version v0.1.0-alpha

Work in progress. This is already viable to assemble the source code
of the CTS256A-AL2 chip from G.I. without error and without any difference
wrt. the binary image (https://www.github.com/GmEsoft/CTS256A-AL2).


Current limitations:
--------------------

- Macros not supported;
- No support for linkable object files;
- No support for expressions in constants;
- Decimal and hexadecimal literals supported, but not binary literals;
- Generates only a binary core image file, no support yet for hexadecimal output files (Intel HEX, etc.);
- Symbols not allowed for registers Rn and ports Pn;
- Code (mnemonics and pre-defined symbols) must be in upper case.


To compile the assembler
------------------------

With Microsoft Visual Studio, execute the following commands to produce the executable `ASM7000.exe`:
````
call /path/to/vcvars32.bat
cl /EHsc ASM7000.cpp
````

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

- Allow lower case;
- Allow symbols for `Rn` and `Pn`;
- Allow expressions;
- Allow binary constants;
- Allow Z-80-style hex constants `0nnH`;
- Generate Intel HEX format output.


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
