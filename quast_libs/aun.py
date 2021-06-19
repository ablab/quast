from numpy.linalg import norm


def auN(numlist: list) -> float:
	'''
	numlist - length of contigs
	metric explanation http://lh3.github.io/2020/04/08/a-new-metric-on-assembly-contiguity
	'''
	assert len(numlist) > 0, 'Empty list as input'
	denum = sum(numlist)
	assert denum > 0, 'all contigs are 0'
	return sum([norm(n) for n in numlist]) / denum
