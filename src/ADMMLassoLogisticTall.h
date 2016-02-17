#ifndef ADMMLASSOLOGISTICTALL_H
#define ADMMLASSOLOGISTICTALL_H

#include "FADMMBase.h"
#include "Linalg/BlasWrapper.h"
#include "Spectra/SymEigsSolver.h"
#include "ADMMMatOp.h"
#include "utils.h"
#include <Eigen/Geometry>

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
class ADMMLassoLogisticTall: public FADMMBase<Eigen::VectorXd, Eigen::SparseVector<double>, Eigen::VectorXd>
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
    typedef Eigen::LLT<Matrix> LLT;
    typedef Eigen::LDLT<Matrix> LDLT;
    
    MapMat datX;                  // data matrix
    MapVec datY;                  // response vector
    Vector XY;                    // X'Y
    MatrixXd XX;                  // X'X
    MatrixXd HH;                  // X'WX
    LDLT solver;                  // matrix factorization
    VectorXd savedEigs;           // saved eigenvalues
    bool rho_unspecified;         // was rho unspecified? if so, we must set it
    
    Scalar lambda;                // L1 penalty
    Scalar lambda0;               // minimum lambda to make coefficients all zero
    
    
    
    // x -> Ax
    void A_mult (Vector &res, Vector &x)  { res.swap(x); }
    // y -> A'y
    void At_mult(Vector &res, Vector &y)  { res.swap(y); }
    // z -> Bz
    void B_mult (Vector &res, SparseVector &z) { res = -z; }
    // ||c||_2
    double c_norm() { return 0.0; }
    
    
    
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
    void next_x(Vector &res)
    {
        
        res = main_x;
        //LDLT solver_logreg;
        //solver.compute((0.25 * XX).selfadjointView<Eigen::Lower>());
        int maxit_newton = 100;
        double tol_newton = 1e-5;
        
        for (int i = 0; i < maxit_newton; ++i)
        {
            // calculate gradient
            //VectorXd xbycur_exp( (-1 * ((datY.asDiagonal() * datX) * res).array()).array().exp() );
            //VectorXd xbycur         ( ((datY.asDiagonal() * datX) * res).matrix() );
            
            VectorXd prob = 1 / (1 + (-1 * (datX * res).array()).exp().array());
            
            //VectorXd grad( ((datY.asDiagonal() * datX).adjoint() * 
            //    (-1 * xbycur_exp.array() / (1 + xbycur_exp.array() ).array()).matrix()).array() + 
            //    adj_y.array() + rho * res.array());
            
            VectorXd grad = (-1 * XY.array()).array() + (datX.adjoint() * prob).array() + 
                adj_y.array() + (rho * res.array()).array();
            
            
            for(SparseVector::InnerIterator iter(adj_z); iter; ++iter)
                grad[iter.index()] -= rho * iter.value();
            
            //VectorXd xbycur_exp((-1 * xbycur.array()).array().exp());
            
            //calculate Jacobian
            //VectorXd w((xbycur_exp.array() / (1 + xbycur_exp.array()).array().square().array()).matrix() );
            VectorXd W = prob.array() * (1 - prob.array());
            //MatrixXd HH(XtWX(datY.asDiagonal() * datX, w));  //datY.asDiagonal() * datX
            HH = XtWX(datX, W);
            HH.diagonal().array() += rho;
            
            //res -= (0.15 * HH.ldlt().solve(grad).array()).matrix();
            VectorXd dx = HH.ldlt().solve(grad);
            res.noalias() -= dx;
            //std::cout << "cross:\n" << grad.adjoint() * dx << std::endl;
            if (std::abs(grad.adjoint() * dx) < tol_newton)
            {
                //std::cout << "iters:\n" << i+1 << std::endl;
                break;
            }
            //std::cout << "beta:\n" << res.head(5).adjoint() << std::endl;
        }
        
        //Vector rhs = XY - adj_y;
        // rhs += rho * adj_z;
        
        // manual optimization
        //for(SparseVector::InnerIterator iter(adj_z); iter; ++iter)
        //    rhs[iter.index()] += rho * iter.value();
        
        //res.noalias() = solver.solve(rhs);
    }
    virtual void next_z(SparseVector &res)
    {
        Vector vec = main_x + adj_y / rho;
        soft_threshold(res, vec, lambda / rho);
    }
    void next_residual(Vector &res)
    {
        // res = main_x;
        // res -= aux_z;
        
        // manual optimization
        std::copy(main_x.data(), main_x.data() + dim_main, res.data());
        for(SparseVector::InnerIterator iter(aux_z); iter; ++iter)
            res[iter.index()] -= iter.value();
    }
    void rho_changed_action() 
    {
        //MatrixXd matToSolve(XX);
        //matToSolve.diagonal().array() += rho;
        
        //// precompute LLT decomposition of (X'X + rho * I)
        //solver.compute(matToSolve.selfadjointView<Eigen::Lower>());
    }
    //void update_rho() {}
    
    
    
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
    
    // Faster computation of epsilons and residuals
    double compute_eps_primal()
    {
        double r = std::max(main_x.norm(), aux_z.norm());
        return r * eps_rel + std::sqrt(double(dim_dual)) * eps_abs;
    }
    double compute_eps_dual()
    {
        return dual_y.norm() * eps_rel + std::sqrt(double(dim_main)) * eps_abs;
    }
    double compute_resid_dual()
    {
        return rho * std::sqrt(diff_squared_norm(aux_z, old_z));
    }
    double compute_resid_combined()
    {
        // SparseVector tmp = aux_z - adj_z;
        // return rho * resid_primal * resid_primal + rho * tmp.squaredNorm();
        
        // manual optmization
        return rho * resid_primal * resid_primal + rho * diff_squared_norm(aux_z, adj_z);
    }
    
public:
    ADMMLassoLogisticTall(ConstGenericMatrix &datX_, ConstGenericVector &datY_,
                          double eps_abs_ = 1e-6,
                          double eps_rel_ = 1e-6) :
    FADMMBase(datX_.cols(), datX_.cols(), datX_.cols(),
              eps_abs_, eps_rel_),
              datX(datX_.data(), datX_.rows(), datX_.cols()),
              datY(datY_.data(), datY_.size()),
              XY(datX.transpose() * datY),
              XX(XtX(datX)),
              lambda0(XY.cwiseAbs().maxCoeff())
    {}
    
    double get_lambda_zero() const { return lambda0; }
    
    // init() is a cold start for the first lambda
    void init(double lambda_, double rho_)
    {
        main_x.setZero();
        aux_z.setZero();
        dual_y.setZero();
        
        adj_z.setZero();
        adj_y.setZero();
        
        lambda = lambda_;
        rho = rho_;
        
        //MatrixXd XX(XtX(datX));
        //Matrix XX;
        //Linalg::cross_prod_lower(XX, datX);
        
        if(rho <= 0)
        {
            rho_unspecified = true;
            MatOpSymLower<Double> op(XX);
            Spectra::SymEigsSolver< Double, Spectra::LARGEST_ALGE, MatOpSymLower<Double> > eigs(&op, 1, 3);
            srand(0);
            eigs.init();
            eigs.compute(100, 0.1);
            savedEigs = eigs.eigenvalues();
            rho = std::pow(savedEigs[0], 1.0 / 3) * std::pow(lambda, 2.0 / 3);
        } else 
        {
            rho_unspecified = false;
        }
        
        //XX.diagonal().array() += rho;
        
        //XX.diagonal().array() += rho;
        //solver.compute(XX.selfadjointView<Eigen::Lower>());
        
        eps_primal = 0.0;
        eps_dual = 0.0;
        resid_primal = 9999;
        resid_dual = 9999;
        
        adj_a = 1.0;
        adj_c = 9999;
        
        rho_changed_action();
    }
    // when computing for the next lambda, we can use the
    // current main_x, aux_z, dual_y and rho as initial values
    void init_warm(double lambda_)
    {
        lambda = lambda_;
        
        if (rho_unspecified)
        {
            MatOpSymLower<Double> op(XX);
            Spectra::SymEigsSolver< Double, Spectra::LARGEST_ALGE, MatOpSymLower<Double> > eigs(&op, 1, 3);
            srand(0);
            eigs.init();
            eigs.compute(100, 0.1);
            savedEigs = eigs.eigenvalues();
            rho = std::pow(savedEigs[0], 1.0 / 3) * std::pow(lambda, 2.0 / 3);
        }
        
        eps_primal = 0.0;
        eps_dual = 0.0;
        resid_primal = 9999;
        resid_dual = 9999;
        
        // adj_a = 1.0;
        // adj_c = 9999;
    }
};



#endif // ADMMLASSOLOGISTICTALL_H