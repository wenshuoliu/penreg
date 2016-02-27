#ifndef COORDLASSOTALL_H
#define COORDLASSOTALL_H

#include "CoordBase.h"
#include "Linalg/BlasWrapper.h"
#include "ADMMMatOp.h"
#include "utils.h"

// minimize  1/2 * ||y - X * beta||^2 + lambda * ||beta||_1
//
// In ADMM form,
//   minimize f(x) + g(z)
//   s.t. x - z = 0
//
// x => beta
// z => -X * beta
// A => X
// b => y
// f(x) => 1/2 * ||Ax - b||^2
// g(z) => lambda * ||z||_1
class CoordLasso: public CoordBase<Eigen::VectorXd> //Eigen::SparseVector<double>
{
protected:
    typedef float Scalar;
    typedef double Double;
    typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> Matrix;
    typedef Eigen::Matrix<double, Eigen::Dynamic, 1> Vector;
    typedef Eigen::Map<const Matrix> MapMat;
    typedef Eigen::Map<const Vector> MapVec;
    typedef const Eigen::Ref<const Matrix> ConstGenericMatrix;
    typedef const Eigen::Ref<const Vector> ConstGenericVector;
    typedef Eigen::SparseMatrix<double> SpMat;
    typedef Eigen::SparseVector<double> SparseVector;
    
    MapMat datX;                  // data matrix
    MapVec datY;                  // response vector
    Vector XY;                    // X'Y
    MatrixXd Xsq;                 // colSums(X^2)
    
    Scalar lambda;                // L1 penalty
    Scalar lambda0;               // minimum lambda to make coefficients all zero
    
    double threshval;
    VectorXd resid_cur;
    
    /*
    static void soft_threshold(SparseVector &res, const Vector &vec, const double &penalty)
    {
        int v_size = vec.size();
        res.setZero();
        res.reserve(v_size);
        
        const double *ptr = vec.data();
        for(int i = 0; i < v_size; i++)
        {
            if(ptr[i] > penalty)
                res.insertBack(i) = ptr[i] - penalty;
            else if(ptr[i] < -penalty)
                res.insertBack(i) = ptr[i] + penalty;
        }
    }
    */
    
    static double soft_threshold(double &value, const double &penalty)
    {
        if(value > penalty)
            return(value - penalty);
        else if(value < -penalty)
            return(value + penalty);
        else 
            return(0);
    }
    
    void update_grad()
    {
        
    }
    
    void next_beta(Vector &res)
    {
        
        int j;
        double grad;
        for (j = 0; j < nvars; ++j)
        {
            double beta_prev = beta(j);
            grad = datX.col(j).dot(resid_cur) / Xsq(j) + beta(j);
            
            threshval = soft_threshold(grad, lambda / Xsq(j));
            
            // update residual if the coefficient changes after
            // thresholding. 
            if (beta_prev != threshval)
            {
                beta(j) = threshval;
                resid_cur -= (threshval - beta_prev) * datX.col(j);
            }
            
        }
        
    }
    
    
    // Calculate ||v1 - v2||^2 when v1 and v2 are sparse
    static double diff_squared_norm(const SparseVector &v1, const SparseVector &v2)
    {
        const int n1 = v1.nonZeros(), n2 = v2.nonZeros();
        const double *v1_val = v1.valuePtr(), *v2_val = v2.valuePtr();
        const int *v1_ind = v1.innerIndexPtr(), *v2_ind = v2.innerIndexPtr();
        
        double r = 0.0;
        int i1 = 0, i2 = 0;
        while(i1 < n1 && i2 < n2)
        {
            if(v1_ind[i1] == v2_ind[i2])
            {
                double val = v1_val[i1] - v2_val[i2];
                r += val * val;
                i1++;
                i2++;
            } else if(v1_ind[i1] < v2_ind[i2]) {
                r += v1_val[i1] * v1_val[i1];
                i1++;
            } else {
                r += v2_val[i2] * v2_val[i2];
                i2++;
            }
        }
        while(i1 < n1)
        {
            r += v1_val[i1] * v1_val[i1];
            i1++;
        }
        while(i2 < n2)
        {
            r += v2_val[i2] * v2_val[i2];
            i2++;
        }
        
        return r;
    }
    
    
public:
    CoordLasso(ConstGenericMatrix &datX_, 
               ConstGenericVector &datY_,
               double tol_ = 1e-6) :
    CoordBase(datX_.rows(), datX_.cols(),
              tol_),
              datX(datX_.data(), datX_.rows(), datX_.cols()),
              datY(datY_.data(), datY_.size()),
              XY(datX.transpose() * datY),
              resid_cur(datY),  //assumes we start our beta estimate at 0
              Xsq(datX.array().square().colwise().sum()),
              lambda0(XY.cwiseAbs().maxCoeff())
    {}
    
    double get_lambda_zero() const { return lambda0; }
    
    // init() is a cold start for the first lambda
    void init(double lambda_)
    {
        beta.setZero();
        
        lambda = lambda_;
        
        threshval = 1; // just need to initialize with some nonzero value, it will be changed
    }
    // when computing for the next lambda, we can use the
    // current main_x, aux_z, dual_y and rho as initial values
    void init_warm(double lambda_)
    {
        lambda = lambda_;
        
        threshval = 1; // just need to initialize with some nonzero value, it will be changed
    }
};



#endif // COORDLASSOTALL_H
