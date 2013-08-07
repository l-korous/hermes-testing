#include "definitions.h"
#include "../../../testing-core/testing-core.h"

//#define SHOW_OUTPUT

// This example shows the use of subdomains. It models a round graphite object that is 
// heated through internal heat sources and cooled with a fluid (air or water) flowing 
// past it. This model is semi-realistic, double-check all parameter values and equations 
// before using it for your applications. NOTE: The file definitions.h contains numbers 
// that need to be compatible with the mesh file.

// If true, then just Stokes equation will be considered, not Navier-Stokes.
const bool STOKES = false;         
// If defined, pressure elements will be discontinuous (L2),
// otherwise continuous (H1).   
#define PRESSURE_IN_L2                           
// Initial polynomial degree for velocity components.
const int P_INIT_VEL = 2;          
// Initial polynomial degree for pressure.
// Note: P_INIT_VEL should always be greater than
// P_INIT_PRESSURE because of the inf-sup condition.
const int P_INIT_PRESSURE = 1;     
// Initial polynomial degree for temperature.
const int P_INIT_TEMPERATURE = 1;

// Initial uniform mesh refinements.
const int INIT_REF_NUM_TEMPERATURE_GRAPHITE = 2;
const int INIT_REF_NUM_TEMPERATURE_FLUID = 3;
const int INIT_REF_NUM_FLUID = 3;
const int INIT_REF_NUM_BDY_GRAPHITE = 1;   
const int INIT_REF_NUM_BDY_WALL = 1;   

// Problem parameters.
// Inlet velocity (reached after STARTUP_TIME).
const double VEL_INLET = 0.1;              
// Initial temperature.
const double TEMPERATURE_INIT_FLUID = 20.0;                       
const double TEMPERATURE_INIT_GRAPHITE = 100.0;                       
// Correct is 1.004e-6 (at 20 deg Celsius) but then RE = 2.81713e+06 which 
// is too much for this simple model, so we use a larger viscosity. Note
// that kinematic viscosity decreases with rising temperature.
const double KINEMATIC_VISCOSITY_FLUID = 15.68e-6;    // Water has 1.004e-2.
// We found a range of 25 - 470, but this is another 
// number that one needs to be careful about.
const double THERMAL_CONDUCTIVITY_GRAPHITE = 450;    
// At 25 deg Celsius.
const double THERMAL_CONDUCTIVITY_FLUID = 0.025;  // Water has 0.6.  
// Density of graphite from Wikipedia, one should be 
// careful about this number.    
const double RHO_GRAPHITE = 2220;                      
const double RHO_FLUID = 1.1839;    // Water has 1000.0.      
// Also found on Wikipedia.
const double SPECIFIC_HEAT_GRAPHITE = 711;        
const double SPECIFIC_HEAT_FLUID = 1012.0;     // Water has 4187.0         
// Heat source in graphite. This value is not realistic.
const double HEAT_SOURCE_GRAPHITE = 1e6;          
// During this time, inlet velocity increases gradually
// from 0 to VEL_INLET, then it stays constant.
const double STARTUP_TIME = 1.0;                  
// Time step.
const double time_step = 0.5;                     
// Time interval length.
//const double T_FINAL = 30000.0;
const double T_FINAL = time_step * 2;
// Stopping criterion for the Newton's method.
const double NEWTON_TOL = 1e-4;                   
// Maximum allowed number of Newton iterations.
const int NEWTON_MAX_ITER = 10;                   
// Matrix solver: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
// SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.
Hermes::MatrixSolverType matrix_solver = Hermes::SOLVER_UMFPACK;  

// Temperature advection treatment.
// true... velocity from previous time level is used in temperature 
//         equation (which makes it linear).
// false... full Newton's method is used.
bool SIMPLE_TEMPERATURE_ADVECTION = false; 

int main(int argc, char* argv[])
{
  // Load the mesh.
  MeshSharedPtr mesh_whole_domain(new Mesh), mesh_with_hole(new Mesh);
  Hermes::vector<MeshSharedPtr> meshes (mesh_whole_domain, mesh_with_hole);
  MeshReaderH2DXML mloader;
  mloader.load("domain.xml", meshes);

  // Temperature mesh: Initial uniform mesh refinements in graphite.
  meshes[0]->refine_by_criterion(element_in_graphite, INIT_REF_NUM_TEMPERATURE_GRAPHITE);

  // Temperature mesh: Initial uniform mesh refinements in fluid.
  meshes[0]->refine_by_criterion(element_in_fluid, INIT_REF_NUM_TEMPERATURE_FLUID);

  // Fluid mesh: Initial uniform mesh refinements.
  for(int i = 0; i < INIT_REF_NUM_FLUID; i++)
    meshes[1]->refine_all_elements();

  // Initial refinements towards boundary of graphite.
  for(unsigned int meshes_i = 0; meshes_i < meshes.size(); meshes_i++)
    meshes[meshes_i]->refine_towards_boundary("Inner Wall", INIT_REF_NUM_BDY_GRAPHITE);

  // Initial refinements towards the top and bottom edges.
  for(unsigned int meshes_i = 0; meshes_i < meshes.size(); meshes_i++)
    meshes[meshes_i]->refine_towards_boundary("Outer Wall", INIT_REF_NUM_BDY_WALL);

  // Initialize boundary conditions.
  EssentialBCNonConst bc_inlet_vel_x("Inlet", VEL_INLET, H, STARTUP_TIME);
  DefaultEssentialBCConst<double> bc_other_vel_x(Hermes::vector<std::string>("Outer Wall", "Inner Wall"), 0.0);
  EssentialBCs<double> bcs_vel_x(Hermes::vector<EssentialBoundaryCondition<double> *>(&bc_inlet_vel_x, &bc_other_vel_x));
  DefaultEssentialBCConst<double> bc_vel_y(Hermes::vector<std::string>("Inlet", "Outer Wall", "Inner Wall"), 0.0);
  EssentialBCs<double> bcs_vel_y(&bc_vel_y);
  EssentialBCs<double> bcs_pressure;
  DefaultEssentialBCConst<double> bc_temperature(Hermes::vector<std::string>("Outer Wall", "Inlet"), 20.0);
  EssentialBCs<double> bcs_temperature(&bc_temperature);

  // Spaces for velocity components, pressure and temperature.
  SpaceSharedPtr<double> xvel_space(new H1Space<double>(mesh_with_hole, &bcs_vel_x, P_INIT_VEL));
  SpaceSharedPtr<double> yvel_space(new H1Space<double>(mesh_with_hole, &bcs_vel_y, P_INIT_VEL));
#ifdef PRESSURE_IN_L2
  SpaceSharedPtr<double> p_space(new L2Space<double>(mesh_with_hole, P_INIT_PRESSURE));
#else
  SpaceSharedPtr<double> p_space(new H1Space<double>(mesh_with_hole, &bcs_pressure, P_INIT_PRESSURE));
#endif
  SpaceSharedPtr<double> temperature_space(new H1Space<double> (mesh_whole_domain, &bcs_temperature, P_INIT_TEMPERATURE));
  Hermes::vector<SpaceSharedPtr<double> > all_spaces(xvel_space, yvel_space, p_space, temperature_space);

  // Calculate and report the number of degrees of freedom.
  int ndof = Space<double>::get_num_dofs(Hermes::vector<SpaceSharedPtr<double> >(xvel_space, 
      yvel_space, p_space, temperature_space));

  // Define projection norms.
  NormType vel_proj_norm = HERMES_H1_NORM;
#ifdef PRESSURE_IN_L2
  NormType p_proj_norm = HERMES_L2_NORM;
#else
  NormType p_proj_norm = HERMES_H1_NORM;
#endif
  NormType temperature_proj_norm = HERMES_H1_NORM;
  Hermes::vector<NormType> all_proj_norms = Hermes::vector<NormType>(vel_proj_norm, 
      vel_proj_norm, p_proj_norm, temperature_proj_norm);

  // Initial conditions and such.
  //Hermes::Mixins::Loggable::Static::Hermes::Mixins::Loggable::Static::info("Setting initial conditions.");
  MeshFunctionSharedPtr<double> xvel_prev_time(new ZeroSolution<double>(mesh_with_hole)), yvel_prev_time(new ZeroSolution<double>(mesh_with_hole)), p_prev_time(new ZeroSolution<double>(mesh_with_hole));
  MeshFunctionSharedPtr<double> temperature_init_cond(new CustomInitialConditionTemperature (mesh_whole_domain, HOLE_MID_X, HOLE_MID_Y, 
      0.5*OBSTACLE_DIAMETER, TEMPERATURE_INIT_FLUID, TEMPERATURE_INIT_GRAPHITE)); 
  MeshFunctionSharedPtr<double> temperature_prev_time(new Solution<double>);
  Hermes::vector<MeshFunctionSharedPtr<double> > initial_solutions = Hermes::vector<MeshFunctionSharedPtr<double> >(xvel_prev_time, 
      yvel_prev_time, p_prev_time, temperature_init_cond);
  Hermes::vector<MeshFunctionSharedPtr<double> > all_solutions = Hermes::vector<MeshFunctionSharedPtr<double> >(xvel_prev_time, 
      yvel_prev_time, p_prev_time, temperature_prev_time);

  // Project all initial conditions on their FE spaces to obtain aninitial
  // coefficient vector for the Newton's method. We use local projection
  // to avoid oscillations in temperature on the graphite-fluid interface
  // FIXME - currently the LocalProjection only does the lowest-order part (linear
  // interpolation) at the moment. Higher-order part needs to be added.
  double* coeff_vec = new double[ndof];
  
  Hermes::Mixins::Loggable::Static::info("Projecting initial condition to obtain initial vector for the Newton's method.");
  OGProjection<double> ogProjection;
  ogProjection.project_global(all_spaces, initial_solutions, coeff_vec, all_proj_norms);

  // Translate the solution vector back to Solutions. This is needed to replace
  // the discontinuous initial condition for temperature_prev_time with its projection.
  Solution<double>::vector_to_solutions(coeff_vec, all_spaces, all_solutions);

  // Calculate Reynolds number.
  double reynolds_number = VEL_INLET * OBSTACLE_DIAMETER / KINEMATIC_VISCOSITY_FLUID;
  //Hermes::Mixins::Loggable::Static::Hermes::Mixins::Loggable::Static::info("RE = %g", reynolds_number);

  // Initialize weak formulation.
  CustomWeakFormHeatAndFlow wf(STOKES, reynolds_number, time_step, xvel_prev_time, yvel_prev_time, temperature_prev_time, 
      HEAT_SOURCE_GRAPHITE, SPECIFIC_HEAT_GRAPHITE, SPECIFIC_HEAT_FLUID, RHO_GRAPHITE, RHO_FLUID, 
      THERMAL_CONDUCTIVITY_GRAPHITE, THERMAL_CONDUCTIVITY_FLUID, SIMPLE_TEMPERATURE_ADVECTION);
  
  // Initialize the FE problem.
  DiscreteProblem<double> dp(&wf, all_spaces);

  // Initialize the Newton solver.
  NewtonSolver<double> newton(&dp);
  newton.set_max_steps_with_reused_jacobian(0);
  newton.set_tolerance(1e-6, ResidualNormAbsolute);

  // Initialize views.
  Views::VectorView vview("velocity [m/s]", new Views::WinGeom(0, 0, 700, 360));
  Views::ScalarView pview("pressure [Pa]", new Views::WinGeom(0, 415, 700, 350));
  Views::ScalarView tempview("temperature [C]", new Views::WinGeom(715, 0, 700, 350));
  //vview.set_min_max_range(0, 0.5);
  vview.fix_scale_width(80);
  //pview.set_min_max_range(-0.9, 1.0);
  pview.fix_scale_width(80);
  pview.show_mesh(false);
  tempview.fix_scale_width(80);
  tempview.show_mesh(false);

  // Time-stepping loop:
  char title[100];
  int num_time_steps = T_FINAL / time_step;
  double current_time = 0.0;
  for (int ts = 1; ts <= num_time_steps; ts++)
  {
    current_time += time_step;
    //Hermes::Mixins::Loggable::Static::Hermes::Mixins::Loggable::Static::info("---- Time step %d, time = %g:", ts, current_time);

    // Update time-dependent essential BCs.
    if (current_time <= STARTUP_TIME) 
    {
      //Hermes::Mixins::Loggable::Static::Hermes::Mixins::Loggable::Static::info("Updating time-dependent essential BC.");
      Space<double>::update_essential_bc_values(all_spaces, current_time);
    }

    // Perform Newton's iteration.
    //Hermes::Mixins::Loggable::Static::Hermes::Mixins::Loggable::Static::info("Solving nonlinear problem:");
    bool verbose = true;
    // Perform Newton's iteration and translate the resulting coefficient vector into previous time level solutions.
    newton.set_verbose_output(verbose);
    try
    {
      newton.solve(coeff_vec);
    }
    catch(Hermes::Exceptions::Exception e)
    {
      e.print_msg();
    };
    {
      Solution<double>::vector_to_solutions(newton.get_sln_vector(), all_spaces, all_solutions);
    }
    
    // Show the solution at the end of time step.
    vview.get_vectorizer()->process_solution(xvel_prev_time, yvel_prev_time, H2D_FN_VAL_0, H2D_FN_DX_0);
    pview.get_linearizer()->save_solution_vtk(p_prev_time, "pressure.vtk", "pressure");

#ifdef SHOW_OUTPUT
    sprintf(title, "Velocity [m/s], time %g s", current_time);
    vview.set_title(title);
    vview.show(xvel_prev_time, yvel_prev_time);
    sprintf(title, "Pressure [Pa], time %g s", current_time);
    pview.set_title(title);
    pview.show(p_prev_time);
    sprintf(title, "Temperature [C], time %g s", current_time);
    tempview.set_title(title);
    tempview.show(temperature_prev_time,  Views::HERMES_EPS_HIGH);
#endif
  }

  double norm = get_l2_norm(coeff_vec, Space<double>::get_num_dofs(all_spaces));

  bool success = Testing::test_value(norm, 578.49360112096122, "sln-norm", 1e-1);

  delete [] coeff_vec;

  // Wait for all views to be closed.
#ifdef SHOW_OUTPUT
  Views::View::wait();
#endif

  if(success)
  {
    printf("Success!\n");
    return 0;
  }
  else
  {
    printf("Failure!\n");
    return -1;
  }
  return 0;
}