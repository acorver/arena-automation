


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
  files = list.files(".")
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
filebase <- '2017-03-(22|21).*'

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
  col=(f$trajectory+f$dataset*1e6),
  size=1)

# ============================================================
# Investigate trajectories classified as non-Flysim
# ============================================================

fsIDs <- unique(flysim.t[!flysim.t$is_flysim,]$trajectory)
plt <- function(id) {
  f <- f[f$trajectory==fsIDs[id],]
  plot3d(
    f$x,
    f$y,
    f$z,
    col=(f$trajectory+f$dataset*1e6),
    size=1)
}
open3d()
twiddle(
  plt(id),
  id = knob(c(1, length(fsIDs)),1),
  auto = FALSE)