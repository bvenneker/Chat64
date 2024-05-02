.var CARTRIDGE_8K   = $2000
.var CARTRIDGE_SIZE = CARTRIDGE_8K
.segment CARTRIDGE_FILE [start=$8000,min=$8000, max=$8000+CARTRIDGE_SIZE-1, fill,outBin="cartridge.bin"]   
//=========================================================================================================
//	Start of cartridge memory and start parameters
//=========================================================================================================
* = $8000											// Start address cartridge
													//
	.word	coldstart								// Cartridge cold-start vector dynamic
	.word	warmstart								// Cartridge warm-start vector dynamic
	.byte	$C3, $C2, $CD, $38, $30					// CBM8O - Cartridge present string
													//
//=========================================================================================================
//	Cold start procedure
//=========================================================================================================

coldstart:										
		// KERNAL RESET ROUTINE
		sei
		stx $d016		// Turn on VIC for PAL / NTSC check
		jsr $fda3		// IOINIT - Init CIA chips
		jsr $fd50		// RANTAM - Clear/test system RAM
		//lda #$a0
		//sta $0284		// ignore cartridge ROM for end of detected RAM for BASIC
		jsr $fd15		// RESTOR - Init KERNAL RAM vectors
		jsr $ff5b		// CINT   - Init VIC and screen editor
		//cli			// Re-enable IRQ interrupts

warmstart:
		lda #<(nmi)                                   // \
		sta $0318                                     //  \ Load our new nmi vector
		lda #>(nmi)                                   //  / And replace the old vector to our own nmi routine
		sta $0319                                     // /			
					
		// BASIC RESET  Routine
		jsr $e453		// Init BASIC RAM vectors
		jsr $e3bf		// Main BASIC RAM Init routine
		jsr $e422		// Power-up message / NEW command
		ldx #$fb
		txs			    // Reduce stack pointer for BASIC
		

//=========================================================================================================
// Main program
//=========================================================================================================
jsr $E544               // clear the screen
lda #1
sta $02
sta $05
                        // tell the ESP32 we are ready to recieve data        
lda #100
sta $de00               // write byte 100 to IO1

                        // All the real actions is interupt driven, through the NMI routine
!wait:                  // We just wait here
lda $02					// until the value of $02 is zero
cmp #0                  //
beq !run+               // that means all bytes have been received
jmp !wait-              // and we can continue to run


!run:                   // run the program!
lda #0
sta $d020
sta $d021
jsr $A660               // CLR
cli                     // enable interrupts
jmp $a7ae               // jump to basic RUN command



//=========================================================================================================
// NMI ROUTINE
//=========================================================================================================
// $02 = 1 receive border color (store it in $03) colors 0-15, 20=blinking
// $02 = 2 receive screen color (store it in $04) colors 0-15, 20=blinking
// $02 = 3 write start address, low byte ($fb)
// $02 = 4 write start address, high byte ($fc)
// $02 = 5 write end address, low byte ($fd)
// $02 = 6 write end address, high byte ($fe)
// $02 = 7 write program bytes
// $02 = 0 run the program
nmi:                                              // When the ESP32 loads a byte in the 74ls244 it pulls the NMI line low
                                                  // to signal the C64. Telling it to read the byte
    pushreg()                                     // 

	lda $02                                       // check the status of $2
	cmp #0                                        // if zero, we have nothing to de here, exit
	bne !+
	jmp !exit_nmi+

!:  lda $03                                       // blink the border color if requested
    cmp #20
    bne !+
    inc $d020                                     
!:  lda $04                                       // blink the screen color if requested
    cmp #20
    bne !+
    inc $d021

!:
!n:
    lda $02                                       // check the status of $2
    cmp #1
    beq !wbc+                                     // $02 = 1 write border color
    cmp #2
    beq !wsc+                                     // $02 = 2 write screen color
	cmp #3
	beq !wslb+                                    // $02 = 3 write start address, low byte ($fb)
	cmp #4
	beq !wshb+                                    // $02 = 4 write start address, high byte ($fc)
	cmp #5
	beq !welb+                                    // $02 = 5 write end address, low byte ($fd)
	cmp #6
	beq !wehb+                                    // $02 = 6 write end address, high byte ($fe)
	cmp #7
	beq !wpb+                                     // $02 = 7 write program bytes
	
	
!wbc:                                             // set the border color
  lda $df00	
  sta $d020
  sta $03
  inc $02
  jmp !exit_nmi+

!wsc:                                             // set the screen color
  lda $df00	
  sta $d021
  sta $04
  inc $02
  jmp !exit_nmi+
  
!wslb:                                            // write start address low byte ($fb)
  lda $df00
  sta $fb
  inc $02	
  jmp !exit_nmi+
 
!wshb:                                            // write start addres high byte ($fc)
  lda $df00
  sta $fc
  inc $02
  jmp !exit_nmi+

!welb:                                            // write end address low byte ($fd)
  lda $df00
  sta $fd
  inc $02
  jmp !exit_nmi+
 
!wehb:                                            // write end address high byte ($fe)
  lda $df00
  sta $fe
  inc $02
  jmp !exit_nmi+

!wpb:                                             // write program bytes
  lda $df00                                       // load the byte from io2
  ldy #0                                          //
  sta ($fb),y                                     // write the byte to the address through the pointer in $fb-$fc
  
                                                  // compare the address we just used to the end address to see if we just wrote the final byte
  lda $fc                                         // load the high byte of the address we just wrote to 
  cmp $fe                                         // compare it to the value of $fe (that has the high byte of the end address)
  bne !next+                                      // if not equal we can continue with the next byte
  lda $fb                                         // load the low byte of the address we just wrote to
  cmp $fd                                         // compare it to the low byte of the end address
  bne !next+                                      // if not equal we can continue with the next byte
  lda #0                                          // at this point both high and low byte are equal, we have just written the final byte!
  sta $02
  jmp !exit_nmi+

!next:
  inc $fb
  lda $fb
  cmp #0                                          // if the low byte rolls over, we also need to increase the high byte
  bne !exit_nmi+
  inc $fc

!exit_nmi:                                        // 
    lda #$01                                      // acknoledge the nmi interrupt
    sta $dd0d                                     // you MUST write and read this address to acknoledge the nmi interrupt
    lda $dd0d                                     // you MUST write and read this address to acknoledge the nmi interrupt
    popreg()                                      // 
    rti                                           // return interupt
                                                  
    
//=========================================================================================================//
//  MACROS
//=========================================================================================================
.macro pushreg(){                                 // 
                                                  // 
    php                                           // push the status register to stack
    pha                                           // push A to stack
    txa                                           // move x to a
    pha                                           // push it to the stack
    tya                                           // move y to a
    pha                                           // push it to the stack
    }                                             // 
                                                  // 
.macro popreg(){                                  // 
                                                  // 
    pla                                           // pull the y register from the stack
    tay                                           // move it to the y register
    pla                                           // pull the x register from the stack
    tax                                           // move it to the x register
    pla                                           // pull the acc from the stack
    plp                                           // pull the the processor status from the stack
    }                                             // 
                                                  //                                                   

.fill ($8000+CARTRIDGE_SIZE - *),0                // fill the rest of the cartridge space with 0