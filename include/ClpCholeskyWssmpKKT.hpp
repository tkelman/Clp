// Copyright (C) 2004, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef ClpCholeskyWssmpKKT_H
#define ClpCholeskyWssmpKKT_H

#include "ClpCholeskyBase.hpp"


/** WssmpKKT class for Clp Cholesky factorization

*/
class ClpMatrixBase;
class ClpCholeskyDense;
class ClpCholeskyWssmpKKT : public ClpCholeskyBase {
  
public:
   /**@name Virtual methods that the derived classes provides  */
   //@{
  /** Orders rows and saves pointer to matrix.and model.
   Returns non-zero if not enough memory */
  virtual int order(ClpInterior * model) ;
  /** Factorize - filling in rowsDropped and returning number dropped.
      If return code negative then out of memory */
  virtual int factorize(const double * diagonal, int * rowsDropped) ;
  /** Uses factorization to solve. */
  virtual void solve (double * region) ;
  /** Uses factorization to solve. - given as if KKT.
   region1 is rows+columns, region2 is rows */
  virtual void solveKKT (double * region1, double * region2, const double * diagonal,
			 double diagonalScaleFactor);
  //@}


  /**@name Constructors, destructor */
  //@{
  /** Constructor which has dense columns activated.
      Default is off. */
  ClpCholeskyWssmpKKT(int denseThreshold=-1);
  /** Destructor  */
  virtual ~ClpCholeskyWssmpKKT();
  // Copy
  ClpCholeskyWssmpKKT(const ClpCholeskyWssmpKKT&);
  // Assignment
  ClpCholeskyWssmpKKT& operator=(const ClpCholeskyWssmpKKT&);
  /// Clone
  virtual ClpCholeskyBase * clone() const ;
  //@}
   
    
private:
  /**@name Data members */
   //@{
  /// sparseFactor.
  double * sparseFactor_;
  /// choleskyStart
  CoinBigIndex * choleskyStart_;
  /// choleskyRow
  int * choleskyRow_;
  /// sizeFactor.
  CoinBigIndex sizeFactor_;
  /// integerParameters
  int integerParameters_[64];
  /// doubleParameters;
  double doubleParameters_[64];
  /// Dense threshold
  int denseThreshold_;
  //@}
};

#endif
