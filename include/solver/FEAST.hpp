#pragma once
#include "support/config.hpp"
#include "solver/Solver.hpp"

#ifdef TBM_USE_FEAST
namespace tbm {

struct FEASTConfig {
    // required user config
    float energy_min = 0; ///< lowest eigenvalue
    float energy_max = 0; ///< highest eigenvalue
    int initial_size_guess = 0; ///< initial user guess for the subspace size

    // optional user config
    bool is_verbose = false; ///< [false] print information directly to stdout
    bool recycle_subspace = false; ///< [false] use previous data as a starting point

    // advanced optional user config
    int contour_points = 8; ///< [8] complex integral contour point
    int max_refinement_loops = 5; ///< [20] maximum number of refinement loops
    int sp_stop_criteria = 3; ///< [5] single precision error trace stopping criteria
    int dp_stop_criteria = 10; ///< [12] double precision error trace stopping criteria
    bool residual_convergence = false; /**< [false] use residual stop criteria
                                           instead of error trace criteria */

    // implementation detail config
    char matrix_format = 'F'; ///<  full matrix 'F' or triangular: lower 'L' and upper 'U'
    int system_size = 0; ///< size of the Hamiltonian matrix, i.e. number of atoms in the system
};

/**
 Implementation of the FEASTStrategy eigensolver.
 */
template<typename scalar_t>
class FEASTStrategy : public SolverStrategyT<scalar_t> {
    using real_t = num::get_real_t<scalar_t>;
    using complex_t = num::get_complex_t<scalar_t>;

public:
    struct Info {
        int suggested_size; ///< post-calculation suggested subspace size
        int final_size; ///< final subspace size
        int refinement_loops = 0; ///< the number of refinement loops executed
        real_t error_trace; ///< relative error on trace
        real_t max_residual; ///< biggest residual
        int return_code; ///< function return information and error codes
        bool recycle_warning = false; ///< error with recycled subspace, the calculation was rerun
        int recycle_warning_loops = 0;  ///< total loop count including those reset after a warning
        bool size_warning = false; ///< the initial subspace size was too small
    };
    
public:
    explicit FEASTStrategy(const FEASTConfig& config) : config(config) {}

public: // overrides
    // map eigenvalues and wavefunctions to only expose results up to the usable subspace size
    virtual DenseURef eigenvalues() const override {
        return Map<const ArrayX<real_t>>(_eigenvalues.data(), info.final_size);
    }
    virtual DenseURef eigenvectors() const override {
        return Map<const ArrayXX<scalar_t>>(_eigenvectors.data(), _eigenvectors.rows(), info.final_size);
    }

    virtual void solve() override;
    virtual std::string report(bool shortform) const override;
    virtual void hamiltonian_changed() override;

private: // implementation
    void init_feast(); ///< initialize FEAST parameters
    void init_pardiso(); ///< initialize PARDISO (sparse linear solver) parameters
    void call_feast(); ///< setup and call FEAST solver
    void call_feast_impl(); ///< call for scalar_t specific solver
    void force_clear(); ///< clear eigenvalue, eigenvector and residual data

private:
    int	fpm[128]; ///< FEAST init parameters
    FEASTConfig config;
    Info info;
    ArrayX<real_t> residual; ///< relative residual

protected: // declared used inherited members (template class requirement)
    using SolverStrategyT<scalar_t>::_eigenvalues;
    using SolverStrategyT<scalar_t>::_eigenvectors;
    using SolverStrategyT<scalar_t>::hamiltonian;
};


/**
 FEAST eigensolver.
 */
class FEAST : public Solver {
public: // construction and configuration
    static constexpr auto defaults = FEASTConfig{};
    
    /**
     Find the eigenvalues and eigenvectors in the given energy range.
     
     @param energy_range Where to look for eigenvalues.
     @param initial_size_guess A guess for the number of eigenvalues in the energy range.
                               The optimal value should be 50% bigger than final subspace size.
     @param recycle_subspace Reuse previous results as initial data for the solver.
     @param is_verbose Activate FEAST solver info (prints directly to stdout).
     */
    FEAST(const std::shared_ptr<const Model>& model,
          std::pair<double, double> energy_range,
          int initial_size_guess,
          bool recycle_subspace = defaults.recycle_subspace,
          bool is_verbose = defaults.is_verbose,
          FEASTConfig const& c = {})
        : config(c) {
        config.energy_min = static_cast<float>(energy_range.first);
        config.energy_max = static_cast<float>(energy_range.second);
        config.initial_size_guess = initial_size_guess;
        config.recycle_subspace = recycle_subspace;
        config.is_verbose = is_verbose;

        set_model(model);
    }

    // required implementation
    virtual std::unique_ptr<SolverStrategy>
        create_strategy_for(const std::shared_ptr<const Hamiltonian>&) const override;

private:
    FEASTConfig config;
};

} // namespace tbm
#endif // TBM_USE_FEAST
