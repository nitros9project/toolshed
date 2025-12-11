all: bas13.rom extbas11.rom disk11.rom coco3.rom checksum

bas13.rom: equates.asm bas.asm
	lwasm --raw -o $@ $^

cb_equates.asm: equates.asm bas.asm
	lwasm --symbol-dump --no-output $^ > $@

extbas11.rom: cb_equates.asm extbas.asm
	lwasm --raw -o $@ $^

ecb_equates.asm: equates.asm bas.asm extbas.asm
	lwasm --symbol-dump --no-output $^ > $@

disk11.rom: ecb_equates.asm disk.asm
	lwasm --raw --pragma=noindex0tonone -o $@ $^

coco3.rom: equates.asm coco3.asm
	lwasm --raw -o $@ $^

checksum:
	md5 *.rom >md5sums-new.txt

clean:
	-rm -f cb_equates.asm ecb_equates.asm coco3.rom md5sums-new.txt
