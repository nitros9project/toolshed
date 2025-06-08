
/* top nibble classifications */
void	(*topnib[])(void) = {imm, moveb, movel, movew, misc, addq, bsr,
					    moveq, or, sub, huh, cmp, and, add, shift, huh};

void	(*modetbl[])(int) = {drd, ard, ri, piri, pdri, rio, irio, spadd};
void	(*spaddtbl[])(void) = {as, al, relo, relio, aimm, as, as, as};

char	*imm_mne[] = {"ori", "andi", "subi", "addi",
					"huh", "eori", "cmpi", "huh"};
char	*bittyp[] = {"btst ", "bchg ", "bclr ", "bset "};

void	(*misctbl[])(void) = {negx, clr, neg, not, nbcd, tst, movem, trap};
char	*msc_mne[] = {"reset", "nop", "stop", "rte",
						"rtd", "rts", "trapv", "rtr"};

char	*cc_mne[] = {"t", "f", "hi", "ls", "cc", "cs", "ne", "eq",
					"vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"};

char	*shft_mne[] = {"as", "ls", "rox", "ro"};

char	*movec_mne[] = {"sfc", "dfc", "cacr", "usp",
						"vbr", "caar", "msp", "isp"};
