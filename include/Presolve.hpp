// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#ifndef Presolve_H
#define Presolve_H
#include "ClpSimplex.hpp"

class PresolveAction;
class PresolveMatrix;
class PostsolveMatrix;

/** This class stores information generated by the presolve procedure.
 */
class Presolve {
public:
  /**@name Constructor and destructor 
     No copy method is defined.
     I have not attempted to prevent the default copy method from being
     generated.
   */
  //@{
  /// Default constructor
  Presolve();

  /// Virtual destructor
  virtual ~Presolve();
  //@}

  /**@name presolve - presolves a model, transforming the model
   * and saving information in the Presolve object needed for postsolving.
   * This is method is virtual; the idea is that in the future,
   * one could override this method to customize how the various
   * presolve techniques are applied.
   */
  virtual void presolve(ClpSimplex& si);

  /**@name postsolve - postsolve the problem.  The problem must
    have been solved to optimality.
   If you are using an algorithm like simplex that has a concept
   of "basic" rows/cols, then pass in two arrays
   that indicate which cols/rows are basic in the problem passed in (si);
   on return, they will indicate which rows/cols in the original
   problem are basic in the solution.
   These two arrays must have enough room for ncols/nrows in the
   *original* problem, not the problem being passed in.
   If you aren't interested in this information, or it doesn't make
   sense, then pass in 0 for each array.
  
   Note that if you modified the problem after presolving it,
   then you must ``undo'' these modifications before calling postsolve.
  */
  virtual void postsolve(ClpSimplex& si,
			 unsigned char *colstat,
			 unsigned char *rowstat);
  /** This version of presolve returns a pointer to a new presolved 
      model.  NULL if infeasible or unbounded.  
      This should be paired with postsolve
      below.  The adavantage of going back to original model is that it
      will be exactly as it was i.e. 0.0 will not become 1.0e-19.
      If keepIntegers is true then bounds may be tightened in
      original.  Bounds will be moved by up to feasibilityTolerance
      to try and stay feasible.
  */
  virtual ClpSimplex * presolvedModel(ClpSimplex & si,
				      double feasibilityTolerance=0.0,
				      bool keepIntegers=true,
				      int numberPasses=5);

  /** Return pointer to presolved model,
      Up to user to destroy */
  ClpSimplex * model() const;
  /// Return pointer to original model
  ClpSimplex * originalModel() const;
  /// return pointer to original columns
  const int * originalColumns() const;


  /** This version updates original*/
  virtual void postsolve(bool updateStatus=true);

  /**@name private or protected data */
private:
  /// Original model - must not be destroyed before postsolve
  ClpSimplex * originalModel_;

  /// Presolved model - up to user to destroy
  ClpSimplex * presolvedModel_;
  /// Original column numbers
  int * originalColumn_;
  /// The list of transformations applied.
  const PresolveAction *paction_;

  /// The postsolved problem will expand back to its former size
  /// as postsolve transformations are applied.
  /// It is efficient to allocate data structures for the final size
  /// of the problem rather than expand them as needed.
  /// These fields give the size of the original problem.
  int ncols_;
  int nrows_;
  int nelems_;
  /// Number of major passes
  int numberPasses_;

protected:
  /// If you want to apply the individual presolve routines differently,
  /// or perhaps add your own to the mix,
  /// define a derived class and override this method
  virtual const PresolveAction *presolve(PresolveMatrix *prob);

  /// Postsolving is pretty generic; just apply the transformations
  /// in reverse order.
  /// You will probably only be interested in overriding this method
  /// if you want to add code to test for consistency
  /// while debugging new presolve techniques.
  virtual void postsolve(PostsolveMatrix &prob);
};
#endif
