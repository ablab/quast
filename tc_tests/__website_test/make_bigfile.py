with open("smallfile.fasta", 'r') as sf:
	with open("bigfile.fasta", 'a') as bf:
		bf.write(sf.read())
		bf.write(sf.read())
		bf.write(sf.read())
		bf.write(sf.read())
	