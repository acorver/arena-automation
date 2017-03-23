
# Import commonly used libraries
library(rgl)
library(parallel)
library(data.table)
library(plyr)
library(twiddler)
library(bit64)
library(ggplot2)

# Change the working directory for easy access to data files
setwd('Z:/people/Abel/arena-automation/data')

# Load data
takeoffs <- read.csv('data/2016-11-15 16-49-11_Cortex.takeoffs.csv')
perches  <- read.csv('data/2016-11-07 12-51-23_Cortex.perches.csv')
tracking <- NULL

# Plot all trajectories
plot3d(
  tracking$x,
  tracking$y,
  tracking$z,
  col=tracking$trajectory,
  size=1)



