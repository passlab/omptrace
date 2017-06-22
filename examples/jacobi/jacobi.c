#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <omp.h>
// Add timing support
#include <sys/timeb.h>

#define REAL float

static double read_timer_ms() {
    struct timeb tm;
    ftime(&tm);
    return (double) tm.time * 1000.0 + (double) tm.millitm;
}

/************************************************************
 * program to solve a finite difference
 * discretization of Helmholtz equation :
 * (d2/dx2)u + (d2/dy2)u - alpha u = f
 * using Jacobi iterative method.
 *
 * Modified: Sanjiv Shah,       Kuck and Associates, Inc. (KAI), 1998
 * Author:   Joseph Robicheaux, Kuck and Associates, Inc. (KAI), 1998
 *
 * This c version program is translated by
 * Chunhua Liao, University of Houston, Jan, 2005
 *
 * Directives are used in this code to achieve parallelism.
 * All do loops are parallelized with default 'static' scheduling.
 *
 * Input :  n - grid dimension in x direction
 *          m - grid dimension in y direction
 *          alpha - Helmholtz constant (always greater than 0.0)
 *          tol   - error tolerance for iterative solver
 *          relax - Successice over relaxation parameter
 *          mits  - Maximum iterations for iterative solver
 *
 * On output
 *       : u(n,m) - Dependent variable (solutions)
 *       : f(n,m) - Right hand side function
 *************************************************************/

#define DEFAULT_DIMSIZE 256

void print_array(char *title, char *name, REAL *A, int n, int m) {
    printf("%s:\n", title);
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < m; j++) {
            printf("%s[%d][%d]:%f  ", name, i, j, A[i * m + j]);
        }
        printf("\n");
    }
    printf("\n");
}

/*      subroutine initialize (n,m,alpha,dx,dy,u,f)
 ******************************************************
 * Initializes data
 * Assumes exact solution is u(x,y) = (1-x^2)*(1-y^2)
 *
 ******************************************************/
void initialize(int n, int m, REAL alpha, REAL *dx, REAL *dy, REAL *u_p, REAL *f_p) {
    int i;
    int j;
    int xx;
    int yy;
    REAL (*u)[m] = (REAL (*)[m]) u_p;
    REAL (*f)[m] = (REAL (*)[m]) f_p;

//double PI=3.1415926;
    *dx = (2.0 / (n - 1));
    *dy = (2.0 / (m - 1));
    /* Initialize initial condition and RHS */
//#pragma omp parallel for private(xx,yy,j,i)
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++) {
            xx = ((int) (-1.0 + (*dx * (i - 1))));
            yy = ((int) (-1.0 + (*dy * (j - 1))));
            u[i][j] = 0.0;
            f[i][j] = (((((-1.0 * alpha) * (1.0 - (xx * xx)))
                         * (1.0 - (yy * yy))) - (2.0 * (1.0 - (xx * xx))))
                       - (2.0 * (1.0 - (yy * yy))));
        }
}

/*  subroutine error_check (n,m,alpha,dx,dy,u,f)
 implicit none
 ************************************************************
 * Checks error between numerical and exact solution
 *
 ************************************************************/
void error_check(int n, int m, REAL alpha, REAL dx, REAL dy, REAL *u_p, REAL *f_p) {
    int i;
    int j;
    REAL xx;
    REAL yy;
    REAL temp;
    REAL error;
    error = 0.0;
    REAL (*u)[m] = (REAL (*)[m]) u_p;
    REAL (*f)[m] = (REAL (*)[m]) f_p;
//#pragma omp parallel for private(xx,yy,temp,j,i) reduction(+:error)
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++) {
            xx = (-1.0 + (dx * (i - 1)));
            yy = (-1.0 + (dy * (j - 1)));
            temp = (u[i][j] - ((1.0 - (xx * xx)) * (1.0 - (yy * yy))));
            error = (error + (temp * temp));
        }
    error = (sqrt(error) / (n * m));
    printf("Solution Error: %2.6g\n", error);
}

void jacobi_seq(int n, int m, REAL dx, REAL dy, REAL alpha, REAL relax, REAL *u_p, REAL *f_p, REAL tol, int mits);

void jacobi_omp(int n, int m, REAL dx, REAL dy, REAL alpha, REAL relax, REAL *u_p, REAL *f_p, REAL tol, int mits);

int main(int argc, char *argv[]) {
    int n = DEFAULT_DIMSIZE;
    int m = DEFAULT_DIMSIZE;
    REAL alpha = 0.0543;
    REAL tol = 0.0000000001;
    REAL relax = 1.0;
    int mits = 5000;

    fprintf(stderr, "Usage: jacobi [<n> <m> <alpha> <tol> <relax> <mits>]\n");
    fprintf(stderr, "\tn - grid dimension in x direction, default: %d\n", n);
    fprintf(stderr, "\tm - grid dimension in y direction, default: n if provided or %d\n", m);
    fprintf(stderr, "\talpha - Helmholtz constant (always greater than 0.0), default: %g\n", alpha);
    fprintf(stderr, "\ttol   - error tolerance for iterative solver, default: %g\n", tol);
    fprintf(stderr, "\trelax - Successice over relaxation parameter, default: %g\n", relax);
    fprintf(stderr, "\tmits  - Maximum iterations for iterative solver, default: %d\n", mits);

    if (argc == 2) {
        sscanf(argv[1], "%d", &n);
        m = n;
    }
    else if (argc == 3) {
        sscanf(argv[1], "%d", &n);
        sscanf(argv[2], "%d", &m);
    }
    else if (argc == 4) {
        sscanf(argv[1], "%d", &n);
        sscanf(argv[2], "%d", &m);
        sscanf(argv[3], "%g", &alpha);
    }
    else if (argc == 5) {
        sscanf(argv[1], "%d", &n);
        sscanf(argv[2], "%d", &m);
        sscanf(argv[3], "%g", &alpha);
        sscanf(argv[4], "%g", &tol);
    }
    else if (argc == 6) {
        sscanf(argv[1], "%d", &n);
        sscanf(argv[2], "%d", &m);
        sscanf(argv[3], "%g", &alpha);
        sscanf(argv[4], "%g", &tol);
        sscanf(argv[5], "%g", &relax);
    }
    else if (argc == 7) {
        sscanf(argv[1], "%d", &n);
        sscanf(argv[2], "%d", &m);
        sscanf(argv[3], "%g", &alpha);
        sscanf(argv[4], "%g", &tol);
        sscanf(argv[5], "%g", &relax);
        sscanf(argv[6], "%d", &mits);
    }
    else {
        /* the rest of arg ignored */
    }
    printf("jacobi %d %d %g %g %g %d\n", n, m, alpha, tol, relax, mits);
    printf("------------------------------------------------------------------------------------------------------\n");
    /** init the array */
    REAL *u = (REAL *) malloc(sizeof(REAL) * n * m);
    REAL *uomp = (REAL *) malloc(sizeof(REAL) * n * m);
    REAL *f = (REAL *) malloc(sizeof(REAL) * n * m);

    REAL dx; /* grid spacing in x direction */
    REAL dy; /* grid spacing in y direction */

    initialize(n, m, alpha, &dx, &dy, u, f);
    memcpy(uomp, u, sizeof(REAL) * n * m);

    REAL elapsed = read_timer_ms();
    jacobi_seq(n, m, dx, dy, alpha, relax, u, f, tol, mits);
    elapsed = read_timer_ms() - elapsed;
    printf("seq elasped time(ms): %12.6g\n", elapsed);
    double mflops = (0.001 * mits * (n - 2) * (m - 2) * 13) / elapsed;
    printf("MFLOPS: %12.6g\n", mflops);

#if CORRECTNESS_CHECK
    print_array("udev", "udev", (REAL*)udev, n, m);
#endif

    elapsed = read_timer_ms();
    jacobi_omp(n, m, dx, dy, alpha, relax, uomp, f, tol, mits);
    elapsed = read_timer_ms() - elapsed;
    int num_threads;
#pragma omp parallel
    {
#pragma omp single
        num_threads = omp_get_num_threads();
    }
    printf("OpenMP (%d threads) elasped time(ms): %12.6g\n", num_threads, elapsed);
    mflops = (0.001 * mits * (n - 2) * (m - 2) * 13) / elapsed;
    printf("MFLOPS: %12.6g\n", mflops);

    //print_array("Sequential Run", "u",(REAL*)u, n, m);

    error_check(n, m, alpha, dx, dy, u, f);
    free(u);
    free(f);
    free(uomp);

    return 0;
}

/*      subroutine jacobi (n,m,dx,dy,alpha,omega,u,f,tol,mits)
 ******************************************************************
 * Subroutine HelmholtzJ
 * Solves poisson equation on rectangular grid assuming :
 * (1) Uniform discretization in each direction, and
 * (2) Dirichlect boundary conditions
 *
 * Jacobi method is used in this routine
 *
 * Input : n,m   Number of grid points in the X/Y directions
 *         dx,dy Grid spacing in the X/Y directions
 *         alpha Helmholtz eqn. coefficient
 *         omega Relaxation factor
 *         f(n,m) Right hand side function
 *         u(n,m) Dependent variable/Solution
 *         tol    Tolerance for iterative solver
 *         mits  Maximum number of iterations
 *
 * Output : u(n,m) - Solution
 *****************************************************************/
void jacobi_seq(int n, int m, REAL dx, REAL dy, REAL alpha, REAL omega, REAL *u_p, REAL *f_p, REAL tol, int mits) {
    int i, j, k;
    REAL error;
    REAL ax;
    REAL ay;
    REAL b;
    REAL resid;
    REAL uold[n][m];
    REAL (*u)[m] = (REAL (*)[m]) u_p;
    REAL (*f)[m] = (REAL (*)[m]) f_p;
    /*
     * Initialize coefficients */
    /* X-direction coef */
    ax = (1.0 / (dx * dx));
    /* Y-direction coef */
    ay = (1.0 / (dy * dy));
    /* Central coeff */
    b = (((-2.0 / (dx * dx)) - (2.0 / (dy * dy))) - alpha);
    error = (10.0 * tol);
    k = 1;
    while ((k <= mits) && (error > tol)) {
        error = 0.0;

        /* Copy new solution into old */
        for (i = 0; i < n; i++)
            for (j = 0; j < m; j++)
                uold[i][j] = u[i][j];

        for (i = 1; i < (n - 1); i++)
            for (j = 1; j < (m - 1); j++) {
                resid = (ax * (uold[i - 1][j] + uold[i + 1][j]) + ay * (uold[i][j - 1] + uold[i][j + 1]) +
                         b * uold[i][j] - f[i][j]) / b;
                //printf("i: %d, j: %d, resid: %f\n", i, j, resid);

                u[i][j] = uold[i][j] - omega * resid;
                error = error + resid * resid;
            }
        /* Error check */
        if (k % 500 == 0)
            printf("Finished %d iteration with error: %g\n", k, error);
        error = sqrt(error) / (n * m);

        k = k + 1;
    } /*  End iteration loop */
    printf("Total Number of Iterations: %d\n", k);
    printf("Residual: %.15g\n", error);
}

void jacobi_omp(int n, int m, REAL dx, REAL dy, REAL alpha, REAL omega, REAL *u_p, REAL *f_p, REAL tol, int mits) {
    int i, j, k;
    REAL error;
    REAL ax;
    REAL ay;
    REAL b;
    REAL resid;
    REAL uold[n][m];
    REAL (*u)[m] = (REAL (*)[m]) u_p;
    REAL (*f)[m] = (REAL (*)[m]) f_p;
    /*
     * Initialize coefficients */
    /* X-direction coef */
    ax = (1.0 / (dx * dx));
    /* Y-direction coef */
    ay = (1.0 / (dy * dy));
    /* Central coeff */
    b = (((-2.0 / (dx * dx)) - (2.0 / (dy * dy))) - alpha);
    error = (10.0 * tol);
    k = 1;
    while ((k <= mits) && (error > tol)) {
        error = 0.0;

        /* Copy new solution into old */
#pragma omp parallel for private(j,i) shared(m,n,uold,u)
        for (i = 0; i < n; i++)
            for (j = 0; j < m; j++)
                uold[i][j] = u[i][j];

#pragma omp parallel for private(j,i, resid) reduction(+:error)
        for (i = 1; i < (n - 1); i++)
            for (j = 1; j < (m - 1); j++) {
                resid = (ax * (uold[i - 1][j] + uold[i + 1][j]) + ay * (uold[i][j - 1] + uold[i][j + 1]) +
                         b * uold[i][j] - f[i][j]) / b;
                //printf("i: %d, j: %d, resid: %f\n", i, j, resid);

                u[i][j] = uold[i][j] - omega * resid;
                error = error + resid * resid;
            }
        /* Error check */
        if (k % 500 == 0)
            printf("Finished %d iteration with error: %g\n", k, error);
        error = sqrt(error) / (n * m);

        k = k + 1;
    } /*  End iteration loop */
    printf("Total Number of Iterations: %d\n", k);
    printf("Residual: %.15g\n", error);
}

