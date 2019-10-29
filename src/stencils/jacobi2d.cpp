#include "yask_compiler_api.hpp"
using namespace std;
using namespace yask;

// Create an anonymous namespace to ensure that types are local.
namespace {

    class Jacobi2D : public yc_solution_base {

    public:
        Jacobi2D(const string& name) :
            yc_solution_base(name) { }

        Jacobi2D() :
            yc_solution_base("jacobi2d") { }

        // Define equation at t+1 based on values at t.
        virtual void define() {

            // Indices & dimensions.
            yc_index_node_ptr t = new_step_index("t");           // step in time dim.
            yc_index_node_ptr x = new_domain_index("x");         // spatial dim.
            yc_index_node_ptr y = new_domain_index("y");         // spatial dim.
            // Vars.
            yc_var_proxy u("U", get_soln(), {t, x, y});          // time-varying 2D var.

            // misc index and 1D array for coefficients
            // auto i = new_misc_index("i");
            // yc_var_proxy c("C", get_soln(), {i});

            // stencil definition
            // auto nu = c(0) * u(t, x, y);
            //
            // nu += c(1) * ( u(t, x - 1, y) + u(t, x + 1, y) + u(t, x, y + 1) + u(t, x, y - 1));

            u(t+1, x, y) EQUALS (u(t, x, y) + u(t, x - 1, y) + u(t, x + 1, y) + u(t, x, y + 1) + u(t, x, y - 1)) / 5; // define u(t+1)
            //
        }
    };

    // Create an object of type 'Jacobi2D',
    // making it available in the YASK compiler utility via the
    // '-stencil' commmand-line option or the 'stencil=' build option.
    static Jacobi2D Jacobi2D_instance;

} // namespace
