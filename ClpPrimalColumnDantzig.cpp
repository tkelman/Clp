// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif


#include "ClpSimplex.hpp"
#include "ClpPrimalColumnDantzig.hpp"
#include "OsiIndexedVector.hpp"
#include "ClpFactorization.hpp"
#include "ClpPackedMatrix.hpp"
#include <stdio.h>
//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpPrimalColumnDantzig::ClpPrimalColumnDantzig () 
: ClpPrimalColumnPivot()
{
  type_=1;
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpPrimalColumnDantzig::ClpPrimalColumnDantzig (const ClpPrimalColumnDantzig & source) 
: ClpPrimalColumnPivot(source)
{  

}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpPrimalColumnDantzig::~ClpPrimalColumnDantzig ()
{

}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpPrimalColumnDantzig &
ClpPrimalColumnDantzig::operator=(const ClpPrimalColumnDantzig& rhs)
{
  if (this != &rhs) {
    ClpPrimalColumnPivot::operator=(rhs);
  }
  return *this;
}

// Returns pivot column, -1 if none
int 
ClpPrimalColumnDantzig::pivotColumn(OsiIndexedVector * updates,
				    OsiIndexedVector * spareRow1,
				    OsiIndexedVector * spareRow2,
				    OsiIndexedVector * spareColumn1,
				    OsiIndexedVector * spareColumn2)
{
  assert(model_);
  int iSection,j;
  int number;
  int * index;
  double * updateBy;
  double * reducedCost;
  double dj = model_->dualIn();

  bool anyUpdates;

  if (updates->getNumElements()) {
    // would have to have two goes for devex, three for steepest
    anyUpdates=true;
    // add in pivot contribution
    if (model_->pivotRow()>=0) 
      updates->add(model_->pivotRow(),-dj);
  } else if (model_->pivotRow()>=0) {
    updates->insert(model_->pivotRow(),-dj);
    anyUpdates=true;
  } else {
    // sub flip - nothing to do
    anyUpdates=false;
  }

  if (anyUpdates) {
    model_->factorization()->updateColumnTranspose(spareRow2,updates);
    // put row of tableau in rowArray and columnArray
    model_->clpMatrix()->transposeTimes(model_,-1.0,
					updates,spareColumn2,spareColumn1);
    for (iSection=0;iSection<2;iSection++) {
      
      reducedCost=model_->djRegion(iSection);
      
      if (!iSection) {
	number = updates->getNumElements();
	index = updates->getIndices();
	updateBy = updates->denseVector();
      } else {
	number = spareColumn1->getNumElements();
	index = spareColumn1->getIndices();
	updateBy = spareColumn1->denseVector();
      }
      
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double value = reducedCost[iSequence];
	value -= updateBy[iSequence];
	updateBy[iSequence]=0.0;
	reducedCost[iSequence] = value;
      }
      
    }
    updates->setNumElements(0);
    spareColumn1->setNumElements(0);
  }


  // update of duals finished - now do pricing

  double largest=model_->currentPrimalTolerance();
  // we can't really trust infeasibilities if there is primal error
  if (model_->largestDualError()>1.0e-8)
    largest *= model_->largestDualError()/1.0e-8;

  

  double bestDj = model_->dualTolerance();
  int bestSequence=-1;

  double bestFreeDj = model_->dualTolerance();
  int bestFreeSequence=-1;
  
  number = model_->numberRows()+model_->numberColumns();
  int iSequence;
  reducedCost=model_->djRegion();

  for (iSequence=0;iSequence<number;iSequence++) {
    double value = reducedCost[iSequence];
    if (!model_->fixed(iSequence)) {
      ClpSimplex::Status status = model_->getStatus(iSequence);
      
      switch(status) {

      case ClpSimplex::basic:
	break;
      case ClpSimplex::isFree:
      case ClpSimplex::superBasic:
	if (fabs(value)>bestFreeDj) { 
	  bestFreeDj = fabs(value);
	  bestFreeSequence = iSequence;
	}
	break;
      case ClpSimplex::atUpperBound:
	if (value>bestDj) {
	  bestDj = value;
	  bestSequence = iSequence;
	}
	break;
      case ClpSimplex::atLowerBound:
	if (value<-bestDj) {
	  bestDj = -value;
	  bestSequence = iSequence;
	}
      }
    }
  }
  // bias towards free
  if (bestFreeSequence>=0&&bestFreeDj > 0.1*bestDj) 
    bestSequence = bestFreeSequence;
  return bestSequence;
}
  
//-------------------------------------------------------------------
// Clone
//-------------------------------------------------------------------
ClpPrimalColumnPivot * ClpPrimalColumnDantzig::clone(bool CopyData) const
{
  if (CopyData) {
    return new ClpPrimalColumnDantzig(*this);
  } else {
    return new ClpPrimalColumnDantzig();
  }
}

