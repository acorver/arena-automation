
# ============================================================
# Import commonly used libraries
# ============================================================

library(rgl)
library(parallel)
library(data.table)
library(dplyr)
library(twiddler)
library(bit64)
library(ggplot2)

# Change the working directory for easy access to data files
setwd('Z:/people/Abel/arena-automation/data')

# ============================================================
# 
# ============================================================

fname <- "Z:/people/Abel/arena-automation/data/2017-04-20 14-08-04/2017-04-20 14-08-04.unids.csv"

data <- fread(fname)

colnames(data) <- c('frame','vertex','x','y','z')

d <- data[0:(nrow(data)/20),]

d <- data[data$x > 400  & data$x < 600 & 
          data$y > 50   & data$y < 150 & 
          data$z > -500 & data$z < 500,]
points3d(
  d$x,
  d$y,
  d$z,
  size=1)

plot3d(
  d$x,
  d$y,
  d$z,
  size=1)

points3d(
  perches$x,
  perches$y,
  perches$z,
  size=1,
  c='red')

fAll <- function(frameStart, frameNum) {
  
  tmp <- d[d$frame > frameStart & d$frame < (frameStart + frameNum),]
  
  #tmp <- tmp[tmp$x > 400 & tmp$y > 50 & tmp$y < 100 & 
  #             tmp$z > -100 & tmp$z < 100 &  
  #             tmp$frame < (frameStart + frameNum),]
  
  rgl.clear()
  plot3d(
    tmp$x,
    tmp$y,
    tmp$z,
    size=12, 
    xlim=c(400,600),
    ylim=c(50,150),
    zlim=c(-500,500),
    c='red')
  #points3d(
  #  d$x,
  #  d$y,
  #  d$z,
  #  size=1)
}
open3d()
twiddle(
  fAll(frameStart, frameNum),
  frameStart = knob(c(min(d$frame), max(d$frame),1)),
  frameNum = knob(c(1, 200,1)),
  auto = FALSE)
