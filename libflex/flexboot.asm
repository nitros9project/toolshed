* flexboot.asm - Load FLEX on the Color Computer, from Disk BASIC
*
* Written for ToolShed's cflex gen.  The FLEX.BAS that gen writes reads
* this from track 16, sector 9, into a string, and runs it there, so it
* must be position independent and at most 255 bytes long.
*
* The first sector of FLEX.SYS is at offset 5, where FLEX's LINK puts it.
* Its chain of sectors is followed, and its binary records are loaded,
* until a sector links to 0,0.  Then the transfer address is jumped to,
* with all of the 64K RAM switched in, as FLEX expects.
*
* Sectors are read with Disk BASIC's DSKCON, from drive 0 side 0, into
* DBUF0.  Disk BASIC's ROM is switched back in for each read, and out for
* each byte stored, so FLEX can load over it.  A read error returns to
* BASIC, which says that FLEX didn't load.
*
* Assemble with: lwasm --format=raw --output=flexboot.bin flexboot.asm
* and put the result in libflexboot.c.

DSKCON	equ	$C004		vector to DSKCON
DCOPC	equ	$EA		DSKCON operation: 2 reads
DCDRV	equ	$EB		drive
DCTRK	equ	$EC		track
DCSEC	equ	$ED		sector
DCBPT	equ	$EE		buffer
DCSTA	equ	$F0		status, 0 for success
BUFFER	equ	$0600		DBUF0
ROMMODE	equ	$FFDE		SAM: ROMs in at $8000
RAMMODE	equ	$FFDF		SAM: RAM in at $8000

	org	0

start	bra	begin
	fcb	0,0,0
track	fcb	0		offset 5: first track of FLEX.SYS
sector	fcb	0		offset 6: first sector of FLEX.SYS
xfer	fdb	0		transfer address
load	fdb	0		load address of the current record
stack	fdb	0		stack on entry, to return to BASIC

begin	sts	stack,pcr
	ldd	track,pcr	the first sector, as a link
	std	>BUFFER
	ldy	#BUFFER+256	nothing left to read

record	bsr	getbyte
	cmpa	#$02		data record
	beq	data
	cmpa	#$16		transfer address
	bne	record		nuls, and anything else, are skipped
	bsr	getbyte
	sta	xfer,pcr
	bsr	getbyte
	sta	xfer+1,pcr
	bra	record

data	bsr	getbyte
	sta	load,pcr
	bsr	getbyte
	sta	load+1,pcr
	bsr	getbyte		count
	tfr	a,b
	tstb
	beq	record
	ldx	load,pcr
store	bsr	getbyte
	pshs	cc
	orcc	#$50		no interrupts while BASIC's ROM is out
	sta	>RAMMODE
	sta	,x+
	sta	>ROMMODE
	puls	cc
	decb
	bne	store
	bra	record

* Get the next byte of the file in A, reading its next sector if need be.
* B and X are kept.

getbyte	cmpy	#BUFFER+256
	bne	getnext
	pshs	b,x
	ldd	>BUFFER		link to the next sector
	beq	run		0,0 ends the file
	std	>DCTRK		track, then sector
	lda	#2
	sta	>DCOPC
	clr	>DCDRV
	ldx	#BUFFER
	stx	>DCBPT
	jsr	[DSKCON]
	tst	>DCSTA
	bne	failed
	puls	b,x
	ldy	#BUFFER+4	past the link and record number
getnext	lda	,y+
	rts

run	orcc	#$50
	sta	>RAMMODE
	jmp	[xfer,pcr]

failed	lds	stack,pcr
	rts

	end
