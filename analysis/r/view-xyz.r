
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

# Load data
data <- as.data.frame(fread(
  "2017-04-20 14-08-04/2017-04-20 14-08-04.xyz.csv"))
colnames(data) <- c('data','file','body','frame','x','y','z')

# Subset data to only Yframes
yframes <- data[data$body=="b'Yframe'",]
perches <- data[data$body!="b'Yframe'",]
data <- NULL
gc()

# Plot
plot3d(
  yframes$x,
  yframes$y,
  yframes$z,
  size=1)

points3d(
  perches$x,
  perches$y,
  perches$z,
  size=1,
  c='red')

