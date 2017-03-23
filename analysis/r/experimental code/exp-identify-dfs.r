
###################
### IMPORTS
###################

library(ggplot2)
library(rgl)
library(data.table)
library(plyr)
library(reshape2)

# Helper functions
rep.row<-function(x,n){
  matrix(rep(x,each=n),nrow=n)
}
rep.col<-function(x,n){
  matrix(rep(x,each=n), ncol=n, byrow=TRUE)
}

###################
### GLOBAL SETTINGS
###################

setwd('Z:/people/Abel/perching-analysis/')

dn <- '2016-10-26 15-14-16'
dn <- '2016-11-04 15-10-25'

###################
### 
###################

readUnIDed <- function(fname) {
  data <- fread(fname,sep=',')
  colnames(data) <- c('trajectory','frame','yframe.x','yframe.y','yframe.z','x','y','z')
  data <- as.data.frame(data)
  data$trajectory <- as.numeric(data$trajectory)
  data$frame <- as.numeric(data$frame)
  data$x <- as.numeric(data$x)
  data$y <- as.numeric(data$y)
  data$z <- as.numeric(data$z)
  return(data)  
}

d1 <- readUnIDed('test/2016-10-22 18-17-13_Cortex.csv')
d2 <- readUnIDed('test/2016-10-27 15-05-35_Cortex.csv')

l <- as.numeric(sapply(1:nrow(d2), FUN=function(i){
  return(sqrt(sum(d2[i,c('x','y','z')]^2)))}))
d <- d2[10 < l & l<50.0,] # & data$frame==48898,]

plot3d(d$x,d$y,d$z)

# ====================================
# Draw a plot connecting the points in the same frame
# ====================================

# Add index of point within (trajectory, frame) pairs
d2g <- ddply(d2, c("trajectory", "frame"), function(X) data.frame(X, id=1:nrow(X)))

d <- d2g[1:100,]

segments3d(x=as.vector(t( cbind(d$x[d$id==1], d$x[d$id==2]) )),
           y=as.vector(t( cbind(d$y[d$id==1], d$y[d$id==2]) )),
           z=as.vector(t( cbind(d$z[d$id==1], d$z[d$id==2]) )))

# ====================================
# average number of markers per frame?
# ====================================

hist(aggregate(list(n=rep(1, nrow(data))), by=list(data$frame), FUN=sum)$n)

# ====================================
# Average distance between markers
# ====================================

#fDists <- 'testold/2016-10-26 15-14-16_Cortex.dists.csv'
#fDists <- 'test/2016-10-19 12-59-24_Cortex.dists.10mm.csv'
#fDists <- 'test/2016-10-19 12-59-24_Cortex.dists.5mm.csv'
#fDists <- 'test/2016-10-19 12-59-24_Cortex.dists.csv'

fDists <- paste0('test/',dn,'_Cortex.dists.csv')

dists <- as.data.frame(fread(fDists, sep=','))
colnames(dists) <- c('trajectory','frame','yframe.x',
                     'yframe.y','yframe.z',
                     'vx1','vy1','vz1','vx2','vy2','vz2',
                     'x1','y1','z1','x2','y2','z2','dist')

hist(dists$dist[dists$dist < 50], main=fDists)

# Keep point pairs with known distance (5 mm, 10 mm)
#l <- apply(data[,c('x','y','z')], 1, function(x){return(sqrt(sum(x,2)^2))})
#d <- data[l<30 & data$frame==48898,]

distsg <- ddply(dists , c("trajectory", "frame"), function(X) data.frame(X, id=1:nrow(X)))
distsg <- ddply(distsg, c("trajectory", "frame"), function(X) data.frame(X, numids=rep(nrow(X), nrow(X))))
distsg <- ddply(distsg, c("trajectory"), function(X) data.frame(X, num.frames=length(unique(X$frame))))
distsg <- ddply(distsg, c("trajectory"), function(X) data.frame(X, max.x=max(X$yframe.x)))
distsg <- ddply(distsg, c("trajectory"), function(X) data.frame(X, min.x=min(X$yframe.x)))
distsg <- ddply(distsg, c("trajectory"), function(X) data.frame(X, span.x=X$max.x-X$min.x))

diff <- distsg[,c('x1','y1','z1')] - distsg[,c('x2','y2','z2')]
distsg$dist.r <- as.numeric(sapply(1:nrow(diff), FUN=function(i){
  return(sqrt(sum(diff[i,]^2)))}))
#d <- distsg[abs(distsg$dist-10) < 6,]

d <- distsg[
  abs(abs(distsg$x1-distsg$x2) - 0) < 15 & 
  abs(abs(distsg$y1-distsg$y2) - 0) < 15 & 
  abs(abs(distsg$z1-distsg$z2) - 0) < 15 ,]

# Option 0: Filter short trajectories? (i.e. often noise)
d <- d[d$num.frames > 100,]

# Option 0.b: Filter trajectories that didn't move (stable noise points)
d <- d[d$span.x > 50,]
dyf <- yframes[yframes$span.x > 50,]

# Option 0a: Display position of unIDed-containing frames
points3d(d$yframe.x, d$yframe.y, d$yframe.z, col='green')

# Option 0b: Display all 3 yframe coords
segments3d(x=as.vector(t( cbind(dyf$vx1, dyf$vx2) )),
           y=as.vector(t( cbind(dyf$vy1, dyf$vy2) )),
           z=as.vector(t( cbind(dyf$vz1, dyf$vz2) )), col='blue') 
segments3d(x=as.vector(t( cbind(dyf$vx2, dyf$vx3) )),
           y=as.vector(t( cbind(dyf$vy2, dyf$vy3) )),
           z=as.vector(t( cbind(dyf$vz2, dyf$vz3) )), col='blue') 
segments3d(x=as.vector(t( cbind(dyf$vx3, dyf$vx1) )),
           y=as.vector(t( cbind(dyf$vy3, dyf$vy1) )),
           z=as.vector(t( cbind(dyf$vz3, dyf$vz1) )), col='blue') 

# Option 1b: Plot all in black for maximum clarity with dist comparison
plot3d(dyf$yframe.x, dyf$yframe.y, dyf$yframe.z, col='black')
# Option 2: Plot trajectory id as color
plot3d(dyf$yframe.x, dyf$yframe.y, dyf$yframe.z, col=dyf$trajectory)

# Display connector
segments3d(x=as.vector(t( cbind(d$x1+d$yframe.x, d$x2+d$yframe.x) )),
           y=as.vector(t( cbind(d$y1+d$yframe.y, d$y2+d$yframe.y) )),
           z=as.vector(t( cbind(d$z1+d$yframe.z, d$z2+d$yframe.z) )), col='red') 

# Option 3: Display segment length
text3d(
  d$x1+d$yframe.x,
  d$y1+d$yframe.y,
  d$z1+d$yframe.z, round(d$dist.r), col=d$trajectory)

# Separate plot
# segments3d(x=as.vector(t( cbind(d$x1, d$x2) )),
#            y=as.vector(t( cbind(d$y1, d$y2) )),
#            z=as.vector(t( cbind(d$z1, d$z2) )), col='blue') 

# ====================================
# Now group the data by trajectory
# ====================================

# Compute norm column
diff <- distsg[,c('x1','y1','z1')] - distsg[,c('x2','y2','z2')]
distsga <- distsg
distsga$norm <- as.numeric(sapply(1:nrow(diff), FUN=function(i){
  return(sqrt(sum(diff[i,]^2)))}))
d <- distsga[distsga$norm > 4 & distsga$norm < 15,]
d$dist <- as.numeric(d$dist)

da <- aggregate(list(dist.mean=d$dist), by=list(trajectory=d$trajectory), FUN=mean)
da$dist.sd <- aggregate(d$dist, by=list(d$trajectory), FUN=sd)[,2]
da$dist.sd[is.na(da$dist.sd)] <- 0

dodge <- position_dodge(width = 0.9)
limits <- aes(ymax = da$dist.mean + da$dist.sd,
              ymin = da$dist.mean - da$dist.sd)

ggplot(data = da, aes(x = trajectory, y = dist.mean, color='grey')) + 
  geom_errorbar(limits, position = dodge, width = 0.25) +
  geom_point(aes(color='black')) + 
  theme(axis.text.x=element_blank(), axis.ticks.x=element_blank(),
        axis.title.x=element_blank())

# ====================================
# Now pick some random trajectories and display distribution of distances
# Also display the trajectory itself
# ====================================

tids <- unique(d$trajectory); tids

tid <- 9657
dt  <- distsg[distsg$trajectory==tid,]
plot3d(dt$yframe.x, dt$yframe.y, dt$yframe.z)

dt$yframe.x

# Plot clean trajectory set (filter small ones)

d <- distsg[
  abs(abs(distsg$x1-distsg$x2) - 0) < 15 & 
  abs(abs(distsg$y1-distsg$y2) - 0) < 15 & 
  abs(abs(distsg$z1-distsg$z2) - 0) < 15 ,]

plot3d(d$yframe.x, d$yframe.y, d$yframe.z, col='red')

# Option 1: Plot all in black for maximum clarity with dist comparison
points3d(yframes$yframe.x, yframes$yframe.y, yframes$yframe.z, col='black')
# Option 2: Plot trajectory id as color
points3d(yframes$yframe.x, yframes$yframe.y, yframes$yframe.z, col=yframes$trajectory)

segments3d(x=as.vector(t( cbind(d$x1+d$yframe.x, d$x2+d$yframe.x) )),
           y=as.vector(t( cbind(d$y1+d$yframe.y, d$y2+d$yframe.y) )),
           z=as.vector(t( cbind(d$z1+d$yframe.z, d$z2+d$yframe.z) )), col='blue') 

# ====================================
# =======     Plot yframes     =======
# ====================================

yframes <- as.data.frame(
  fread(paste0('test/',dn,'_Cortex.yframe.csv'),sep=','))
colnames(yframes) <- c(
   'trajectory','frame',
   'yframe.x','yframe.y','yframe.z',
   'vx1','vy1','vz1','vx2','vy2','vz2','vx3','vy3','vz3',
   'x1','y1','z1','x2','y2','z2','x3','y3','z3')

yframes <- ddply(yframes, c("trajectory"), function(X) data.frame(X, max.x=max(X$yframe.x)))
yframes <- ddply(yframes, c("trajectory"), function(X) data.frame(X, min.x=min(X$yframe.x)))
yframes <- ddply(yframes, c("trajectory"), function(X) data.frame(X, span.x=X$max.x-X$min.x))

plot3d(yframes$yframe.x, yframes$yframe.y, yframes$yframe.z)

# ====================================
# Option 1: Plot projected Yframes
# ====================================

mx <- 100
yf <- yframes[
 abs(yframes$x1) < mx & abs(yframes$x2) < mx & abs(yframes$x3) < mx &
 abs(yframes$y1) < mx & abs(yframes$y2) < mx & abs(yframes$y3) < mx &
 abs(yframes$z1) < mx & abs(yframes$z2) < mx & abs(yframes$z3) < mx ,]

 # ====================================
 # Option 2: Plot projected Yframes
 # ====================================
 
yfp <- yframes[,6:14] - rep.col(as.matrix(yframes[,3:5]), 3)
mx <- 25
yf <- cbind(yframes$trajectory,yframes$frame, yfp)[
 abs(yfp$vx1) < mx & abs(yfp$vx2) < mx & abs(yfp$vx3) < mx &
 abs(yfp$vy1) < mx & abs(yfp$vy2) < mx & abs(yfp$vy3) < mx &
 abs(yfp$vz1) < mx & abs(yfp$vz2) < mx & abs(yfp$vz3) < mx ,]
colnames(yf) <- c('trajectory','frame',
    'x1','y1','z1','x2','y2','z2','x3','y3','z3')

# ====================================
# Continue yframe plots (preceeded by either option 1 or 2)
# ====================================

yf <- yf[1:10,]

plot3d(
  rbind(yf$x1, yf$x2, yf$x3),
  rbind(yf$y1, yf$y2, yf$y3),
  rbind(yf$z1, yf$z2, yf$z3)
)

text3d(yf$x1,yf$y1,yf$z1,'  1')
text3d(yf$x2,yf$y2,yf$z2,'  2')
text3d(yf$x3,yf$y3,yf$z3,'  3')

segments3d(x=as.vector(t(yf[,c('x1','x2')])),
           y=as.vector(t(yf[,c('y1','y2')])),
           z=as.vector(t(yf[,c('z1','z2')])))

segments3d(x=as.vector(t(yf[,c('x2','x3')])),
           y=as.vector(t(yf[,c('y2','y3')])),
           z=as.vector(t(yf[,c('z2','z3')])))

segments3d(x=as.vector(t(yf[,c('x3','x1')])),
           y=as.vector(t(yf[,c('y3','y1')])),
           z=as.vector(t(yf[,c('z3','z1')])))

