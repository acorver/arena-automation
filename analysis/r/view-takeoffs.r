


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
# Helper functions
# ============================================================

# Function to merge multiple datasets
readall <- function(filename) {
  files = list.files(".", recursive=TRUE)
  files = files[grep(filename, files, perl=TRUE)]
  d <- lapply(seq_along(files), FUN=function(n){
    x <- fread(files[n])
    x$dataset <- n
    return(x)
  });
  return(as.data.frame(rbindlist(d)))
}

# ============================================================
# Load data
# ============================================================

# The data file base name to process
#filebase <- '2017-04-20 14-08-04/*'
filebase <- '2017-05-25/2017-05-25'

# Create filenames
f.takeoffs <- paste0(filebase, '.takeoffs.csv')
f.perches  <- paste0(filebase, '.perches.csv')
f.traj     <- paste0(filebase, '(?<!flysim).tracking.csv')
f.flysim   <- paste0(filebase, '.flysim.csv')
f.flysim.t <- paste0(filebase, '.flysim.tracking.csv')

# Load trajectory files
takeoffs <- readall(f.takeoffs)
traj     <- left_join(readall(f.traj), 
  takeoffs[,c("trajectory","flysimTraj")], 
  by=c("trajectory"="trajectory"))

# Load flysim files
flysim   <- readall(f.flysim)
flysim.t <- left_join(readall(f.flysim.t), 
  flysim, by=c("trajectory"="flysimTraj"))

# ============================================================
# Display takeoff trajectories
# ============================================================

f <- traj[traj$takeoffTraj!=-1,]

f[1:1000,]

plot3d(
  f$x,
  f$y,
  f$z,
  col=(f$trajectory+f$dataset*1e6),
  size=1)

# ============================================================
# Display takeoff trajectories after FlySim
# ============================================================

f <- traj[traj$flysimTraj!=-1 & traj$takeoffTraj!=-1,]
plot3d(
  f$x,
  f$y,
  f$z,
  col=(f$flysimTraj+f$dataset*1e6),
  size=1)

f <- left_join(takeoffs[takeoffs$flysimTraj!=-1,], 
   flysim.t, by=c("flysimTraj"="trajectory"))
points3d(
  f$x,
  f$y,
  f$z,
  col=(f$flysimTraj+f$dataset*1e6),
  size=1)

# ============================================================
# Display FlySim trajectories
# ============================================================

f <- flysim.t[flysim.t$is_flysim,]
plot3d(
  f$x,
  f$y,
  f$z,
  col=(f$trajectory+f$dataset.x*1e6),
  size=1)

# ============================================================
# Investigate trajectories classified as non-Flysim
# ============================================================

f <- flysim.t[!flysim.t$is_flysim,]
fsIDs <- unique(f$trajectory)
plt <- function(id) {
  f2 <- f[f$trajectory==fsIDs[id],]
  print(
    c(unique(f2$dir_ok), 
    unique(f2$len_ok), 
    unique(f2$dist_ok),
    unique(f2$std),
    unique(f2$distanceFromYframe)
    ))
  plot3d(
    f2$x,
    f2$y,
    f2$z,
    col=f2$trajectory + f2$dataset.x,
    size=1)
}
open3d()
twiddle(
  plt(id),
  id = knob(c(1, length(fsIDs)),1),
  auto = FALSE)



