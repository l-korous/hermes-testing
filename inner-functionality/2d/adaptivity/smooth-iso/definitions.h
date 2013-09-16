#include "hermes2d.h"

using namespace Hermes::Hermes2D;;
using namespace WeakFormsH1;
using Hermes::Ord;

/*  Exact solution */

class CustomExactSolution : public ExactSolutionScalar<double>
{
public:
  CustomExactSolution(MeshSharedPtr mesh) : ExactSolutionScalar<double>(mesh)
  {
  }

  ~CustomExactSolution()
  {
    this->a = 0;
  }

  virtual void derivatives(double x, double y, double& dx, double& dy) const;

  virtual double value(double x, double y) const;

  virtual Ord ord(double x, double y) const;

  virtual MeshFunction<double>* clone() const { return new CustomExactSolution(this->mesh); }

  int a;
};

/* Custom function f */

class CustomFunction : public Hermes::Hermes2DFunction<double>
{
public:
  CustomFunction() : Hermes::Hermes2DFunction<double>()
  {
  }

  virtual double value(double x, double y) const;

  virtual Ord value(Ord x, Ord y) const;
};
