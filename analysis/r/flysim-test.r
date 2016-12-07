
library(rgl)
library(data.table)
library(plyr)
library(twiddler)


setwd('Z:/people/Abel/perching-analysis/')

# TODO: Read all points, then plot, to make sure line is recognized...
all <- fread('data/flysim.dbg.csv',sep=',')
colnames(all) <- c('frame','x','y','z')
all <- flysim[flysim$dist > 800,]

fAll <- function(frameStart, frameRange) {
  rgl.clear()
  a <- all[all$frame > frameStart & all$frame < (frameStart+frameRange),]
  plot3d(a$x, a$y, a$z, col=a$trajectory)
}

open3d()
twiddle(
  fAll(frameStart, frameRange),
  frameStart = knob(c(min(all$frame), max(all$frame)), 1),
  frameRange = knob(c(0, 1000), 1),
  auto = FALSE)

###############################################

fAll <- function(trajID) {
  rgl.clear()
  a <- all[all$trajectory == unique(all$trajectory)[trajID],]
  plot3d(a$x, a$y, a$z)
  #print(nrow(a))
  print(a[1,1:4])
}

open3d()
twiddle(
  fAll(trajID),
  trajID = knob(c(0, length(unique(all$trajectory))), 1),
  auto = FALSE)

###############################################

# 
readFlySim <- function(fname) {
  data <- fread(fname,sep=',')
  colnames(data) <- c('p1','p2','p3','score','dist','len','trajectory','frame','x','y','z')
  data <- as.data.frame(data)
  data$trajectory <- as.numeric(data$trajectory)
  data$frame <- as.numeric(data$frame)
  data$x <- as.numeric(data$x)
  data$y <- as.numeric(data$y)
  data$z <- as.numeric(data$z)
  return(data)  
}

flysim <- readFlySim('data/2016-11-11 12-20-41_Cortex.flysim.csv')
#flysim <- readFlySim('data/2016-11-11 12-20-41_Cortex.flysim.2.csv')
#flysim <- readFlySim('data/flysim.csv')
flysim.original <- flysim

# Compute onsets
flysim <- ddply(flysim, c("trajectory"), function(X) data.frame(X, onset=min(X$frame)))
onsets <- unique(flysim$onset)

o = 20; a <- all[all$frame > onsets[o] & all$frame < onsets[o]+200,]; plot3d(a$x, a$y, a$z)

# Filter noise outside bounds
flysim <- flysim[flysim$x > -500 & flysim$x < 500 &
                 flysim$y > -500 & flysim$y < 500 & 
                 flysim$z > -100 & flysim$x < 800, ]

# Filter out small trajectories (i.e. noise)
flysim <- ddply(flysim, c("trajectory"), function(X) data.frame(X, num.frames=length(unique(X$frame))))
flysim <- flysim[flysim$num.frames > 20 & flysim$num.frames < 2000,]
f <- flysim
plot3d(f$x, f$y, f$z, col=f$trajectory)

f <- flysim
f <- flysim[flysim$dist > 500,]
f <- flysim[flysim$len  > 5e4,]
f <- flysim[flysim$len  > 5e4 & flysim$dist > 500,]
plot3d(f$x, f$y, f$z, col=f$trajectory)

u = unique(f$trajectory);
length(u)
f <- flysim[flysim$trajectory==u[20],]; plot3d(f$x, f$y, f$z)


flysim$score <- as.numeric(flysim$score)

hist(flysim$num.frames)
hist(flysim$score)
hist(flysim$dist)
hist(flysim$len)

hist(f$dist)
