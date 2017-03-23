

library(rgl)
library(parallel)
library(data.table)
library(plyr)
library(twiddler)
library(bit64)
library(ggplot2)

setwd('Z:/people/Abel/perching-analysis/')
base.path <- 'Z:/people/Abel/perching-analysis/data/'

###
#takeoffs <- read.csv('data/2016-11-11 12-20-41_Cortex.takeoffs.csv')
takeoffs <- read.csv('data/2016-11-15 16-49-11_Cortex.takeoffs.csv')
#colnames(takeoffs) <- c('frame','startframe','time','bboxsize','upward','a','b','c','r2')
takeoffs$upward <- ifelse(as.character(takeoffs$upward)=="True",TRUE,FALSE)

### Perches
perches <- read.csv('data/2016-11-07 12-51-23_Cortex.perches.csv')

###
#filePattern <- '2016-11-11 12-20-41_Cortex.msgpack.csv'
filePattern <- '2016-11-15 16-49-11_Cortex.tracking.csv'
t1 <- loadData( base.path, filePattern )
#, 
#                c('frame','trajectory','timestamp','time','x','y','z'))
t1 <- ddply(t1, c("trajectory"), function(X) data.frame(X, num.frames=length(unique(X$frame))))
t1 <- t1[!is.na(t1$x * t1$y * t1$z),]
t1 <- t1[t1$z > -100,]
t1$afterTakeoff <- sapply(t1$frame, function(x){
  return(sum(
    x > takeoffs$frame & #[takeoffs$upward] & 
    x < (takeoffs$frame+2000) # [takeoffs$upward]
  )>0)
})

f <- t1[t1$afterTakeoff,]
plot3d(f$x,f$y,f$z,col=f$trajectory,size=1)

#################################################
### Plot misc. statistics
#################################################

hist(takeoffs$bboxsize)

#################################################
### Plot small trajectories 
#################################################

f <- t1
f <- t1[t1$num.frames > 50,]

plot3d(f$x,f$y,f$z,col='black',size=1)

plot3d(f$x,f$y,f$z,col=f$trajectory,size=1)

fAll <- function(idxTrajectory) {
  a <- f[f$trajectory==unique(f$trajectory)[idxTrajectory],]
  rgl.clear()
  plot3d(a$x, a$y, a$z)
}
open3d()
twiddle(
  fAll(idxTrajectory),
  idxTrajectory = knob(c(1, length(unique(f$trajectory))),1),
  auto = FALSE)

#################################################
### Plot frames not 1000 ms (200 frames) after a takeoff
#################################################

t2 <- t1[afterTakeoff,]
plot3d(t2$x,t2$y,t2$z,col='black',size=1)
t3 <- t1[!afterTakeoff,]
plot3d(t3$x,t3$y,t3$z,col='red',size=1)
points3d(t2$x,t2$y,t2$z,col='black',size=1)

### Plot trajectories
fAll <- function(idxTakeoff, frameRangeAfter, frameRangeBefore) {
  
  #tf <- takeoffs[takeoffs$bboxsize > 400,]
  tf <- takeoffs[takeoffs$upward,]
  tf <- tf$frame[idxTakeoff]
  a <- t1[t1$frame > tf-frameRangeBefore & t1$frame < tf+frameRangeAfter,]  
#          t1$trajectory = tf$trajectory,]
  
  rgl.clear()
  plot3d(a$x, a$y, a$z, col=a$trajectory)
  
  #print(a[1,1:4])
}

open3d()
twiddle(
  fAll(idxTakeoff, frameRangeAfter, frameRangeBefore),
  idxTakeoff = knob(c(1, nrow(takeoffs)),1),
  frameRangeAfter  = knob(c(1, 1000), 1),
  frameRangeBefore = knob(c(1, 1000), 1),
  auto = FALSE)
  

#################################################
### Plot frames right after flysim
#################################################

trials     <- read.csv('data/2016-11-15 16-48-54.trials.csv')
trials$num <- 1:nrow(trials)

takeoffs <- read.csv('data/2016-11-15 16-49-11_Cortex.takeoffs.csv')

tracking <- loadData( base.path, 
    '2016-11-15 16-49-11_Cortex.tracking.csv', 
    c('frame','trajectory','timestamp','time','x','y','z'))
tracking$timestamp <- as.double(tracking$timestamp)

tracking$afterFlysim <- sapply(tracking$timestamp, function(x){
  d <- (x-(trials$timestamp-1000)); return(min(d[d>=0]))
})

tracking$trial <- sapply(tracking$timestamp, function(x){
  d <- (x-(trials$timestamp-1000)); d[d<0] <- 1e9
  i <- which(d==min(d))[1]; return(i)
})

t <- tracking[tracking$afterFlysim < 2000,]
plot3d(t$x, t$y, t$z, col=t$trial,size=1)

#d <- cbind( 
#  t$x[1:(nrow(t)-1)], t$x[2:nrow(t)], 
#  t$y[1:(nrow(t)-1)], t$y[2:nrow(t)], 
#  t$z[1:(nrow(t)-1)], t$z[2:nrow(t)])
#d <- d[t$trial[1:(nrow(t)-1)]==t$trial[2:nrow(t)],]
#segments3d(
#  d[,c(1,2)],
#  d[,c(3,4)],
#  d[,c(5,6)],col=t$trial,size=1)

# ================================================
# Print basic takeoff statistics
# ================================================

print("Number of takeoffs: ")
nrow(takeoffs[takeoffs$upward,])




fAll <- function(idxTakeoff, frameRangeAfter, frameRangeBefore) {
  a <- tracking[tracking$upward,]
  a <- a[a$trajectory==unique(a$trajectory)[idxTakeoff],]
  a <- a[1:frameRangeAfter,]
  rgl.clear()
  plot3d(a$x, a$y, a$z, col=a$trajectory)
}
open3d()
twiddle(
  fAll(idxTakeoff, frameRangeAfter, frameRangeBefore),
  idxTakeoff = knob(c(1, length(unique(tracking$trajectory[is.na(tracking$upward)]))),1),
  frameRangeAfter  = knob(c(1, 1000), 1),
  frameRangeBefore = knob(c(1, 1000), 1),
  auto = FALSE)

# ==============================================================

dbg <- read.csv('data/2016-11-14 14-09-32_Cortex.fsdbg.csv', header=FALSE)
colnames(dbg) <- c('framestart','frame','trajectory','i','d','x','y','z')
dbg$trajectory <- as.numeric(as.character(dbg$trajectory))

dbg <- aggregate(dbg, by=list(trajectory=dbg$trajectory, i=dbg$i), FUN=mean)

plot(dbg$d[dbg$trajectory==4])
dbg$i
dbg$d

ggplot(dbg, aes(x=i,y=d)) + geom_line() + facet_wrap( ~ trajectory)

plot3d(t1$x, t1$y, t1$z)


