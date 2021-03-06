
## Taken from Jerome Friedman, Trevor Hastie, Noah Simon, and Rob Tibshirani's package glmnet
## https://cran.r-project.org/web/packages/glmnet/index.html
lambda.interp=function(lambda,s){
    ### lambda is the index sequence that is produced by the model
    ### s is the new vector at which evaluations are required.
    ### the value is a vector of left and right indices, and a vector of fractions.
    ### the new values are interpolated bewteen the two using the fraction
    ### Note: lambda decreases. you take:
    ### sfrac*left+(1-sfrac*right)
    
    if(length(lambda)==1){# degenerate case of only one lambda
        nums=length(s)
        left=rep(1,nums)
        right=left
        sfrac=rep(1,nums)
    }
    else{
        s[s > max(lambda)] = max(lambda)
        s[s < min(lambda)] = min(lambda)
        k=length(lambda)
        sfrac <- (lambda[1]-s)/(lambda[1] - lambda[k])
        lambda <- (lambda[1] - lambda)/(lambda[1] - lambda[k])
        coord <- approx(lambda, seq(lambda), sfrac)$y
        left <- floor(coord)
        right <- ceiling(coord)
        sfrac=(sfrac-lambda[right])/(lambda[left] - lambda[right])
        sfrac[left==right]=1
    }
    list(left=left,right=right,frac=sfrac)
}

## Taken from Jerome Friedman, Trevor Hastie, Noah Simon, and Rob Tibshirani's package glmnet
## https://cran.r-project.org/web/packages/glmnet/index.html
nonzeroCoef = function (beta, bystep = FALSE) 
{
    ### bystep = FALSE means which variables were ever nonzero
    ### bystep = TRUE means which variables are nonzero for each step
    nr=nrow(beta)
    if (nr == 1) {#degenerate case
        if (bystep) 
            apply(beta, 2, function(x) if (abs(x) > 0) 
                1
                else NULL)
        else {
            if (any(abs(beta) > 0)) 
                1
            else NULL
        }
    }
    else {
        beta=abs(beta)>0 # this is sparse
        which=seq(nr)
        ones=rep(1,ncol(beta))
        nz=as.vector((beta%*%ones)>0)
        which=which[nz]
        if (bystep) {
            if(length(which)>0){
                beta=as.matrix(beta[which,,drop=FALSE])
                nzel = function(x, which) if (any(x)) 
                    which[x]
                else NULL
                which=apply(beta, 2, nzel, which)
                if(!is.list(which))which=data.frame(which)# apply can return a matrix!!
                which
            }
            else{
                dn=dimnames(beta)[[2]]
                which=vector("list",length(dn))
                names(which)=dn
                which
            }
            
        }
        else which
    }
}

## Taken from Rahul Mazumder, Trevor Hastie, and Jerome Friedman's sparsenet package
## https://cran.r-project.org/web/packages/sparsenet/index.html
argmin=function(x){
    vx=as.vector(x)
    imax=order(vx)[1]
    if(!is.matrix(x))imax
    else{
        d=dim(x)
        c1=as.vector(outer(seq(d[1]),rep(1,d[2])))[imax]
        c2=as.vector(outer(rep(1,d[1]),seq(d[2])))[imax]
        c(c1,c2)
        
    }
}
