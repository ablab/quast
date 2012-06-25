#!/usr/bin/env Rscript

#
# Assemblathon Mauve scoring summary graphics generation
# (c) 2011 Aaron Darling
# Licensed under the GPL
#
#
# Usage: assemblathonMauvePlots.R <output directory> <alignment files>
#

plotChromobounds <- function(outdir) {
	# now plot chromosome bounds
	chromos <- read.table(paste(outdir, "/chromosomes.txt", sep=""))
	abline(v=chromos$V2, lwd=2, col=2)
	par(srt=90)
	text(x=chromos$V2, y=rep(0,length(chromos$V2)), labels=chromos$V1, pos=4)
	par(srt=0)
}

#
# read and plot all the miscalled bases files
#

plotCalls <- function(outdir, files, pnames, extension, plottitle, pdfname, totalname, column) {
	# first read the total number from each table
	totals <- vector()
	maxpoint<-0
	for(i in 1:length(files)){
		mcname <- paste( files[i], extension, sep="" )
		mc <- read.table( mcname, header=T )
		totals[i] <- nrow(mc)
		maxpoint <- max(maxpoint, mc[,column])
	}

	#read chromosome bounds
	chromos <- read.table(paste(outdir, "/chromosomes.txt", sep=""))
	
	# set up a multi-row figure with one plot for every "maxseries" data series
	maxseries <- 7
	plotrows <- ceiling(length(files) / maxseries)
	pdf(paste(outdir, "/mauve_", pdfname,sep=""),width=10,height=(plotrows*2.25)+2)
	par(mfrow=c(plotrows,1), mar=c(0.25,4,1,2)+0.1, oma=c(5,0,3,0)+0.1)
	plotcount <- 1
	plotinit <- 0
	s <- 1
	ltys <- c(1,2,4,5,6)
	ltys <- c(ltys,ltys,ltys,ltys,ltys,ltys)

	# start plotting data series
	for(i in 1:length(files)){
		if( i %% maxseries == 1 ){
			if(plotcount > 1){
				plotChromobounds(outdir)
				ss <- i-1
				legend("topright", legend=pnames[s:ss], lty=ltys[seq(s,ss)], col=seq(s,ss))
				s <- i
			}
			plotcount <- plotcount + 1
			plotinit <- 0
		}
		mcname <- paste( files[i], extension, sep="" )
		mc <- read.table( mcname, header=T )
		den <- density(c(-1,-1))
		if(nrow(mc)>0){
			den <- density(mc[,column], adjust=0.05)
		}
		#normalize to unit height
		den$y <- den$y / max(den$y)
		# now scale relative to the 'worst'
		den$y <- den$y * totals[i]
		# now add a zero point
		den$x <- c(0,0,den$x)
		den$y <- c(0,den$y[1],den$y)
		# and a max point
		den$x <- c(den$x, den$x[length(den$x)], maxpoint)
		den$y <- c(den$y, 0, 0)
		if(plotinit==0){
			plot(den, las=1, xaxt="n", lty=ltys[i], col=i, xlim=c(0,maxpoint*1.1), ylim=c(0,max(totals)), main="", xlab="Concatenated chromosome coordinates", ylab="")
			axis(1, labels=(plotcount==plotrows+1))
			plotinit <- 1
			if(i==1){
				title(plottitle)
			}
		}else{
			lines(den, lty=ltys[i], col=i)
		}
	}
	plotChromobounds(outdir)
	legend("topright", legend=pnames[s:i], lty=ltys[seq(s,i)], col=seq(s,i))
	dev.off()
	
	# now make a PDF barplot with total counts
	pdf(paste(outdir, "/mauve_total_",pdfname,sep=""))
	barplot(height=totals,names.arg=pnames, main=totalname, las=2, cex.names=0.8)
	dev.off()
}


plotGapSizes <- function(outdir, files, pnames, extension, plottitle, sequence, pdfname, totalname) {
	totals <- vector()
	maxpoint<-0
	for(i in 1:length(files)){
		mcname <- paste( files[i], extension, sep="" )
		mc <- read.table( mcname, header=T )
		totals[i] <- 0
		totals[i] <- sum(mc$Sequence==sequence)
		if(nrow(mc)==0){
			next
		}
		den <- density(log2(mc$Length[mc$Sequence==sequence]), adjust=0.15)
		maxpoint <- max(maxpoint, den$x)
	}

	# set up a multi-row figure with one plot for every "maxseries" data series
	maxseries <- 7
	plotrows <- ceiling(length(files) / maxseries)
	pdf(paste(outdir, "/mauve_", pdfname,sep=""),width=10,height=(plotrows*2.25)+2)
	par(mfrow=c(plotrows,1), mar=c(0.25,4,1,2)+0.1, oma=c(5,0,3,0)+0.1)
	plotcount <- 1
	plotinit <- 0
	s<-1
	ltys <- c(1,2,4,5,6)
	ltys <- c(ltys,ltys,ltys,ltys,ltys,ltys)

	for(i in 1:length(files)){
		if( i %% maxseries == 1 ){
			if(plotcount > 1){
				ss <- i-1
				legend("topright", legend=pnames[s:ss], lty=ltys[seq(s,ss)], col=seq(s,ss))
				s <- i
			}
			plotcount <- plotcount + 1
			plotinit <- 0
		}
		mcname <- paste( files[i], extension, sep="" )
		mc <- read.table( mcname, header=T )
		den <- density(c(-1,-1))
		if(nrow(mc)>0){
			den <- density(log2(mc$Length[mc$Sequence==sequence]), adjust=0.15)
		}
		#normalize to unit height
		den$y <- den$y / max(den$y)
		# now scale relative to the 'worst'
		den$y <- den$y * log2(totals[i])
		# now add a zero point
		den$x <- c(0,0,den$x)
		den$y <- c(0,den$y[1],den$y)
		# and a max point
		den$x <- c(den$x, den$x[length(den$x)], maxpoint)
		den$y <- c(den$y, 0, 0)
		if(plotinit==0){
			plot(den, las=1, xaxt="n", lty=ltys[i], col=i, ylim=c(0, log2(max(totals))), xlim=c(0, maxpoint*0.9), main="", xlab="log2 segment sizes", ylab="")
			if(plotcount==plotrows+1){
				axis(1, at=log2(seq(from=2,to=16, by=1)), labels=FALSE )  
				axis(1, at=log2(seq(from=20,to=60, by=4)), labels=FALSE )  
				axis(1, at=log2(seq(from=80,to=240, by=16)), labels=FALSE )  
				axis(1, at=log2(seq(from=320,to=960, by=64)), labels=FALSE )  
				axis(1, at=log2(seq(from=1280,to=3840, by=256)), labels=FALSE )  
				axis(1, at=c(0,2,4,6,8,10,12),labels=c(1,4,16,64,256,1024,4096))
			}else{
				axis(1, labels=(plotcount==plotrows+1))
			}
			plotinit <- 1
			if(i==1){
				title(plottitle)
			}
		}else{
			lines(den, lty=ltys[i], col=i)
		}
	}
	legend("topright", legend=pnames[s:i], lty=ltys[seq(s,i)], col=seq(s,i))
	dev.off()
	
	# now make a PDF with total counts
	pdf(paste(outdir, "/mauve_total_",pdfname,sep=""))
	barplot(height=totals,names.arg=pnames, main=totalname, las=2, cex.names=0.8)
	dev.off()
}

plotMissingGC <- function(outdir, files, pnames, extension, plottitle, sequence, pdfname, totalname) {
	totals <- vector()
	maxpoint<-0
	# gather up background distribution
	bgdist <- vector()
	for(i in 1:length(files)){
		mcname <- paste( files[i], "_background_gc_distribution.txt", sep="" )
		mc <- read.table( mcname )
		bgdist <- c(bgdist,mc$V1)
	}

	ltys <- c(1,2,4,5,6)
	ltys <- c(ltys,ltys,ltys,ltys,ltys,ltys)

	# first plot the background
	pdf(paste(outdir, "/mauve_", pdfname,sep=""),width=10,height=6)
	den <- density(bgdist)
	den$y <- den$y / max(den$y)
	plot(den, main=plottitle, type="l", lty=ltys[1],lwd=2,ylab=totalname,xlab="Fraction GC")
	# then add each assembly
	for(i in 1:length(files)){
		mcname <- paste( files[i], extension, sep="" )
		mc <- read.table(mcname)
		den <- density(mc$V1)
		den$y <- den$y / max(den$y)
		lines(den, lty=ltys[i+1], col=(i+1))
	}
	legend("topright", legend=c("Background GC",pnames), lty=ltys[seq(1,length(pnames)+1)], col=seq(1,length(pnames)+1))
	dev.off()
	
}

##### MAIN ####
args <- commandArgs(TRUE)
outdir <- args[1]
files <- args[2:length(args)]


scoresum <- read.table(paste(outdir, "/summaries.txt", sep=""),header=T) 

pnames <- sub( ".fa.fas", "", scoresum$Name, perl=TRUE)

plotCalls(outdir, files,pnames, "__miscalls.txt", "Density of miscalled bases in genome", "miscalls.pdf", "Total number of miscalled bases", 4)
plotCalls(outdir, files,pnames, "__uncalls.txt", "Density of uncalled bases in genome", "uncalls.pdf", "Total number of uncalled bases", 4)
plotCalls(outdir, files,pnames, "__gaps.txt", "Density of missing and extra segments in genome", "missingextra.pdf", "Total number of missing or extra bases", 6)
plotGapSizes(outdir, files,pnames, "__gaps.txt", "Size distribution of missing segments", "assembly", "missing_sizes.pdf", "Total number of missing segments")
plotGapSizes(outdir, files,pnames, "__gaps.txt", "Size distribution of extra segments", "reference", "extra_sizes.pdf", "Total number of extra segments")

#####################################
### NOT NEEDABLE FOR OUR PURPOSES ###
#####################################

#plotMissingGC(outdir, files,pnames, "_missing_gc.txt", "GC content distribution of missing regions", "reference", "missing_gc.pdf", "Total number of missing segments")

#pdf(paste(outdir, "/mauve_scaffold_counts.pdf", sep=""))
#scafs <- log10(scoresum$NumContigs)
#scafs[is.infinite(scafs)]<-0
#maxy<-ceiling(max(scafs))
#barplot(height=scafs, names.arg=pnames, ylab="count (log10 scale)", main="Scaffold counts", las=2, cex.names=0.5,yaxt="n")
#axis(side=2,at=seq(0,maxy),labels=10^seq(0,maxy,by=1))
#dev.off()

#pdf(paste(outdir, "/mauve_lcb_counts.pdf", sep=""))
#bpoints <- log10(scoresum$NumLCBs)
#bpoints[is.infinite(bpoints)]<-0
#maxy2<-ceiling(max(bpoints))
#barplot(height=bpoints, names.arg=pnames, ylab="count (log10 scale)", main="Breakpoints", las=2, cex.names=0.5,yaxt="n")
#axis(side=2,at=seq(0,maxy2),labels=10^seq(0,maxy2,by=1))
#dev.off()

#pdf(paste(outdir, "/mauve_dcj_dist.pdf", sep=""))
#dcjdist <- log10(scoresum$DCJ_Distance)
#dcjdist[is.infinite(dcjdist)]<-0
#maxy3<-ceiling(max(dcjdist))
#barplot(height=dcjdist, names.arg=pnames, ylab="count (log10 scale)", main="DCJ Distance", las=2, cex.names=0.5,yaxt="n")
#axis(side=2,at=seq(0,maxy3),labels=10^seq(0,maxy3,by=1))
#dev.off()

#pdf(paste(outdir, "/mauve_scaffold_vs_lcb.pdf", sep=""))
#plot(bpoints,scafs, main="Scaffold count and Breakpoint count",yaxt="n",xaxt="n",ylab="Scaffold count (log10)",xlab="LCB count (log10)")
#axis(side=2,at=seq(0,maxy),labels=10^seq(0,maxy,by=1))
#axis(side=1,at=seq(0,maxy2),labels=10^seq(0,maxy2,by=1))
#dev.off()

#pdf(paste(outdir, "/mauve_basecall_precision.pdf", sep=""), width=10, height=6)
#baseacc <- scoresum$NumAssemblyBases-scoresum$NumMisCalled-scoresum$NumUnCalled-scoresum$ExtraBases
#poscalls <- baseacc / scoresum$NumAssemblyBases
#barplot(height=poscalls, names.arg=pnames, ylab="fraction", main="Precision of bases called in haplotype", las=2, cex.names=0.5)
#dev.off()

#pdf(paste(outdir, "/mauve_basecall_recall.pdf", sep=""), width=10, height=6)
#baseacc <- scoresum$NumReferenceBases-scoresum$NumMisCalled-scoresum$NumUnCalled-scoresum$TotalBasesMissed
#poscalls <- baseacc / scoresum$NumReferenceBases
#barplot(height=poscalls, names.arg=pnames, ylab="fraction", main="Recall of bases in haplotype", las=2, cex.names=0.5)
#dev.off()

#
# Make the base miscall bias plot
#
blob<-scoresum
normalizer <- blob$AA+blob$AC+blob$AG+blob$AT+blob$CA+blob$CC+blob$CG+blob$CT+blob$GA+blob$GC+blob$GG+blob$GT+blob$TA+blob$TC+blob$TG+blob$TT
normalizer[normalizer==0]<-1
#mlim <- max(abs( c(blob$AA/normalizer,blob$AC/normalizer,blob$AG/normalizer,blob$AT/normalizer,blob$CA/normalizer,blob$CC/normalizer,blob$CG/normalizer,blob$CT/normalizer,blob$GA/normalizer,blob$GC/normalizer,blob$GG/normalizer,blob$GT/normalizer,blob$TA/normalizer,blob$TC/normalizer,blob$TG/normalizer,blob$TT/normalizer) ))

equalfreqs <- c(0,1/12,1/12,1/12, 1/12,0,1/12,1/12, 1/12,1/12,0,1/12, 1/12,1/12,1/12,0)
assemblathon<-0
rowlimit <- nrow(blob)
if(assemblathon==1){
	rowlimit <- 88
	truerow<-92
	h2mat <- matrix(data=c(blob$AA[truerow]/normalizer[truerow],blob$AC[truerow]/normalizer[truerow],blob$AG[truerow]/normalizer[truerow],blob$AT[truerow]/normalizer[truerow],blob$CA[truerow]/normalizer[truerow],blob$CC[truerow]/normalizer[truerow],blob$CG[truerow]/normalizer[truerow],blob$CT[truerow]/normalizer[truerow],blob$GA[truerow]/normalizer[truerow],blob$GC[truerow]/normalizer[truerow],blob$GG[truerow]/normalizer[truerow],blob$GT[truerow]/normalizer[truerow],blob$TA[truerow]/normalizer[truerow],blob$TC[truerow]/normalizer[truerow],blob$TG[truerow]/normalizer[truerow],blob$TT[truerow]), nrow=88, ncol=16,byrow=TRUE)
}else{
	
	h2mat <- matrix(equalfreqs, nrow=rowlimit, ncol=16, byrow=TRUE)
}
submlim <- cbind(blob$AA[0:rowlimit]/normalizer[0:rowlimit],blob$AC[0:rowlimit]/normalizer[0:rowlimit],blob$AG[0:rowlimit]/normalizer[0:rowlimit],blob$AT[0:rowlimit]/normalizer[0:rowlimit],blob$CA[0:rowlimit]/normalizer[0:rowlimit],blob$CC[0:rowlimit]/normalizer[0:rowlimit],blob$CG[0:rowlimit]/normalizer[0:rowlimit],blob$CT[0:rowlimit]/normalizer[0:rowlimit],blob$GA[0:rowlimit]/normalizer[0:rowlimit],blob$GC[0:rowlimit]/normalizer[0:rowlimit],blob$GG[0:rowlimit]/normalizer[0:rowlimit],blob$GT[0:rowlimit]/normalizer[0:rowlimit],blob$TA[0:rowlimit]/normalizer[0:rowlimit],blob$TC[0:rowlimit]/normalizer[0:rowlimit],blob$TG[0:rowlimit]/normalizer[0:rowlimit],blob$TT[0:rowlimit]/normalizer[0:rowlimit]) 

ranger <- submlim/h2mat
ranger[h2mat==0] <- 1
mlims <- range(ranger)
if(mlims[1]==0){
	mlims[1]<-mlims[2]-1
}
mlim <- max(1/mlims[1],mlims[2])

coltable <- hsv(h=rep(0.5,100), s=seq(0.495,0,by=-0.005), v=seq(0.505,1,by=0.005))
coltable <- c(coltable,rev(heat.colors(100)))

maxseries <- 8
plotrows <- ceiling(nrow(blob) / maxseries)
pdf(paste(outdir, "/mauve_basecall_bias.pdf", sep=""),width=8,height=(plotrows*1.75))
par(mfrow=c(plotrows,maxseries), mar=c(0,0,0,0)+1, oma=c(5,0,3,0)+0.1, mex=0.7, mgp=c(3,0,0), cex.axis=0.6)

haplo2mat <- equalfreqs
if(assemblathon==1){
	i<-truerow
	haplo2mat <- cbind( c(blob$AA[i],blob$AC[i],blob$AG[i],blob$AT[i]), c(blob$CA[i],blob$CC[i],blob$CG[i],blob$CT[i]), c(blob$GA[i],blob$GC[i],blob$GG[i],blob$GT[i]), c(blob$TA[i],blob$TC[i],blob$TG[i],blob$TT[i]) )
	haplo2mat <- haplo2mat / sum(haplo2mat)
}

for(i in 1:nrow(blob)){
	submat <- cbind( c(blob$AA[i],blob$AC[i],blob$AG[i],blob$AT[i]), c(blob$CA[i],blob$CC[i],blob$CG[i],blob$CT[i]), c(blob$GA[i],blob$GC[i],blob$GG[i],blob$GT[i]), c(blob$TA[i],blob$TC[i],blob$TG[i],blob$TT[i]) )
	submat <- submat / sum(submat)
	differ <- submat/haplo2mat
	differ[haplo2mat==0]<-1
	image(differ,zlim=c(2-mlim, mlim),col=coltable,xaxt="n",yaxt="n", mex=0.7, mgp=c(3,0,0), cex.axis=0.6,main=pnames[i], cex.main=0.7)
	axis(side=1,at=seq(0,1,by=(1/3)),labels=c("A","C","G","T"),tick=F, mgp=c(3,0,0), cex.axis=0.6)
	axis(side=2,at=seq(0,1,by=(1/3)),labels=c("A","C","G","T"),tick=F, mgp=c(3,0,0), cex.axis=0.6)
}

mlimticks <- seq(1,mlim,by=((mlim-1)/50))
mlimticks <- c(rev(1/mlimticks),mlimticks[2:length(mlimticks)])

image(x=mlimticks,y=c(1),z=matrix( mlimticks[1:100], nrow=100, ncol=1 ), zlim=c(2-mlim,mlim), col=coltable,yaxt="n", mex=0.7, mgp=c(3,0,0), cex.axis=0.6, main="Key: obs/exp", cex.main=0.7)

dev.off()
