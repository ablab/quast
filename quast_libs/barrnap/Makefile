
EXE=barrnap

.PHONY: database test help clean tarball

help:
#	@echo "make database    # rebuild the pressed HMM files"
	@echo "make test        # run a short test to see if it works"
	@echo "make bigtest     # run a big test on each kingdom"
	@echo "make clean       # remove useless files"
	@echo "make tarball     # [Developer only]"

	
#database:
#	v2.0
#	cd db && cat *.hmm > $(EXE).hmm && hmmpress $(EXE).hmm && rm -f $(EXE).hmm
#	v3.0
#	cd db && hmmpress bacteria.hmm && hmmpress archaea.hmm
	
test:
	./bin/$(EXE) examples/small.fna

bigtest:
	./bin/$(EXE) --kingdom bac  examples/bacteria.fna
	./bin/$(EXE) --kingdom arc  examples/archaea.fna
	./bin/$(EXE) --kingdom euk  examples/fungus.fna
	./bin/$(EXE) --kingdom mito examples/mitochondria.fna
	
	
clean:
	rm -f *~ core foo DEADJOE

TARNAME=$(shell bin/$(EXE) --version 2>&1 | sed 's/ /-/')	
TGZ=/tmp/$(strip $(TARNAME)).tar.gz

tarball: clean
	rm -fr /tmp/$(TARNAME)
	rm -f $(TGZ)
	mkdir /tmp/$(TARNAME)
	cp -rv LICENSE* Makefile README.md bin binaries db examples /tmp/$(TARNAME)
	tar -C /tmp -zcvf $(TGZ) $(TARNAME)
	rm -fr /tmp/$(TARNAME)
	ls -lsa $(TGZ)
