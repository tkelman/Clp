// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

/* 
   Authors
   
   John Forrest

 */
#ifndef ClpSimplexDual_H
#define ClpSimplexDual_H

#include "ClpSimplex.hpp"

/** This solves LPs using the dual simplex method

    It inherits from ClpSimplex.  It has no data of its own and 
    is never created - only cast from a ClpSimplex object at algorithm time. 

*/

class ClpSimplexDual : public ClpSimplex {

public:

  /**@name Description of algorithm */
  //@{
  /** Dual algorithm

      Method

     It tries to be a single phase approach with a weight of 1.0 being
     given to getting optimal and a weight of updatedDualBound_ being
     given to getting dual feasible.  In this version I have used the
     idea that this weight can be thought of as a fake bound.  If the
     distance between the lower and upper bounds on a variable is less
     than the feasibility weight then we are always better off flipping
     to other bound to make dual feasible.  If the distance is greater
     then we make up a fake bound updatedDualBound_ away from one bound.
     If we end up optimal or primal infeasible, we check to see if
     bounds okay.  If so we have finished, if not we increase updatedDualBound_
     and continue (after checking if unbounded). I am undecided about
     free variables - there is coding but I am not sure about it.  At
     present I put them in basis anyway.

     The code is designed to take advantage of sparsity so arrays are
     seldom zeroed out from scratch or gone over in their entirety.
     The only exception is a full scan to find outgoing variable for 
     Dantzig row choice.  For steepest edge we keep an updated list 
     of infeasibilities (actually squares).  
     On easy problems we don't need full scan - just
     pick first reasonable.

     One problem is how to tackle degeneracy and accuracy.  At present
     I am using the modification of costs which I put in OSL and some
     of what I think is the dual analog of Gill et al.
     I am still not sure of the exact details.

     The flow of dual is three while loops as follows:

     while (not finished) {

       while (not clean solution) {

          Factorize and/or clean up solution by flipping variables so
	  dual feasible.  If looks finished check fake dual bounds.
	  Repeat until status is iterating (-1) or finished (0,1,2)

       }

       while (status==-1) {

         Iterate until no pivot in or out or time to re-factorize.

         Flow is:

         choose pivot row (outgoing variable).  if none then
	 we are primal feasible so looks as if done but we need to
	 break and check bounds etc.

	 Get pivot row in tableau

         Choose incoming column.  If we don't find one then we look
	 primal infeasible so break and check bounds etc.  (Also the
	 pivot tolerance is larger after any iterations so that may be
	 reason)

         If we do find incoming column, we may have to adjust costs to
	 keep going forwards (anti-degeneracy).  Check pivot will be stable
	 and if unstable throw away iteration and break to re-factorize.
	 If minor error re-factorize after iteration.

	 Update everything (this may involve flipping variables to stay
	 dual feasible.

       }

     }

     TODO's (or maybe not)

     At present we never check we are going forwards.  I overdid that in
     OSL so will try and make a last resort.

     Needs partial scan pivot out option.

     May need other anti-degeneracy measures, especially if we try and use
     loose tolerances as a way to solve in fewer iterations.

     I like idea of dynamic scaling.  This gives opportunity to decouple
     different implications of scaling for accuracy, iteration count and
     feasibility tolerance.

  */

  int dual();
  //@}

  /**@name Functions used in dual */
  //@{
  /** This has the flow between re-factorizations
      Broken out for clarity and will be used by strong branching
   */
  void whileIterating(); 
  /** The duals are updated by the given arrays.
      Returns number of infeasibilities.
      After rowArray and columnArray will just have those which 
      have been flipped.
      Variables may be flipped between bounds to stay dual feasible.
      The output vector has movement of primal
      solution (row length array) */
  int updateDualsInDual(OsiIndexedVector * rowArray,
		  OsiIndexedVector * columnArray,
		  OsiIndexedVector * outputArray,
		  double theta,
		  double & objectiveChange);
  /** While updateDualsInDual sees what effect is of flip
      this does actuall flipping.
      If change >0.0 then value in array >0.0 => from lower to upper
  */
  void flipBounds(OsiIndexedVector * rowArray,
		  OsiIndexedVector * columnArray,
		  double change);
  /** 
      Row array has row part of pivot row
      Column array has column part.
      This chooses pivot column.
      Spare arrays are used to save pivots which will go infeasible
      We will check for basic so spare array will never overflow.
      If necessary will modify costs
      For speed, we may need to go to a bucket approach when many
      variables are being flipped
  */
  void dualColumn(OsiIndexedVector * rowArray,
		  OsiIndexedVector * columnArray,
		  OsiIndexedVector * spareArray,
		  OsiIndexedVector * spareArray2);
  /** 
      Chooses dual pivot row
      Would be faster with separate region to scan
      and will have this (with square of infeasibility) when steepest
      For easy problems we can just choose one of the first rows we look at
  */
  void dualRow();
  /** Checks if any fake bounds active - if so returns number and modifies
      updatedDualBound_ and everything.
      Free variables will be left as free
      Returns number of bounds changed if >=0
      Returns -1 if not initialize and no effect
      Fills in changeVector which can be used to see if unbounded
      and cost of change vector
  */
  int changeBounds(bool initialize,OsiIndexedVector * outputArray,
		   double & changeCost);
  /** As changeBounds but just changes new bounds for a single variable.
      Returns true if change */
  bool changeBound( int iSequence);
  /// Restores bound to original bound
  void originalBound(int iSequence);
  /** Checks if tentative optimal actually means unbounded in dual
      Returns -3 if not, 2 if is unbounded */
  int checkUnbounded(OsiIndexedVector * ray,OsiIndexedVector * spare,
		     double changeCost);
  /**  Refactorizes if necessary 
       Checks if finished.  Updates status.
       lastCleaned refers to iteration at which some objective/feasibility
       cleaning too place.

       type - 0 initial so set up save arrays etc
            - 1 normal -if good update save
	    - 2 restoring from saved 
  */
  void statusOfProblemInDual(int & lastCleaned, int type);
  /// Perturbs problem (method depends on perturbation())
  void perturb();
  //@}
};
#endif
