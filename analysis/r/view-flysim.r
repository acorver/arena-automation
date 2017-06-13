

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
# Load data
# ============================================================

fst <- fread('2017-05-25/2017-05-25.flysim.tracking.csv')

fs  <- fread('2017-05-25/2017-05-25.flysim.csv')

fst <- left_join(fst, fs, by=c("trajectory"="flysimTraj"))

# ============================================================
# Plot
# ============================================================

f <- fst[fst$,]
plot3d(
  f$x,
  f$y,
  f$z,
  col=(f$trajectory),
  size=1)

