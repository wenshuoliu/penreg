





## Introduction

Based on the following package: https://github.com/yixuan/ADMM

Install using the **devtools** package:

```r
devtools::install_github("jaredhuling/penreg")
```

or by cloning and building 

## Goals

* have capabilities for a wide range of penalties (lasso, group lasso, generalized lasso, overlapping group lasso, ridge) and combinations of them

* have capabilities for multiple models with any of the above penalties (gaussian, binomial, cox PH, etc)

* have capabilities for wide and tall data as well as sparse and dense matrices

* have some sort of unified interface to the fitting functions

* make the code as easily generalizable as possible

## Current Problems

* How to best organize code for ADMM algorithm fo fitting multiple penalties at once?

* How to best organize code for fitting different models with penalties. linear models are simple, but what about binomial and other nonlinear models. Is ADMM best or should we use Newton iterations with ADMM on the inside fitting weighted penalize least squares models

* How to best incorporate sparse design matrices (ie when x itself is sparse). Standardization of the data cannot be used for this case, because it will ruin the sparsity of the matrix

## Immediate to-do list

* Incorporate weighting of observations for lasso and genlasso

* Incorporate a penalty factor multiplier which is a vector multiplied by the tuning parameter value so that individual variables can have their penalty upweighted, downweighted, or not penalized at all. Could also be used for adaptive lasso, etc

* Make automatic rho choice better in terms of convergence

* Add code to make rho possibly update every 40th iteration

* CHANGE C++ VARIABLE NAMING CONVENTIONS (so the code is more readable)

* After the above are complete, work on group lasso then overlapping group lasso

## Code Structure

Each routine (ie lasso, group lasso, genlasso, etc) should have the following files:

* **fitting_function.R** This function (see admm.lasso, admm.genlasso) is a wrapper for the c++ functions. This should return a class related to the routine type (ie class = "admm.genlasso"). This function should have the proper roxygen description commented directly above its definition. Make sure to use **@export** in order for it to be included in the namespace. When building the package in rstudio, make sure roxygen is used.

* **predict.R** This function will be used for predicting based off of a model created from the above file. There should be one predict function for each fitting_function.R for example if admm.genlasso.R returns an object of type "admm.genlasso", then a function called predict.admm.genlasso.R should be created

* possibly more .R functions for each class (plot, summary, etc)

* **fitting_function.cpp** This file will contain a function to be directly called by R. examples include lasso.cpp and genlasso.cpp. It's primary job is to set up the data and then call the appropriate solver for each value of the tuning parameter

* **fitting_function_tall.h** This file is the routine which solves the optimization problem for data with nrow(x) > ncol(x) with admm (or even potentially some other algorithm). Typically with ADMM, high dimensional settings and low dimensional settings should be implemented differently for efficiency. 

* **fitting_function_wide.h** Same as the above but for high dimensional settings

* **fitting_function_sparse.cpp** and **fitting_function_tall_sparse.h** and **fitting_function_wide_sparse.h** Unless I figure out how to handle matrix types generically, when the x matrix itself is sparse I think code must be treated differently. ie we would need to set the data variable to be an Eigen::SparseMatrix<double> instead of an Eigen::MatrixXd. It is possible this can be handled with a generic type and then maybe we don't need extra files for sparse matrices.

The following files are more general:

* **DataStd.h** this centers/standardizes the data and provides code to recover the coefficients after fittin

* **FADMMBase.h** and/or **ADMMBase.h** these files set up the general class structure for admm problems. One of these two files will generally be used by the **fitting.h** files, for example, **FADMMBase.h** is used by **ADMMLassoTall.h** and **ADMMGenLassoTall.h** and **ADMMBase.h** is used by **ADMMLassoWide.h**. These files set up a class structure which is quite general. They **may** need to be modified in the future for problems that involve Newton-type iterations

## Models

### Lasso

```r
library(glmnet)
library(penreg)
set.seed(123)
n <- 100
p <- 20
m <- 5
b <- matrix(c(runif(m), rep(0, p - m)))
x <- matrix(rnorm(n * p, mean = 1.2, sd = 2), n, p)
y <- 5 + x %*% b + rnorm(n)

fit         <- glmnet(x, y)
glmnet_coef <- coef(fit)[,floor(length(fit$lambda/2))]
admm.res    <- admm.lasso(x, y, standardize = TRUE, intercept = TRUE)
admm_coef   <- admm.res$beta[,floor(length(fit$lambda/2))]

data.frame(glmnet = as.numeric(glmnet_coef),
           admm = as.numeric(admm_coef))
```

```
##          glmnet         admm
## 1   4.834825143  4.834735039
## 2   0.228618029  0.228583677
## 3   0.756501047  0.756511096
## 4   0.358817744  0.358804998
## 5   0.913800321  0.913823158
## 6   0.915407255  0.915382516
## 7   0.088653244  0.088682434
## 8   0.061140996  0.061197137
## 9   0.010692602  0.010686573
## 10  0.097466364  0.097516580
## 11  0.078353614  0.078398380
## 12 -0.045029900 -0.045067442
## 13 -0.069797582 -0.069846894
## 14  0.014974835  0.014971714
## 15  0.120619724  0.120626022
## 16 -0.037505776 -0.037514091
## 17  0.015485915  0.015507724
## 18  0.042620239  0.042638073
## 19 -0.017761281 -0.017798186
## 20 -0.027074881 -0.027043449
## 21 -0.006174798 -0.006179665
```

## Performance

### Lasso


```r
library(microbenchmark)
library(penreg)
library(glmnet)
# compute the full solution path, n > p
set.seed(123)
n <- 10000
p <- 500
m <- 50
b <- matrix(c(runif(m), rep(0, p - m)))
x <- matrix(rnorm(n * p, sd = 3), n, p)
y <- x %*% b + rnorm(n)

lambdas = glmnet(x, y)$lambda

microbenchmark(
    "glmnet[lasso]" = {res1 <- glmnet(x, y, thresh = 1e-10)}, # thresh must be very low for glmnet to be accurate
    "admm[lasso]"   = {res2 <- admm.lasso(x, y, lambda = lambdas, 
                                          intercept = TRUE, standardize = TRUE,
                                          abs.tol = 1e-8, rel.tol = 1e-8)},
    times = 5
)
```

```
## Unit: milliseconds
##           expr      min       lq     mean   median       uq      max neval
##  glmnet[lasso] 877.3851 924.8473 927.9810 934.3748 937.4798 965.8181     5
##    admm[lasso] 644.4346 648.0484 669.1382 656.2429 679.4984 717.4669     5
##  cld
##    b
##   a
```

```r
# difference of results
max(abs(coef(res1) - res2$beta))
```

```
## [1] 6.600234e-07
```

```r
mean(abs(coef(res1) - res2$beta))
```

```
## [1] 5.104183e-09
```

```r
# p > n
set.seed(123)
n <- 1000
p <- 2000
m <- 100
b <- matrix(c(runif(m), rep(0, p - m)))
x <- matrix(rnorm(n * p, sd = 2), n, p)
y <- x %*% b + rnorm(n)

lambdas = glmnet(x, y)$lambda

microbenchmark(
    "glmnet[lasso]" = {res1 <- glmnet(x, y, thresh = 1e-14)},
    "admm[lasso]"   = {res2 <- admm.lasso(x, y, lambda = lambdas, 
                                          intercept = TRUE, standardize = TRUE,
                                          abs.tol = 1e-9, rel.tol = 1e-9)},
    times = 5
)
```

```
## Unit: milliseconds
##           expr       min        lq      mean    median        uq       max
##  glmnet[lasso]  817.5585  818.0046  826.8458  825.4685  828.7929  844.4044
##    admm[lasso] 2314.5491 2315.1116 2325.6514 2328.0003 2330.2637 2340.3324
##  neval cld
##      5  a 
##      5   b
```

```r
# difference of results
max(abs(coef(res1) - res2$beta))
```

```
## [1] 1.307248e-06
```

```r
mean(abs(coef(res1) - res2$beta))
```

```
## [1] 5.714507e-09
```

```r
# p >> n
# ADMM is clearly not well-suited for this setting
set.seed(123)
n <- 100
p <- 2000
m <- 10
b <- matrix(c(runif(m), rep(0, p - m)))
x <- matrix(rnorm(n * p, sd = 2), n, p)
y <- x %*% b + rnorm(n)

lambdas = glmnet(x, y)$lambda

microbenchmark(
    "glmnet[lasso]" = {res1 <- glmnet(x, y, thresh = 1e-12)},
    "admm[lasso]"   = {res2 <- admm.lasso(x, y, lambda = lambdas, 
                                          intercept = TRUE, standardize = TRUE,
                                          abs.tol = 1e-9, rel.tol = 1e-9)},
    times = 5
)
```

```
## Unit: milliseconds
##           expr       min        lq      mean    median        uq       max
##  glmnet[lasso]  131.9178  136.7224  136.5627  136.7292  137.9485  139.4954
##    admm[lasso] 8347.6163 8358.6213 8443.3099 8378.4209 8509.5887 8622.3022
##  neval cld
##      5  a 
##      5   b
```

```r
# difference of results
max(abs(coef(res1) - res2$beta))
```

```
## [1] 0.0001883393
```

```r
mean(abs(coef(res1) - res2$beta))
```

```
## [1] 4.384709e-07
```


## Just to make sure I know how to push this up to github. 