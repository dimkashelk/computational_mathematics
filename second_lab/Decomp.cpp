#include "Decomp.h"

#include <stdexcept>
#include <cmath>
#include "Solve.h"

int dimkashelk::details::decomp(int n, int ndim,
                                double *a, double *cond,
                                int pivot[], int *flag)

/* Purpose ...
   -------
   Decomposes a real matrix by gaussian elimination
   and estimates the condition of the matrix.

   Use Solve to compute solutions to linear systems.

   Input ...
   -----
   n    = order of the matrix
   ndim = row dimension of matrix as defined in the calling program
   *a   = pointer to matrix to be triangularized

   Output ...
   ------
   *a          pointer to  an upper triangular matrix U and a
           permuted version of a lower triangular matrix I-L
           so that
           (permutation matrix) * a = L * U
   cond      = an estimate of the condition of a .
           For the linear system a * x = b, changes in a and b
           may cause changes cond times as large in x.
           If cond+1.0 .eq. cond , a is singular to working
           precision, cond is set to 1.0e+32 if exact (or near)
           singularity is detected.
   pivot     = the pivot vector.
   pivot[k]  = the index of the k-th pivot row
   pivot[n-1]= (-1)**(number of interchanges)
   flag      = Status indicator
               0 : successful execution
               1 : could not allocate memory for workspace
               2 : illegal user input n < 1, a == NULL,
                   pivot == NULL, n > ndim.
               3 : matrix is singular

   Work Space ...
   ----------
   The vector work[0..n] is allocated internally by decomp().

   This C code written by ...  Peter & Nigel,
   ----------------------      Design Software,
                               42 Gubberley St,
                               Kenmore, 4069,
                               Australia.

   Version  ... 1.1 ,  2-Dec-87
   -------      2.0 , 11-Feb-89  (pointer used for a)
                2.1 , 15-Apr-89  (work[] allocated internally)
                2.2 , 14-Aug-89  (fixed pivoting)
                2.3 , 3 -Sep-89  (face lift)
                3.0 , 30-Sep-89  (optimize for rowwise storage)

   Notes ...
   -----
   (1) Subscripts range from 0 through (ndim-1).

   (2) The determinant of a can be obtained on output by
       det(a) = pivot[n-1] * a[0][0] * a[1][1] * ... * a[n-1][n-1].

   (3) This routine has been adapted from that in the text
       G.E. Forsythe, M.A. Malcolm & C.B. Moler
       Computer Methods for Mathematical Computations.

   (4) Uses the functions fabs(), free() and malloc().
*/

{
    /* --- function decomp() --- */
    double EPSILON = 2.2e-16;
    double ek, t, pvt, anorm, ynorm, znorm;
    int i, j, k, m;
    double *pa, *pb; /* temporary pointers */
    double *work;

    *flag = 0;
    work = (double *) NULL;

    if (a == NULL || pivot == NULL || n < 1 || ndim < n) {
        *flag = 2;
        return (0);
    }

    pivot[n - 1] = 1;
    if (n == 1) {
        /* One element only */
        *cond = 1.0;
        if (*a == 0.0) {
            *cond = 1.0e+32; /* singular */
            *flag = 3;
            return (0);
        }
        return (0);
    }

    work = (double *) malloc(n * sizeof(double));
    if (work == NULL) {
        *flag = 1;
        return (0);
    }

    /* --- compute 1-norm of a --- */

    anorm = 0.0;
    for (j = 0; j < n; ++j) {
        t = 0.0;
        for (i = 0; i < n; ++i) t += fabs(a[(i * ndim + j)]);
        if (t > anorm) anorm = t;
    }

    /* Apply Gaussian elimination with partial pivoting. */

    for (k = 0; k < n - 1; ++k) {
        /* Find pivot and label as row m.
           This will be the element with largest magnitude in
           the lower part of the kth column. */
        m = k;
        pvt = fabs(a[(m * ndim + k)]);
        for (i = k + 1; i < n; ++i) {
            t = fabs(a[(i * ndim + k)]);
            if (t > pvt) {
                m = i;
                pvt = t;
            }
        }
        pivot[k] = m;
        pvt = a[(m * ndim + k)];

        if (m != k) {
            pivot[n - 1] = -pivot[n - 1];
            /* Interchange rows m and k for the lower partition. */
            for (j = k; j < n; ++j) {
                pa = a + (m * ndim + j);
                pb = a + (k * ndim + j);
                t = *pa;
                *pa = *pb;
                *pb = t;
            }
        }
        /* row k is now the pivot row */

        /* Bail out if pivot is too small */
        if (fabs(pvt) < anorm * EPSILON) {
            /* Singular or nearly singular */
            *cond = 1.0e+32;
            *flag = 3;
            goto DecompExit;
        }

        /* eliminate the lower matrix partition by rows
           and store the multipliers in the k sub-column */
        for (i = k + 1; i < n; ++i) {
            pa = a + (i * ndim + k); /* element to eliminate */
            t = -(*pa / pvt); /* compute multiplier   */
            *pa = t; /* store multiplier     */
            for (j = k + 1; j < n; ++j) /* eliminate i th row */
            {
                if (fabs(t) > anorm * EPSILON)
                    a[(i * ndim + j)] += a[(k * ndim + j)] * t;
            }
        }
    } /* End of Gaussian elimination. */

    /* cond = (1-norm of a)*(an estimate of 1-norm of a-inverse)
       estimate obtained by one step of inverse iteration for the
       small singular vector. This involves solving two systems
       of equations, (a-transpose)*y = e and a*z = y where e
       is a vector of +1 or -1 chosen to cause growth in y.
       estimate = (1-norm of z)/(1-norm of y)

       Solve (a-transpose)*y = e   */

    for (k = 0; k < n; ++k) {
        t = 0.0;
        if (k != 0) {
            for (i = 0; i < k; ++i) t += a[(i * ndim + k)] * work[i];
        }
        if (t < 0.0) ek = -1.0;
        else ek = 1.0;
        pa = a + (k * ndim + k);
        if (fabs(*pa) < anorm * EPSILON) {
            /* Singular */
            *cond = 1.0e+32;
            *flag = 3;
            goto DecompExit;
        }

        work[k] = -(ek + t) / *pa;
    }

    for (k = n - 2; k >= 0; --k) {
        t = 0.0;
        for (i = k + 1; i < n; i++)
            t += a[(i * ndim + k)] * work[i];
        /* we have used work[i] here, however the use of work[k]
       makes some difference to cond */
        work[k] = t;
        m = pivot[k];
        if (m != k) {
            t = work[m];
            work[m] = work[k];
            work[k] = t;
        }
    }

    ynorm = 0.0;
    for (i = 0; i < n; ++i) ynorm += fabs(work[i]);

    /* --- solve a * z = y */
    solve(n, ndim, a, work, pivot);

    znorm = 0.0;
    for (i = 0; i < n; ++i) znorm += fabs(work[i]);

    /* --- estimate condition --- */
    *cond = anorm * znorm / ynorm;
    if (*cond < 1.0) *cond = 1.0;
    if (*cond + 1.0 == *cond) *flag = 3;

DecompExit:
    if (work != NULL) {
        free(work);
        work = (double *) NULL;
    }
    return (0);
} /* --- end of function decomp() --- */

dimkashelk::Decomp::Decomp(): cond_(0.0),
                              size_(0),
                              data_(nullptr),
                              pivot_(nullptr),
                              flag_(0) {
}

void dimkashelk::Decomp::operator()(const std::vector<std::vector<double> > &matrix) {
    if (matrix.empty()) {
        throw std::logic_error("Check matrix");
    }
    if (matrix.size() != matrix[0].size()) {
        throw std::logic_error("Check size of matrix");
    }
    free();
    size_ = static_cast<int>(matrix.size());
    data_ = new double[size_ * size_];
    try {
        pivot_ = new int[matrix.size() * matrix.size()];
    } catch (...) {
        delete[] data_;
        throw;
    }
    int ind = 0;
    for (auto &i: matrix) {
        for (double j: i) {
            data_[ind] = j;
            ind++;
        }
    }
    details::decomp(size_, size_, data_, std::addressof(cond_), pivot_, std::addressof(flag_));
}

dimkashelk::Decomp::~Decomp() {
    free();
}

void dimkashelk::Decomp::free() const {
    if (data_ != nullptr) {
        delete[] data_;
    }
    if (pivot_ != nullptr) {
        delete[] pivot_;
    }
}
