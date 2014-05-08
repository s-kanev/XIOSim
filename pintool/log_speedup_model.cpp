/* Implementation of the logarithmic speedup model functions.
 *
 * Author: Sam Xi.
 */

#include <cmath>
#include <iostream>
#include <map>
#include <vector>

#include "speedup_models.h"

/* Returns a core allocation that optimizes total energy consumption for the
 * system. An analytical solution was not found in all cases but was found for a
 * certain subset of scenarios. We use those solutions as a baseline and perform
 * local optimization around those solutions to find the true global minimum.
 *
 * The two solved solutions are of the form n1 = B/W(B/A), where A = exp(1-1/c),
 * B = Pu/Pc, c is the logarithmic scaling constant, and Pu/Pc is the ratio of
 * uncore to core power; and n2 = A as defined above. W(t) is the principle
 * branch of the Lambert-W function. Ask Sam for more details about the
 * solutions...code documentation is not the right place for math proofs.
 *
 * See header file for function signature details.
 */
void LogSpeedupModel::OptimizeEnergy(
        std::map<int, int> &core_allocs,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime) {
    int n = core_allocs.size();
    double** candidate_solns = new double*[n];
    for (int i = 0; i < n; i++) {
        candidate_solns[i] = new double[n];
    }
    double power_ratio = uncore_power/core_power;
    // Compute all candidate solutions.
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double A = exp(-(1.0/process_scaling.at(j) - 1));
            if (i == j) {
                double w_val = LambertW(power_ratio/A);
                candidate_solns[i][j] = power_ratio/w_val;
            } else {
                candidate_solns[i][j] = A;
            }
        }
    }
    // Compute the total energy for each candidate core allocation solution to
    // find the minimum.
    int min_alloc = 0;
    for (int i = 0; i < n; i++) {
        double max_runtime = 0;
        double min_energy = -1;
        double current_energy = 0.0;
        for (int j = 0; j < n; j++) {
            double current_rt = ComputeRuntime(
                    candidate_solns[i][j],
                    process_scaling.at(j),
                    process_serial_runtime.at(j));
            if (current_rt > max_runtime)
                max_runtime = current_rt;
            current_energy += core_power * current_rt;
        }
        current_energy += uncore_power * max_runtime;
        if (min_energy == -1 || current_energy < min_energy) {
            min_energy = current_energy;
            min_alloc = i;
        }
    }
    // Build a core_allocs map of asid and core alloc key value pairs and
    // perform local minimization. Allocate at least one core.
    // TODO: I don't think we've guaranteed that we allocate less than num_cores
    // cores.
    for (int i = 0; i < n; i++) {
        double cores = candidate_solns[min_alloc][i];
        core_allocs[i] = int(ceil(cores));
    }
    MinimizeCoreAllocations(core_allocs,
                            process_scaling,
                            process_serial_runtime,
                            OptimizationTarget::ENERGY);

    for (int i = 0; i < n; i++)
        delete[] candidate_solns[i];
    delete[] candidate_solns;
}

/* The solution to this optimization problem has been published in Creech et al.
 * "Efficient multiprogramming for Multicores with SCAF", MICRO 2013. The
 * solution is: n_i = N * C_i / sum(Cj for all j), where C is the fitted scaling
 * parameter to the speedup equation S(n) = 1 + C*ln(n).
 */
void LogSpeedupModel::OptimizeThroughput(
        std::map<int, int> &core_allocs,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime) {
    double sum_scaling = 0;
    for (auto it = process_scaling.begin(); it != process_scaling.end(); ++it)
        sum_scaling += *it;

    for (size_t i = 0; i < process_scaling.size(); i++) {
        double cores = num_cores * process_scaling.at(i) / sum_scaling;
        // TODO: I think this should guarantee we never allocate more than
        // num_cores cores to begin with, but I can't be certain.
        core_allocs[i] = cores < 1 ? 1 : int(cores);
    }
    MinimizeCoreAllocations(
            core_allocs,
            process_scaling,
            process_serial_runtime,
            OptimizationTarget::THROUGHPUT);
}

/* Computes runtime for log scaling. */
double LogSpeedupModel::ComputeRuntime(
        int num_cores_alloc,
        double process_scaling,
        double process_serial_runtime) {
    return process_serial_runtime /
        (1 + process_scaling * log(num_cores_alloc));
}

/* Approximation of the LambertW function. */
double LogSpeedupModel::LambertW(const double z) {
    int i;
    const double eps=4.0e-16, em1=0.3678794411714423215955237701614608;
    double p,e,t,w;
    // if (dbgW) fprintf(stderr,"LambertW: z=%g\n",z);
    if (z<-em1 || isinf(z) || isnan(z)) {
        fprintf(stderr,"LambertW: bad argument %g, exiting.\n",z);
        exit(1);
    }
    if (0.0==z) return 0.0;
    if (z<-em1+1e-4) { // series near -em1 in sqrt(q)
        double q=z+em1,r=sqrt(q),q2=q*q,q3=q2*q;
        return
            -1.0
            +2.331643981597124203363536062168*r
            -1.812187885639363490240191647568*q
            +1.936631114492359755363277457668*r*q
            -2.353551201881614516821543561516*q2
            +3.066858901050631912893148922704*r*q2
            -4.175335600258177138854984177460*q3
            +5.858023729874774148815053846119*r*q3
            -8.401032217523977370984161688514*q3*q; // error approx 1e-16
    }
    /* initial approx for iteration... */
    if (z<1.0) { /* series near 0 */
        p=sqrt(2.0*(2.7182818284590452353602874713526625*z+1.0));
        w=-1.0+p*(1.0+p*(-0.333333333333333333333+p*0.152777777777777777777777));
    } else  {
        w=log(z); /* asymptotic */
    }
    if (z>3.0) w-=log(w); /* useful? */
    for (i=0; i<10; i++) { /* Halley iteration */
        e=exp(w);
        t=w*e-z;
        p=w+1.0;
        t/=e*p-0.5*(p+1.0)*t/p;
        w-=t;
        if (fabs(t)<eps*(1.0+fabs(w)))
            return w; /* rel-abs error */
    }
    /* should never get here */
    fprintf(stderr,"LambertW: No convergence at z=%g, exiting.\n",z);
    exit(1);
}
