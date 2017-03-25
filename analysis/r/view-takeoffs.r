
# Import commonly used libraries
library(rgl)
library(parallel)
library(data.table)
library(plyr)
library(dplyr)
library(twiddler)
library(bit64)
library(ggplot2)

# Change the working directory for easy access to data files
setwd('Z:/people/Abel/arena-automation/data')

# The data file base name to process
filebase <- '2017-03-22 15-17-55'

# Create filenames
f.takeoffs <- paste0(filebase, '.takeoffs.csv')
f.perches  <- paste0(filebase, '.perches.csv')
f.flysim   <- paste0(filebase, '.flysim.csv')
f.flysim.t <- paste0(filebase, '.flysim.tracking.csv')

# Load files
flysim   <- fread(f.flysim)
flysim.t <- fread(f.flysim.t)

flysim.t <- left_join(flysim.t, flysim, by=c("trajectory"="flysimTraj"))

# ============================================================
# Display takeoff trajectories
# ============================================================



# ============================================================
# Display FlySim trajectories
# ============================================================

f <- flysim.t[flysim.t$is_flysim,]
plot3d(
  f$x,
  f$y,
  f$z,
  col=f$trajectory,
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
    col=f$trajectory,
    size=1)
}
open3d()
twiddle(
  plt(id),
  id = knob(c(1, length(fsIDs)),1),
  auto = FALSE)