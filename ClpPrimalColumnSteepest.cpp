// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#include "CoinPragma.hpp"

#include "ClpSimplex.hpp"
#include "ClpPrimalColumnSteepest.hpp"
#include "CoinIndexedVector.hpp"
#include "ClpFactorization.hpp"
#include "ClpNonLinearCost.hpp"
#include "ClpMessage.hpp"
#include "CoinHelperFunctions.hpp"
#include <stdio.h>


// bias for free variables
#define FREE_BIAS 1.0e1
// Acceptance criteria for free variables
#define FREE_ACCEPT 1.0e2
//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpPrimalColumnSteepest::ClpPrimalColumnSteepest (int mode) 
  : ClpPrimalColumnPivot(),
    devex_(0.0),
    weights_(NULL),
    infeasible_(NULL),
    alternateWeights_(NULL),
    savedWeights_(NULL),
    reference_(NULL),
    state_(-1),
    mode_(mode),
    numberSwitched_(0),
    pivotSequence_(-1),
    savedPivotSequence_(-1),
    savedSequenceOut_(-1)
{
  type_=2+64*mode;
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpPrimalColumnSteepest::ClpPrimalColumnSteepest (const ClpPrimalColumnSteepest & rhs) 
: ClpPrimalColumnPivot(rhs)
{  
  state_=rhs.state_;
  mode_ = rhs.mode_;
  numberSwitched_ = rhs.numberSwitched_;
  model_ = rhs.model_;
  pivotSequence_ = rhs.pivotSequence_;
  savedPivotSequence_ = rhs.savedPivotSequence_;
  savedSequenceOut_ = rhs.savedSequenceOut_;
  devex_ = rhs.devex_;
  if (rhs.infeasible_) {
    infeasible_= new CoinIndexedVector(rhs.infeasible_);
  } else {
    infeasible_=NULL;
  }
  reference_=NULL;
  if (rhs.weights_) {
    assert(model_);
    int number = model_->numberRows()+model_->numberColumns();
    weights_= new double[number];
    ClpDisjointCopyN(rhs.weights_,number,weights_);
    savedWeights_= new double[number];
    ClpDisjointCopyN(rhs.savedWeights_,number,savedWeights_);
    if (mode_!=1) {
      reference_ = new unsigned int[(number+31)>>5];
      memcpy(reference_,rhs.reference_,((number+31)>>5)*sizeof(unsigned int));
    }
  } else {
    weights_=NULL;
    savedWeights_=NULL;
  }
  if (rhs.alternateWeights_) {
    alternateWeights_= new CoinIndexedVector(rhs.alternateWeights_);
  } else {
    alternateWeights_=NULL;
  }
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpPrimalColumnSteepest::~ClpPrimalColumnSteepest ()
{
  delete [] weights_;
  delete infeasible_;
  delete alternateWeights_;
  delete [] savedWeights_;
  delete [] reference_;
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpPrimalColumnSteepest &
ClpPrimalColumnSteepest::operator=(const ClpPrimalColumnSteepest& rhs)
{
  if (this != &rhs) {
    ClpPrimalColumnPivot::operator=(rhs);
    state_=rhs.state_;
    mode_ = rhs.mode_;
    numberSwitched_ = rhs.numberSwitched_;
    model_ = rhs.model_;
    pivotSequence_ = rhs.pivotSequence_;
    savedPivotSequence_ = rhs.savedPivotSequence_;
    savedSequenceOut_ = rhs.savedSequenceOut_;
    devex_ = rhs.devex_;
    delete [] weights_;
    delete [] reference_;
    reference_=NULL;
    delete infeasible_;
    delete alternateWeights_;
    delete [] savedWeights_;
    savedWeights_ = NULL;
    if (rhs.infeasible_!=NULL) {
      infeasible_= new CoinIndexedVector(rhs.infeasible_);
    } else {
      infeasible_=NULL;
    }
    if (rhs.weights_!=NULL) {
      assert(model_);
      int number = model_->numberRows()+model_->numberColumns();
      weights_= new double[number];
      ClpDisjointCopyN(rhs.weights_,number,weights_);
      savedWeights_= new double[number];
      ClpDisjointCopyN(rhs.savedWeights_,number,savedWeights_);
      if (mode_!=1) {
	reference_ = new unsigned int[(number+31)>>5];
	memcpy(reference_,rhs.reference_,
	       ((number+31)>>5)*sizeof(unsigned int));
      }
    } else {
      weights_=NULL;
    }
    if (rhs.alternateWeights_!=NULL) {
      alternateWeights_= new CoinIndexedVector(rhs.alternateWeights_);
    } else {
      alternateWeights_=NULL;
    }
  }
  return *this;
}

#define TRY_NORM 1.0e-4
#define ADD_ONE 1.0
// Returns pivot column, -1 if none
int 
ClpPrimalColumnSteepest::pivotColumn(CoinIndexedVector * updates,
				    CoinIndexedVector * spareRow1,
				    CoinIndexedVector * spareRow2,
				    CoinIndexedVector * spareColumn1,
				    CoinIndexedVector * spareColumn2)
{
  assert(model_);
  if (model_->nonLinearCost()->lookBothWays()||model_->algorithm()==2) {
    // Do old way
    return pivotColumnOldMethod(updates,spareRow1,spareRow2,
				spareColumn1,spareColumn2);
  }
  int number=0;
  int * index;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  int anyUpdates;
  double * infeas = infeasible_->denseVector();

  // Local copy of mode so can decide what to do
  int switchType;
  if (mode_==4) 
    switchType = 5-numberSwitched_;
  else if (mode_>=10)
    switchType=3;
  else
    switchType = mode_;
  /* switchType - 
     0 - all exact devex
     1 - all steepest
     2 - some exact devex
     3 - auto some exact devex
     4 - devex
     5 - dantzig
     10 - can go to mini-sprint
  */
  if (updates->getNumElements()) {
    // would have to have two goes for devex, three for steepest
    anyUpdates=2;
    // add in pivot contribution
    if (pivotRow>=0) 
      updates->add(pivotRow,-dj);
  } else if (pivotRow>=0) {
    if (fabs(dj)>1.0e-15) {
      // some dj
      updates->insert(pivotRow,-dj);
      if (fabs(dj)>1.0e-6) {
	// reasonable size
	anyUpdates=1;
      } else {
	// too small
	anyUpdates=2;
      }
    } else {
      // zero dj
      anyUpdates=-1;
    }
  } else if (pivotSequence_>=0){
    // just after re-factorization
    anyUpdates=-1;
  } else {
    // sub flip - nothing to do
    anyUpdates=0;
  }
  int sequenceOut = model_->sequenceOut();
  if (switchType==5) {
    if (anyUpdates>0) {
      justDjs(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    }
  } else if (anyUpdates==1) {
    if (switchType<4) {
      // exact etc when can use dj 
      djsAndSteepest(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    } else {
      // devex etc when can use dj 
      djsAndDevex(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    }
  } else if (anyUpdates==-1) {
    if (switchType<4) {
      // exact etc when djs okay 
      justSteepest(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    } else {
      // devex etc when djs okay 
      justDevex(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    }
  } else if (anyUpdates==2) {
    if (switchType<4) {
      // exact etc when have to use pivot
      djsAndSteepest2(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    } else {
      // devex etc when have to use pivot
      djsAndDevex2(updates,spareRow1,spareRow2,
	spareColumn1,spareColumn2);
    }
  } 
  // make sure outgoing from last iteration okay
  if (sequenceOut>=0) {
    ClpSimplex::Status status = model_->getStatus(sequenceOut);
    double value = model_->reducedCost(sequenceOut);
    
    switch(status) {
      
    case ClpSimplex::basic:
    case ClpSimplex::isFixed:
      break;
    case ClpSimplex::isFree:
    case ClpSimplex::superBasic:
      if (fabs(value)>FREE_ACCEPT*tolerance) { 
	// we are going to bias towards free (but only if reasonable)
	value *= FREE_BIAS;
	// store square in list
	if (infeas[sequenceOut])
	  infeas[sequenceOut] = value*value; // already there
	else
	  infeasible_->quickAdd(sequenceOut,value*value);
      } else {
	infeasible_->zero(sequenceOut);
      }
      break;
    case ClpSimplex::atUpperBound:
      if (value>tolerance) {
	// store square in list
	if (infeas[sequenceOut])
	  infeas[sequenceOut] = value*value; // already there
	else
	  infeasible_->quickAdd(sequenceOut,value*value);
      } else {
	infeasible_->zero(sequenceOut);
      }
      break;
    case ClpSimplex::atLowerBound:
      if (value<-tolerance) {
	// store square in list
	if (infeas[sequenceOut])
	  infeas[sequenceOut] = value*value; // already there
	else
	  infeasible_->quickAdd(sequenceOut,value*value);
      } else {
	infeasible_->zero(sequenceOut);
      }
    }
  }
  // update of duals finished - now do pricing
  // See what sort of pricing
  int numberWanted=0;
  number = infeasible_->getNumElements();
  int numberColumns = model_->numberColumns();
  if (switchType==5) {
    pivotSequence_=-1;
    pivotRow=-1;
    // See if to switch
    int numberElements = model_->factorization()->numberElements();
    int numberRows = model_->numberRows();
    // ratio is done on number of columns here
    //double ratio = (double) numberElements/(double) numberColumns;
    double ratio = (double) numberElements/(double) numberRows;
    //double ratio = (double) numberElements/(double) model_->clpMatrix()->getNumElements();
    if (ratio<0.1) {
      numberWanted = max(100,number/200);
    } else if (ratio<0.15) {
      numberWanted = max(500,number/40);
    } else if (ratio<0.2) {
      numberWanted = max(2000,number/10);
      numberWanted = max(numberWanted,numberColumns/30);
    } else {
      switchType=4;
      // initialize
      numberSwitched_++;
      // Make sure will re-do
      delete [] weights_;
      weights_=NULL;
      saveWeights(model_,4);
      printf("switching to devex %d nel ratio %g\n",numberElements,ratio);
    }
    //if (model_->numberIterations()%1000==0)
    //printf("numels %d ratio %g wanted %d\n",numberElements,ratio,numberWanted);
  }
  int numberElements = model_->factorization()->numberElements();
  int numberRows = model_->numberRows();
  // ratio is done on number of rows here
  double ratio = (double) numberElements/(double) numberRows;
  if(switchType==4) {
    // Still in devex mode
    // Go to steepest if lot of iterations?
    if (ratio<1.0) {
      numberWanted = max(2000,number/20);
    } else if (ratio<4.0) {
      numberWanted = max(2000,number/10);
      numberWanted = max(numberWanted,numberColumns/20);
    } else {
      // we can zero out
      updates->clear();
      spareColumn1->clear();
      switchType=3;
      // initialize
      pivotSequence_=-1;
      pivotRow=-1;
      numberSwitched_++;
      // Make sure will re-do
      delete [] weights_;
      weights_=NULL;
      saveWeights(model_,4);
      printf("switching to exact %d nel ratio %g\n",numberElements,ratio);
      updates->clear();
    }
    if (model_->numberIterations()%1000==0)
    printf("numels %d ratio %g wanted %d type x\n",numberElements,ratio,numberWanted);
  } 
  if (switchType<4) {
    if (switchType<2 ) {
      numberWanted = number+1;
    } else if (switchType==2) {
      numberWanted = max(2000,number/8);
    } else {
      if (ratio<1.0) {
	numberWanted = max(2000,number/20);
      } else if (ratio<5.0) {
	numberWanted = max(2000,number/10);
	numberWanted = max(numberWanted,numberColumns/40);
      } else if (ratio<10.0) {
	numberWanted = max(2000,number/8);
	numberWanted = max(numberWanted,numberColumns/20);
      } else {
	ratio = number * (ratio/80.0);
	if (ratio>number) {
	  numberWanted=number+1;
	} else {
	  numberWanted = max(2000,(int) ratio);
	  numberWanted = max(numberWanted,numberColumns/10);
	}
      }
    }
    //if (model_->numberIterations()%1000==0)
    //printf("numels %d ratio %g wanted %d type %d\n",numberElements,ratio,numberWanted,
    //switchType);
  }


  double bestDj = 1.0e-30;
  int bestSequence=-1;

  int i,iSequence;
  index = infeasible_->getIndices();
  number = infeasible_->getNumElements();
  // Re-sort infeasible every 100 pivots
  if (0&&model_->factorization()->pivots()>0&&
      (model_->factorization()->pivots()%100)==0) {
    int nLook = model_->numberRows()+numberColumns;
    number=0;
    for (i=0;i<nLook;i++) {
      if (infeas[i]) { 
	if (fabs(infeas[i])>COIN_INDEXED_TINY_ELEMENT) 
	  index[number++]=i;
	else
	  infeas[i]=0.0;
      }
    }
    infeasible_->setNumElements(number);
  }
  if(model_->numberIterations()<model_->lastBadIteration()+200) {
    // we can't really trust infeasibilities if there is dual error
    double checkTolerance = 1.0e-8;
    if (!model_->factorization()->pivots())
      checkTolerance = 1.0e-6;
    if (model_->largestDualError()>checkTolerance)
      tolerance *= model_->largestDualError()/checkTolerance;
    // But cap
    tolerance = min(1000.0,tolerance);
  }
#ifdef CLP_DEBUG
  if (model_->numberDualInfeasibilities()==1) 
    printf("** %g %g %g %x %x %d\n",tolerance,model_->dualTolerance(),
	   model_->largestDualError(),model_,model_->messageHandler(),
	   number);
#endif
  // stop last one coming immediately
  double saveOutInfeasibility=0.0;
  if (sequenceOut>=0) {
    saveOutInfeasibility = infeas[sequenceOut];
    infeas[sequenceOut]=0.0;
  }
  if (model_->factorization()->pivots()&&model_->numberPrimalInfeasibilities())
    tolerance = max(tolerance,1.0e-10*model_->infeasibilityCost());
  tolerance *= tolerance; // as we are using squares

  int iPass;
  // Setup two passes
  int start[4];
  start[1]=number;
  start[2]=0;
  double dstart = ((double) number) * CoinDrand48();
  start[0]=(int) dstart;
  start[3]=start[0];
  //double largestWeight=0.0;
  //double smallestWeight=1.0e100;
  for (iPass=0;iPass<2;iPass++) {
    int end = start[2*iPass+1];
    if (switchType<5) {
      for (i=start[2*iPass];i<end;i++) {
	iSequence = index[i];
	double value = infeas[iSequence];
	if (value>tolerance) {
	  double weight = weights_[iSequence];
	  //weight=1.0;
	  if (value>bestDj*weight) {
	    // check flagged variable and correct dj
	    if (!model_->flagged(iSequence)) {
	      bestDj=value/weight;
	      bestSequence = iSequence;
	    } else {
	      // just to make sure we don't exit before got something
	      numberWanted++;
	    }
	  }
	}
	numberWanted--;
	if (!numberWanted)
	  break;
      }
    } else {
      // Dantzig
      for (i=start[2*iPass];i<end;i++) {
	iSequence = index[i];
	double value = infeas[iSequence];
	if (value>tolerance) {
	  if (value>bestDj) {
	    // check flagged variable and correct dj
	    if (!model_->flagged(iSequence)) {
	      bestDj=value;
	      bestSequence = iSequence;
	    } else {
	      // just to make sure we don't exit before got something
	      numberWanted++;
	    }
	  }
	}
	numberWanted--;
	if (!numberWanted)
	  break;
      }
    }
    if (!numberWanted)
      break;
  }
  if (sequenceOut>=0) {
    infeas[sequenceOut]=saveOutInfeasibility;
  }
  /*if (model_->numberIterations()%100==0)
    printf("%d best %g\n",bestSequence,bestDj);*/
  
#ifndef NDEBUG
  if (bestSequence>=0) {
    if (model_->getStatus(bestSequence)==ClpSimplex::atLowerBound)
      assert(model_->reducedCost(bestSequence)<0.0);
    if (model_->getStatus(bestSequence)==ClpSimplex::atUpperBound)
      assert(model_->reducedCost(bestSequence)>0.0);
  }
#endif
  return bestSequence;
}
// Just update djs
void 
ClpPrimalColumnSteepest::justDjs(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int iSection,j;
  int number=0;
  int * index;
  double * updateBy;
  double * reducedCost;
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  double * infeas = infeasible_->denseVector();
  model_->factorization()->updateColumnTranspose(spareRow2,updates);
  
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  // normal
  for (iSection=0;iSection<2;iSection++) {
    
    reducedCost=model_->djRegion(iSection);
    int addSequence;
    
    if (!iSection) {
      number = updates->getNumElements();
      index = updates->getIndices();
      updateBy = updates->denseVector();
      addSequence = model_->numberColumns();
    } else {
      number = spareColumn1->getNumElements();
      index = spareColumn1->getIndices();
      updateBy = spareColumn1->denseVector();
      addSequence = 0;
    }
    
    for (j=0;j<number;j++) {
      int iSequence = index[j];
      double value = reducedCost[iSequence];
      value -= updateBy[iSequence];
      updateBy[iSequence]=0.0;
      reducedCost[iSequence] = value;
      ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
      
      switch(status) {
	
      case ClpSimplex::basic:
	infeasible_->zero(iSequence+addSequence);
      case ClpSimplex::isFixed:
	break;
      case ClpSimplex::isFree:
      case ClpSimplex::superBasic:
	if (fabs(value)>FREE_ACCEPT*tolerance) {
	  // we are going to bias towards free (but only if reasonable)
	  value *= FREE_BIAS;
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
	break;
      case ClpSimplex::atUpperBound:
	if (value>tolerance) {
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
	break;
      case ClpSimplex::atLowerBound:
	if (value<-tolerance) {
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
      }
    }
  }
  updates->setNumElements(0);
  spareColumn1->setNumElements(0);
  if (pivotRow>=0) {
    // make sure infeasibility on incoming is 0.0
    int sequenceIn = model_->sequenceIn();
    infeasible_->zero(sequenceIn);
  }
}
// Update djs, weights for Devex
void 
ClpPrimalColumnSteepest::djsAndDevex(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int j;
  int number=0;
  int * index;
  double * updateBy;
  double * reducedCost;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  // for weights update we use pivotSequence
  // unset in case sub flip
  assert (pivotSequence_>=0);
  pivotSequence_=-1;
  double * infeas = infeasible_->denseVector();
  model_->factorization()->updateColumnTranspose(spareRow2,updates);
  // and we can see if reference
  double referenceIn=0.0;
  int sequenceIn = model_->sequenceIn();
  if (mode_!=1&&reference(sequenceIn))
    referenceIn=1.0;
  // save outgoing weight round update
  double outgoingWeight=0.0;
  int sequenceOut = model_->sequenceOut();
  if (sequenceOut>=0)
    outgoingWeight=weights_[sequenceOut];
    
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  // update weights
  double * weight;
  int numberColumns = model_->numberColumns();
  double scaleFactor = -1.0/dj; // as formula is with 1.0
  // rows
  reducedCost=model_->djRegion(0);
  int addSequence=model_->numberColumns();;
    
  number = updates->getNumElements();
  index = updates->getIndices();
  updateBy = updates->denseVector();
  weight = weights_+numberColumns;
  // Devex
  for (j=0;j<number;j++) {
    double thisWeight;
    double pivot;
    double value3;
    int iSequence = index[j];
    double value = reducedCost[iSequence];
    double value2 = updateBy[iSequence];
    updateBy[iSequence]=0.0;
    value -= value2;
    reducedCost[iSequence] = value;
    ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
    
    switch(status) {
      
    case ClpSimplex::basic:
      infeasible_->zero(iSequence+addSequence);
    case ClpSimplex::isFixed:
      break;
    case ClpSimplex::isFree:
    case ClpSimplex::superBasic:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      value3 = pivot * pivot*devex_;
      if (reference(iSequence+numberColumns))
	value3 += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value3);
      if (fabs(value)>FREE_ACCEPT*tolerance) {
	// we are going to bias towards free (but only if reasonable)
	value *= FREE_BIAS;
	// store square in list
	if (infeas[iSequence+addSequence])
	  infeas[iSequence+addSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence+addSequence,value*value);
      } else {
	infeasible_->zero(iSequence+addSequence);
      }
      break;
    case ClpSimplex::atUpperBound:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      value3 = pivot * pivot*devex_;
      if (reference(iSequence+numberColumns))
	value3 += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value3);
      if (value>tolerance) {
	// store square in list
	if (infeas[iSequence+addSequence])
	  infeas[iSequence+addSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence+addSequence,value*value);
      } else {
	infeasible_->zero(iSequence+addSequence);
      }
      break;
    case ClpSimplex::atLowerBound:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      value3 = pivot * pivot*devex_;
      if (reference(iSequence+numberColumns))
	value3 += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value3);
      if (value<-tolerance) {
	// store square in list
	if (infeas[iSequence+addSequence])
	  infeas[iSequence+addSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence+addSequence,value*value);
      } else {
	infeasible_->zero(iSequence+addSequence);
      }
    }
  }
  
  // columns
  weight = weights_;
  
  scaleFactor = -scaleFactor;
  reducedCost=model_->djRegion(1);
  number = spareColumn1->getNumElements();
  index = spareColumn1->getIndices();
  updateBy = spareColumn1->denseVector();

  // Devex
  
  for (j=0;j<number;j++) {
    double thisWeight;
    double pivot;
    double value3;
    int iSequence = index[j];
    double value = reducedCost[iSequence];
    double value2 = updateBy[iSequence];
    value -= value2;
    updateBy[iSequence]=0.0;
    reducedCost[iSequence] = value;
    ClpSimplex::Status status = model_->getStatus(iSequence);
    
    switch(status) {
      
    case ClpSimplex::basic:
      infeasible_->zero(iSequence);
    case ClpSimplex::isFixed:
      break;
    case ClpSimplex::isFree:
    case ClpSimplex::superBasic:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      value3 = pivot * pivot*devex_;
      if (reference(iSequence))
	value3 += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value3);
      if (fabs(value)>FREE_ACCEPT*tolerance) {
	// we are going to bias towards free (but only if reasonable)
	value *= FREE_BIAS;
	// store square in list
	if (infeas[iSequence])
	  infeas[iSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence,value*value);
      } else {
	infeasible_->zero(iSequence);
      }
      break;
    case ClpSimplex::atUpperBound:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      value3 = pivot * pivot*devex_;
      if (reference(iSequence))
	value3 += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value3);
      if (value>tolerance) {
	// store square in list
	if (infeas[iSequence])
	  infeas[iSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence,value*value);
      } else {
	infeasible_->zero(iSequence);
      }
      break;
    case ClpSimplex::atLowerBound:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      value3 = pivot * pivot*devex_;
      if (reference(iSequence))
	value3 += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value3);
      if (value<-tolerance) {
	// store square in list
	if (infeas[iSequence])
	  infeas[iSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence,value*value);
      } else {
	infeasible_->zero(iSequence);
      }
    }
  }
  // restore outgoing weight
  if (sequenceOut>=0)
    weights_[sequenceOut]=outgoingWeight;
  // make sure infeasibility on incoming is 0.0
  infeasible_->zero(sequenceIn);
  spareRow2->setNumElements(0);
  //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
  // check for accuracy
  int iCheck=229;
  //printf("weight for iCheck is %g\n",weights_[iCheck]);
  int numberRows = model_->numberRows();
  //int numberColumns = model_->numberColumns();
  for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
    if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	!model_->getStatus(iCheck)!=ClpSimplex::isFixed)
      checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
  }
#endif
  updates->setNumElements(0);
  spareColumn1->setNumElements(0);
}
// Update djs, weights for Steepest
void 
ClpPrimalColumnSteepest::djsAndSteepest(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int j;
  int number=0;
  int * index;
  double * updateBy;
  double * reducedCost;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  // for weights update we use pivotSequence
  // unset in case sub flip
  assert (pivotSequence_>=0);
  pivotSequence_=-1;
  double * infeas = infeasible_->denseVector();
  model_->factorization()->updateColumnTranspose(spareRow2,updates);

  model_->factorization()->updateColumnTranspose(spareRow2,
						 alternateWeights_);
  // and we can see if reference
  double referenceIn=0.0;
  int sequenceIn = model_->sequenceIn();
  if (mode_!=1&&reference(sequenceIn))
    referenceIn=1.0;
  // save outgoing weight round update
  double outgoingWeight=0.0;
  int sequenceOut = model_->sequenceOut();
  if (sequenceOut>=0)
    outgoingWeight=weights_[sequenceOut];
    
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  // get subset which have nonzero tableau elements
  // Luckily spareRow2 is long enough (rowArray_[3])
  model_->clpMatrix()->subsetTransposeTimes(model_,alternateWeights_,
					      spareColumn1,
					      spareRow2);
  assert (spareRow2->packedMode());
  // update weights
  double * weight;
  double * other = alternateWeights_->denseVector();
  int numberColumns = model_->numberColumns();
  double scaleFactor = -1.0/dj; // as formula is with 1.0
  // rows
  reducedCost=model_->djRegion(0);
  int addSequence=model_->numberColumns();;
    
  number = updates->getNumElements();
  index = updates->getIndices();
  updateBy = updates->denseVector();
  weight = weights_+numberColumns;
  
  for (j=0;j<number;j++) {
    double thisWeight;
    double pivot;
    double modification;
    double pivotSquared;
    int iSequence = index[j];
    double value = reducedCost[iSequence];
    double value2 = updateBy[iSequence];
    value -= value2;
    reducedCost[iSequence] = value;
    ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
    updateBy[iSequence]=0.0;
    
    switch(status) {
      
    case ClpSimplex::basic:
      infeasible_->zero(iSequence+addSequence);
    case ClpSimplex::isFixed:
      break;
    case ClpSimplex::isFree:
    case ClpSimplex::superBasic:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      modification = other[iSequence];
      pivotSquared = pivot * pivot;
      
      thisWeight += pivotSquared * devex_ + pivot * modification;
      if (thisWeight<TRY_NORM) {
	if (mode_==1) {
	  // steepest
	  thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	} else {
	  // exact
	  thisWeight = referenceIn*pivotSquared;
	  if (reference(iSequence+numberColumns))
	    thisWeight += 1.0;
	  thisWeight = max(thisWeight,TRY_NORM);
	}
      }
      weight[iSequence] = thisWeight;
      if (fabs(value)>FREE_ACCEPT*tolerance) {
	// we are going to bias towards free (but only if reasonable)
	value *= FREE_BIAS;
	// store square in list
	if (infeas[iSequence+addSequence])
	  infeas[iSequence+addSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence+addSequence,value*value);
      } else {
	infeasible_->zero(iSequence+addSequence);
      }
      break;
    case ClpSimplex::atUpperBound:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      modification = other[iSequence];
      pivotSquared = pivot * pivot;
      
      thisWeight += pivotSquared * devex_ + pivot * modification;
      if (thisWeight<TRY_NORM) {
	if (mode_==1) {
	  // steepest
	  thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	} else {
	  // exact
	  thisWeight = referenceIn*pivotSquared;
	  if (reference(iSequence+numberColumns))
	    thisWeight += 1.0;
	  thisWeight = max(thisWeight,TRY_NORM);
	}
      }
      weight[iSequence] = thisWeight;
      if (value>tolerance) {
	// store square in list
	if (infeas[iSequence+addSequence])
	  infeas[iSequence+addSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence+addSequence,value*value);
      } else {
	infeasible_->zero(iSequence+addSequence);
      }
      break;
    case ClpSimplex::atLowerBound:
      thisWeight = weight[iSequence];
      // row has -1 
      pivot = value2*scaleFactor;
      modification = other[iSequence];
      pivotSquared = pivot * pivot;
      
      thisWeight += pivotSquared * devex_ + pivot * modification;
      if (thisWeight<TRY_NORM) {
	if (mode_==1) {
	  // steepest
	  thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	} else {
	  // exact
	  thisWeight = referenceIn*pivotSquared;
	  if (reference(iSequence+numberColumns))
	    thisWeight += 1.0;
	  thisWeight = max(thisWeight,TRY_NORM);
	}
      }
      weight[iSequence] = thisWeight;
      if (value<-tolerance) {
	// store square in list
	if (infeas[iSequence+addSequence])
	  infeas[iSequence+addSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence+addSequence,value*value);
      } else {
	infeasible_->zero(iSequence+addSequence);
      }
    }
  }
  
  // columns
  weight = weights_;
  
  scaleFactor = -scaleFactor;
  reducedCost=model_->djRegion(1);
  number = spareColumn1->getNumElements();
  index = spareColumn1->getIndices();
  updateBy = spareColumn1->denseVector();
  double * updateBy2 = spareRow2->denseVector();

  for (j=0;j<number;j++) {
    double thisWeight;
    double pivot;
    double modification;
    double pivotSquared;
    int iSequence = index[j];
    double value = reducedCost[iSequence];
    double value2 = updateBy[iSequence];
    updateBy[iSequence]=0.0;
    value -= value2;
    reducedCost[iSequence] = value;
    ClpSimplex::Status status = model_->getStatus(iSequence);
    
    switch(status) {
      
    case ClpSimplex::basic:
      infeasible_->zero(iSequence);
      updateBy2[j]=0.0;
    case ClpSimplex::isFixed:
      updateBy2[j]=0.0;
      break;
    case ClpSimplex::isFree:
    case ClpSimplex::superBasic:
      thisWeight = weight[iSequence];
      pivot = value2*scaleFactor;
      modification = updateBy2[j];
      updateBy2[j]=0.0;
      pivotSquared = pivot * pivot;
      
      thisWeight += pivotSquared * devex_ + pivot * modification;
      if (thisWeight<TRY_NORM) {
	if (mode_==1) {
	  // steepest
	  thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	} else {
	  // exact
	  thisWeight = referenceIn*pivotSquared;
	  if (reference(iSequence))
	    thisWeight += 1.0;
	  thisWeight = max(thisWeight,TRY_NORM);
	}
      }
      weight[iSequence] = thisWeight;
      if (fabs(value)>FREE_ACCEPT*tolerance) {
	// we are going to bias towards free (but only if reasonable)
	value *= FREE_BIAS;
	// store square in list
	if (infeas[iSequence])
	  infeas[iSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence,value*value);
      } else {
	infeasible_->zero(iSequence);
      }
      break;
    case ClpSimplex::atUpperBound:
      thisWeight = weight[iSequence];
      pivot = value2*scaleFactor;
      modification = updateBy2[j];
      updateBy2[j]=0.0;
      pivotSquared = pivot * pivot;
      
      thisWeight += pivotSquared * devex_ + pivot * modification;
      if (thisWeight<TRY_NORM) {
	if (mode_==1) {
	  // steepest
	  thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	} else {
	  // exact
	  thisWeight = referenceIn*pivotSquared;
	  if (reference(iSequence))
	    thisWeight += 1.0;
	  thisWeight = max(thisWeight,TRY_NORM);
	}
      }
      weight[iSequence] = thisWeight;
      if (value>tolerance) {
	// store square in list
	if (infeas[iSequence])
	  infeas[iSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence,value*value);
      } else {
	infeasible_->zero(iSequence);
      }
      break;
    case ClpSimplex::atLowerBound:
      thisWeight = weight[iSequence];
      pivot = value2*scaleFactor;
      modification = updateBy2[j];
      updateBy2[j]=0.0;
      pivotSquared = pivot * pivot;
      
      thisWeight += pivotSquared * devex_ + pivot * modification;
      if (thisWeight<TRY_NORM) {
	if (mode_==1) {
	  // steepest
	  thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	} else {
	  // exact
	  thisWeight = referenceIn*pivotSquared;
	  if (reference(iSequence))
	    thisWeight += 1.0;
	  thisWeight = max(thisWeight,TRY_NORM);
	}
      }
      weight[iSequence] = thisWeight;
      if (value<-tolerance) {
	// store square in list
	if (infeas[iSequence])
	  infeas[iSequence] = value*value; // already there
	else
	  infeasible_->quickAdd(iSequence,value*value);
      } else {
	infeasible_->zero(iSequence);
      }
    }
  }
  // restore outgoing weight
  if (sequenceOut>=0)
    weights_[sequenceOut]=outgoingWeight;
  // make sure infeasibility on incoming is 0.0
  infeasible_->zero(sequenceIn);
  alternateWeights_->clear();
  spareRow2->setNumElements(0);
  //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
  // check for accuracy
  int iCheck=229;
  //printf("weight for iCheck is %g\n",weights_[iCheck]);
  int numberRows = model_->numberRows();
  //int numberColumns = model_->numberColumns();
  for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
    if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	!model_->getStatus(iCheck)!=ClpSimplex::isFixed)
      checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
  }
#endif
  updates->setNumElements(0);
  spareColumn1->setNumElements(0);
}
// Update djs, weights for Devex
void 
ClpPrimalColumnSteepest::djsAndDevex2(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int iSection,j;
  int number=0;
  int * index;
  double * updateBy;
  double * reducedCost;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  double * infeas = infeasible_->denseVector();
  model_->factorization()->updateColumnTranspose(spareRow2,updates);
  
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  // normal
  for (iSection=0;iSection<2;iSection++) {
    
    reducedCost=model_->djRegion(iSection);
    int addSequence;
    
    if (!iSection) {
      number = updates->getNumElements();
      index = updates->getIndices();
      updateBy = updates->denseVector();
      addSequence = model_->numberColumns();
    } else {
      number = spareColumn1->getNumElements();
      index = spareColumn1->getIndices();
      updateBy = spareColumn1->denseVector();
      addSequence = 0;
    }
    
    for (j=0;j<number;j++) {
      int iSequence = index[j];
      double value = reducedCost[iSequence];
      value -= updateBy[iSequence];
      reducedCost[iSequence] = value;
      ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
      
      switch(status) {
	
      case ClpSimplex::basic:
	infeasible_->zero(iSequence+addSequence);
      case ClpSimplex::isFixed:
	break;
      case ClpSimplex::isFree:
      case ClpSimplex::superBasic:
	if (fabs(value)>FREE_ACCEPT*tolerance) {
	  // we are going to bias towards free (but only if reasonable)
	  value *= FREE_BIAS;
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
	break;
      case ClpSimplex::atUpperBound:
	if (value>tolerance) {
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
	break;
      case ClpSimplex::atLowerBound:
	if (value<-tolerance) {
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
      }
    }
  }
  // we can zero out as will have to get pivot row
  updates->clear();
  spareColumn1->clear();
  // make sure infeasibility on incoming is 0.0
  int sequenceIn = model_->sequenceIn();
  infeasible_->zero(sequenceIn);
  // for weights update we use pivotSequence
  if (pivotSequence_>=0) {
    pivotRow = pivotSequence_;
    // unset in case sub flip
    pivotSequence_=-1;
    // make sure infeasibility on incoming is 0.0
    const int * pivotVariable = model_->pivotVariable();
    sequenceIn = pivotVariable[pivotRow];
    infeasible_->zero(sequenceIn);
    // and we can see if reference
    double referenceIn=0.0;
    if (mode_!=1&&reference(sequenceIn))
      referenceIn=1.0;
    // save outgoing weight round update
    double outgoingWeight=0.0;
    int sequenceOut = model_->sequenceOut();
    if (sequenceOut>=0)
      outgoingWeight=weights_[sequenceOut];
    // update weights
    updates->setNumElements(0);
    spareColumn1->setNumElements(0);
    // might as well set dj to 1
    dj = 1.0;
    updates->insert(pivotRow,-dj);
    model_->factorization()->updateColumnTranspose(spareRow2,updates);
    // put row of tableau in rowArray and columnArray
    model_->clpMatrix()->transposeTimes(model_,-1.0,
					updates,spareColumn2,spareColumn1);
    double * weight;
    int numberColumns = model_->numberColumns();
    double scaleFactor = -1.0/dj; // as formula is with 1.0
    // rows
    number = updates->getNumElements();
    index = updates->getIndices();
    updateBy = updates->denseVector();
    weight = weights_+numberColumns;
    
    assert (devex_>0.0);
    for (j=0;j<number;j++) {
      int iSequence = index[j];
      double thisWeight = weight[iSequence];
      // row has -1 
      double pivot = updateBy[iSequence]*scaleFactor;
      updateBy[iSequence]=0.0;
      double value = pivot * pivot*devex_;
      if (reference(iSequence+numberColumns))
	value += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value);
    }
    
    // columns
    weight = weights_;
    
    scaleFactor = -scaleFactor;
    
    number = spareColumn1->getNumElements();
    index = spareColumn1->getIndices();
    updateBy = spareColumn1->denseVector();
    for (j=0;j<number;j++) {
      int iSequence = index[j];
      double thisWeight = weight[iSequence];
      // row has -1 
      double pivot = updateBy[iSequence]*scaleFactor;
      updateBy[iSequence]=0.0;
      double value = pivot * pivot*devex_;
      if (reference(iSequence))
	value += 1.0;
      weight[iSequence] = max(0.99*thisWeight,value);
    }
    // restore outgoing weight
    if (sequenceOut>=0)
      weights_[sequenceOut]=outgoingWeight;
    spareColumn2->setNumElements(0);
    //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
    // check for accuracy
    int iCheck=229;
    //printf("weight for iCheck is %g\n",weights_[iCheck]);
    int numberRows = model_->numberRows();
    //int numberColumns = model_->numberColumns();
    for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
      if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	  !model_->getStatus(iCheck)!=ClpSimplex::isFixed)
	checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
    }
#endif
    updates->setNumElements(0);
    spareColumn1->setNumElements(0);
  }
}
// Update djs, weights for Steepest
void 
ClpPrimalColumnSteepest::djsAndSteepest2(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int iSection,j;
  int number=0;
  int * index;
  double * updateBy;
  double * reducedCost;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  double * infeas = infeasible_->denseVector();
  model_->factorization()->updateColumnTranspose(spareRow2,updates);
  
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  // normal
  for (iSection=0;iSection<2;iSection++) {
    
    reducedCost=model_->djRegion(iSection);
    int addSequence;
    
    if (!iSection) {
      number = updates->getNumElements();
      index = updates->getIndices();
      updateBy = updates->denseVector();
      addSequence = model_->numberColumns();
    } else {
      number = spareColumn1->getNumElements();
      index = spareColumn1->getIndices();
      updateBy = spareColumn1->denseVector();
      addSequence = 0;
    }
    
    for (j=0;j<number;j++) {
      int iSequence = index[j];
      double value = reducedCost[iSequence];
      value -= updateBy[iSequence];
      reducedCost[iSequence] = value;
      ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
      
      switch(status) {
	
      case ClpSimplex::basic:
	infeasible_->zero(iSequence+addSequence);
      case ClpSimplex::isFixed:
	break;
      case ClpSimplex::isFree:
      case ClpSimplex::superBasic:
	if (fabs(value)>FREE_ACCEPT*tolerance) {
	  // we are going to bias towards free (but only if reasonable)
	  value *= FREE_BIAS;
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
	break;
      case ClpSimplex::atUpperBound:
	if (value>tolerance) {
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
	break;
      case ClpSimplex::atLowerBound:
	if (value<-tolerance) {
	  // store square in list
	  if (infeas[iSequence+addSequence])
	    infeas[iSequence+addSequence] = value*value; // already there
	  else
	    infeasible_->quickAdd(iSequence+addSequence,value*value);
	} else {
	  infeasible_->zero(iSequence+addSequence);
	}
      }
    }
  }
  // we can zero out as will have to get pivot row
  // ***** move
  updates->clear();
  spareColumn1->clear();
  if (pivotRow>=0) {
    // make sure infeasibility on incoming is 0.0
    int sequenceIn = model_->sequenceIn();
    infeasible_->zero(sequenceIn);
  }
  // for weights update we use pivotSequence
  pivotRow = pivotSequence_;
  // unset in case sub flip
  pivotSequence_=-1;
  if (pivotRow>=0) {
    // make sure infeasibility on incoming is 0.0
    const int * pivotVariable = model_->pivotVariable();
    int sequenceIn = pivotVariable[pivotRow];
    infeasible_->zero(sequenceIn);
    // and we can see if reference
    double referenceIn=0.0;
    if (mode_!=1&&reference(sequenceIn))
      referenceIn=1.0;
    // save outgoing weight round update
    double outgoingWeight=0.0;
    int sequenceOut = model_->sequenceOut();
    if (sequenceOut>=0)
      outgoingWeight=weights_[sequenceOut];
    // update weights
    updates->setNumElements(0);
    spareColumn1->setNumElements(0);
    // might as well set dj to 1
    dj = 1.0;
    updates->insert(pivotRow,-dj);
    model_->factorization()->updateColumnTranspose(spareRow2,updates);
    // put row of tableau in rowArray and columnArray
    model_->clpMatrix()->transposeTimes(model_,-1.0,
					updates,spareColumn2,spareColumn1);
    double * weight;
    double * other = alternateWeights_->denseVector();
    int numberColumns = model_->numberColumns();
    double scaleFactor = -1.0/dj; // as formula is with 1.0
    // rows
    number = updates->getNumElements();
    index = updates->getIndices();
    updateBy = updates->denseVector();
    weight = weights_+numberColumns;
    
    if (mode_<4||numberSwitched_>1||mode_>=10) {
      // Exact
      // now update weight update array
      model_->factorization()->updateColumnTranspose(spareRow2,
						     alternateWeights_);
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	// row has -1 
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double modification = other[iSequence];
	double pivotSquared = pivot * pivot;
	
	thisWeight += pivotSquared * devex_ + pivot * modification;
	if (thisWeight<TRY_NORM) {
	  if (mode_==1) {
	    // steepest
	    thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	  } else {
	    // exact
	    thisWeight = referenceIn*pivotSquared;
	    if (reference(iSequence+numberColumns))
	      thisWeight += 1.0;
	    thisWeight = max(thisWeight,TRY_NORM);
	  }
	}
	weight[iSequence] = thisWeight;
      }
    } else if (mode_==4) {
      // Devex
      assert (devex_>0.0);
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	// row has -1 
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double value = pivot * pivot*devex_;
	if (reference(iSequence+numberColumns))
	  value += 1.0;
	weight[iSequence] = max(0.99*thisWeight,value);
      }
    }
    
    // columns
    weight = weights_;
    
    scaleFactor = -scaleFactor;
    
    number = spareColumn1->getNumElements();
    index = spareColumn1->getIndices();
    updateBy = spareColumn1->denseVector();
    if (mode_<4||numberSwitched_>1||mode_>=10) {
      // Exact
      // get subset which have nonzero tableau elements
      model_->clpMatrix()->subsetTransposeTimes(model_,alternateWeights_,
						spareColumn1,
						spareColumn2);
      assert (spareColumn2->packedMode());
      double * updateBy2 = spareColumn2->denseVector();
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double modification = updateBy2[j];
	updateBy2[j]=0.0;
	double pivotSquared = pivot * pivot;
	
	thisWeight += pivotSquared * devex_ + pivot * modification;
	if (thisWeight<TRY_NORM) {
	  if (mode_==1) {
	    // steepest
	    thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	  } else {
	    // exact
	    thisWeight = referenceIn*pivotSquared;
	    if (reference(iSequence))
	      thisWeight += 1.0;
	    thisWeight = max(thisWeight,TRY_NORM);
	  }
	}
	weight[iSequence] = thisWeight;
      }
    } else if (mode_==4) {
      // Devex
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	// row has -1 
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double value = pivot * pivot*devex_;
	if (reference(iSequence))
	  value += 1.0;
	weight[iSequence] = max(0.99*thisWeight,value);
      }
    }
    // restore outgoing weight
    if (sequenceOut>=0)
      weights_[sequenceOut]=outgoingWeight;
    alternateWeights_->clear();
    spareColumn2->setNumElements(0);
    //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
    // check for accuracy
    int iCheck=229;
    //printf("weight for iCheck is %g\n",weights_[iCheck]);
    int numberRows = model_->numberRows();
    //int numberColumns = model_->numberColumns();
    for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
      if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	  !model_->getStatus(iCheck)!=ClpSimplex::isFixed)
	checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
    }
#endif
  }
  updates->setNumElements(0);
  spareColumn1->setNumElements(0);
}
// Update weights for Devex
void 
ClpPrimalColumnSteepest::justDevex(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int j;
  int number=0;
  int * index;
  double * updateBy;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  // for weights update we use pivotSequence
  pivotRow = pivotSequence_;
  assert (pivotRow>=0);
  // make sure infeasibility on incoming is 0.0
  const int * pivotVariable = model_->pivotVariable();
  int sequenceIn = pivotVariable[pivotRow];
  infeasible_->zero(sequenceIn);
  // and we can see if reference
  double referenceIn=0.0;
  if (mode_!=1&&reference(sequenceIn))
    referenceIn=1.0;
  // save outgoing weight round update
  double outgoingWeight=0.0;
  int sequenceOut = model_->sequenceOut();
  if (sequenceOut>=0)
    outgoingWeight=weights_[sequenceOut];
  assert (!updates->getNumElements());
  assert (!spareColumn1->getNumElements());
  // unset in case sub flip
  pivotSequence_=-1;
  // might as well set dj to 1
  dj = 1.0;
  updates->insert(pivotRow,-dj);
  model_->factorization()->updateColumnTranspose(spareRow2,updates);
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  double * weight;
  int numberColumns = model_->numberColumns();
  double scaleFactor = -1.0/dj; // as formula is with 1.0
  // rows
  number = updates->getNumElements();
  index = updates->getIndices();
  updateBy = updates->denseVector();
  weight = weights_+numberColumns;
  
  // Devex
  assert (devex_>0.0);
  for (j=0;j<number;j++) {
    int iSequence = index[j];
    double thisWeight = weight[iSequence];
    // row has -1 
    double pivot = updateBy[iSequence]*scaleFactor;
    updateBy[iSequence]=0.0;
    double value = pivot * pivot*devex_;
    if (reference(iSequence+numberColumns))
      value += 1.0;
    weight[iSequence] = max(0.99*thisWeight,value);
  }
  
  // columns
  weight = weights_;
  
  scaleFactor = -scaleFactor;
  
  number = spareColumn1->getNumElements();
  index = spareColumn1->getIndices();
  updateBy = spareColumn1->denseVector();
  // Devex
  for (j=0;j<number;j++) {
    int iSequence = index[j];
    double thisWeight = weight[iSequence];
    // row has -1 
    double pivot = updateBy[iSequence]*scaleFactor;
    updateBy[iSequence]=0.0;
    double value = pivot * pivot*devex_;
    if (reference(iSequence))
      value += 1.0;
    weight[iSequence] = max(0.99*thisWeight,value);
  }
  // restore outgoing weight
  if (sequenceOut>=0)
    weights_[sequenceOut]=outgoingWeight;
  spareColumn2->setNumElements(0);
  //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
  // check for accuracy
  int iCheck=229;
  //printf("weight for iCheck is %g\n",weights_[iCheck]);
  int numberRows = model_->numberRows();
  //int numberColumns = model_->numberColumns();
  for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
    if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	!model_->getStatus(iCheck)!=ClpSimplex::isFixed)
      checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
  }
#endif
  updates->setNumElements(0);
  spareColumn1->setNumElements(0);
}
// Update weights for Steepest
void 
ClpPrimalColumnSteepest::justSteepest(CoinIndexedVector * updates,
	       CoinIndexedVector * spareRow1,
	       CoinIndexedVector * spareRow2,
	       CoinIndexedVector * spareColumn1,
	       CoinIndexedVector * spareColumn2)
{
  int j;
  int number=0;
  int * index;
  double * updateBy;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  // for weights update we use pivotSequence
  pivotRow = pivotSequence_;
  // unset in case sub flip
  pivotSequence_=-1;
  assert (pivotRow>=0);
  // make sure infeasibility on incoming is 0.0
  const int * pivotVariable = model_->pivotVariable();
  int sequenceIn = pivotVariable[pivotRow];
  infeasible_->zero(sequenceIn);
  // and we can see if reference
  double referenceIn=0.0;
  if (mode_!=1&&reference(sequenceIn))
    referenceIn=1.0;
  // save outgoing weight round update
  double outgoingWeight=0.0;
  int sequenceOut = model_->sequenceOut();
  if (sequenceOut>=0)
    outgoingWeight=weights_[sequenceOut];
  assert (!updates->getNumElements());
  assert (!spareColumn1->getNumElements());
  // update weights
  //updates->setNumElements(0);
  //spareColumn1->setNumElements(0);
  // might as well set dj to 1
  dj = 1.0;
  updates->insert(pivotRow,-dj);
  model_->factorization()->updateColumnTranspose(spareRow2,updates);
  // put row of tableau in rowArray and columnArray
  model_->clpMatrix()->transposeTimes(model_,-1.0,
				      updates,spareColumn2,spareColumn1);
  double * weight;
  double * other = alternateWeights_->denseVector();
  int numberColumns = model_->numberColumns();
  double scaleFactor = -1.0/dj; // as formula is with 1.0
  // rows
  number = updates->getNumElements();
  index = updates->getIndices();
  updateBy = updates->denseVector();
  weight = weights_+numberColumns;
  
  // Exact
  // now update weight update array
  model_->factorization()->updateColumnTranspose(spareRow2,
						 alternateWeights_);
  for (j=0;j<number;j++) {
    int iSequence = index[j];
    double thisWeight = weight[iSequence];
    // row has -1 
    double pivot = updateBy[iSequence]*scaleFactor;
    updateBy[iSequence]=0.0;
    double modification = other[iSequence];
    double pivotSquared = pivot * pivot;
    
    thisWeight += pivotSquared * devex_ + pivot * modification;
    if (thisWeight<TRY_NORM) {
      if (mode_==1) {
	// steepest
	thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
      } else {
	// exact
	thisWeight = referenceIn*pivotSquared;
	if (reference(iSequence+numberColumns))
	  thisWeight += 1.0;
	thisWeight = max(thisWeight,TRY_NORM);
      }
    }
    weight[iSequence] = thisWeight;
  }
  
  // columns
  weight = weights_;
  
  scaleFactor = -scaleFactor;
  
  number = spareColumn1->getNumElements();
  index = spareColumn1->getIndices();
  updateBy = spareColumn1->denseVector();
  // Exact
  // get subset which have nonzero tableau elements
  model_->clpMatrix()->subsetTransposeTimes(model_,alternateWeights_,
					    spareColumn1,
					    spareColumn2);
  assert (spareColumn2->packedMode());
  double * updateBy2 = spareColumn2->denseVector();
  for (j=0;j<number;j++) {
    int iSequence = index[j];
    double thisWeight = weight[iSequence];
    double pivot = updateBy[iSequence]*scaleFactor;
    updateBy[iSequence]=0.0;
    double modification = updateBy2[j];
    updateBy2[j]=0.0;
    double pivotSquared = pivot * pivot;
    
    thisWeight += pivotSquared * devex_ + pivot * modification;
    if (thisWeight<TRY_NORM) {
      if (mode_==1) {
	// steepest
	thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
      } else {
	// exact
	thisWeight = referenceIn*pivotSquared;
	if (reference(iSequence))
	  thisWeight += 1.0;
	thisWeight = max(thisWeight,TRY_NORM);
      }
    }
    weight[iSequence] = thisWeight;
  }
  // restore outgoing weight
  if (sequenceOut>=0)
    weights_[sequenceOut]=outgoingWeight;
  alternateWeights_->clear();
  spareColumn2->setNumElements(0);
  //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
  // check for accuracy
  int iCheck=229;
  //printf("weight for iCheck is %g\n",weights_[iCheck]);
  int numberRows = model_->numberRows();
  //int numberColumns = model_->numberColumns();
  for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
    if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	!model_->getStatus(iCheck)!=ClpSimplex::isFixed)
      checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
  }
#endif
  updates->setNumElements(0);
  spareColumn1->setNumElements(0);
}
// Returns pivot column, -1 if none
int 
ClpPrimalColumnSteepest::pivotColumnOldMethod(CoinIndexedVector * updates,
				    CoinIndexedVector * spareRow1,
				    CoinIndexedVector * spareRow2,
				    CoinIndexedVector * spareColumn1,
				    CoinIndexedVector * spareColumn2)
{
  assert(model_);
  int iSection,j;
  int number=0;
  int * index;
  double * updateBy;
  double * reducedCost;
  // dj could be very small (or even zero - take care)
  double dj = model_->dualIn();
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  int pivotRow = model_->pivotRow();
  int anyUpdates;
  double * infeas = infeasible_->denseVector();

  // Local copy of mode so can decide what to do
  int switchType;
  if (mode_==4) 
    switchType = 5-numberSwitched_;
  else if (mode_>=10)
    switchType=3;
  else
    switchType = mode_;
  /* switchType - 
     0 - all exact devex
     1 - all steepest
     2 - some exact devex
     3 - auto some exact devex
     4 - devex
     5 - dantzig
  */
  if (updates->getNumElements()) {
    // would have to have two goes for devex, three for steepest
    anyUpdates=2;
    // add in pivot contribution
    if (pivotRow>=0) 
      updates->add(pivotRow,-dj);
  } else if (pivotRow>=0) {
    if (fabs(dj)>1.0e-15) {
      // some dj
      updates->insert(pivotRow,-dj);
      if (fabs(dj)>1.0e-6) {
	// reasonable size
	anyUpdates=1;
      } else {
	// too small
	anyUpdates=2;
      }
    } else {
      // zero dj
      anyUpdates=-1;
    }
  } else if (pivotSequence_>=0){
    // just after re-factorization
    anyUpdates=-1;
  } else {
    // sub flip - nothing to do
    anyUpdates=0;
  }

  if (anyUpdates>0) {
    model_->factorization()->updateColumnTranspose(spareRow2,updates);
    
    // put row of tableau in rowArray and columnArray
    model_->clpMatrix()->transposeTimes(model_,-1.0,
					updates,spareColumn2,spareColumn1);
    // normal
    for (iSection=0;iSection<2;iSection++) {
      
      reducedCost=model_->djRegion(iSection);
      int addSequence;
      
      if (!iSection) {
	number = updates->getNumElements();
	index = updates->getIndices();
	updateBy = updates->denseVector();
	addSequence = model_->numberColumns();
      } else {
	number = spareColumn1->getNumElements();
	index = spareColumn1->getIndices();
	updateBy = spareColumn1->denseVector();
	addSequence = 0;
      }
      if (!model_->nonLinearCost()->lookBothWays()) {

	for (j=0;j<number;j++) {
	  int iSequence = index[j];
	  double value = reducedCost[iSequence];
	  value -= updateBy[iSequence];
	  reducedCost[iSequence] = value;
	  ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
	  
	  switch(status) {
	    
	  case ClpSimplex::basic:
	    infeasible_->zero(iSequence+addSequence);
	  case ClpSimplex::isFixed:
	    break;
	  case ClpSimplex::isFree:
	  case ClpSimplex::superBasic:
	    if (fabs(value)>FREE_ACCEPT*tolerance) {
	      // we are going to bias towards free (but only if reasonable)
	      value *= FREE_BIAS;
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	    break;
	  case ClpSimplex::atUpperBound:
	    if (value>tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	    break;
	  case ClpSimplex::atLowerBound:
	    if (value<-tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	  }
	}
      } else {
	ClpNonLinearCost * nonLinear = model_->nonLinearCost(); 
	// We can go up OR down
	for (j=0;j<number;j++) {
	  int iSequence = index[j];
	  double value = reducedCost[iSequence];
	  value -= updateBy[iSequence];
	  reducedCost[iSequence] = value;
	  ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
	  
	  switch(status) {
	    
	  case ClpSimplex::basic:
	    infeasible_->zero(iSequence+addSequence);
	  case ClpSimplex::isFixed:
	    break;
	  case ClpSimplex::isFree:
	  case ClpSimplex::superBasic:
	    if (fabs(value)>FREE_ACCEPT*tolerance) {
	      // we are going to bias towards free (but only if reasonable)
	      value *= FREE_BIAS;
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	    break;
	  case ClpSimplex::atUpperBound:
	    if (value>tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      // look other way - change up should be negative
	      value -= nonLinear->changeUpInCost(iSequence+addSequence);
	      if (value>-tolerance) {
		infeasible_->zero(iSequence+addSequence);
	      } else {
		// store square in list
		if (infeas[iSequence+addSequence])
		  infeas[iSequence+addSequence] = value*value; // already there
		else
		  infeasible_->quickAdd(iSequence+addSequence,value*value);
	      }
	    }
	    break;
	  case ClpSimplex::atLowerBound:
	    if (value<-tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      // look other way - change down should be positive
	      value -= nonLinear->changeDownInCost(iSequence+addSequence);
	      if (value<tolerance) {
		infeasible_->zero(iSequence+addSequence);
	      } else {
		// store square in list
		if (infeas[iSequence+addSequence])
		  infeas[iSequence+addSequence] = value*value; // already there
		else
		  infeasible_->quickAdd(iSequence+addSequence,value*value);
	      }
	    }
	  }
	}
      }
    }
    if (anyUpdates==2) {
      // we can zero out as will have to get pivot row
      updates->clear();
      spareColumn1->clear();
    }
    if (pivotRow>=0) {
      // make sure infeasibility on incoming is 0.0
      int sequenceIn = model_->sequenceIn();
      infeasible_->zero(sequenceIn);
    }
  }
  // make sure outgoing from last iteration okay
  int sequenceOut = model_->sequenceOut();
  if (sequenceOut>=0) {
    ClpSimplex::Status status = model_->getStatus(sequenceOut);
    double value = model_->reducedCost(sequenceOut);
    
    switch(status) {
      
    case ClpSimplex::basic:
    case ClpSimplex::isFixed:
      break;
    case ClpSimplex::isFree:
    case ClpSimplex::superBasic:
      if (fabs(value)>FREE_ACCEPT*tolerance) { 
	// we are going to bias towards free (but only if reasonable)
	value *= FREE_BIAS;
	// store square in list
	if (infeas[sequenceOut])
	  infeas[sequenceOut] = value*value; // already there
	else
	  infeasible_->quickAdd(sequenceOut,value*value);
      } else {
	infeasible_->zero(sequenceOut);
      }
      break;
    case ClpSimplex::atUpperBound:
      if (value>tolerance) {
	// store square in list
	if (infeas[sequenceOut])
	  infeas[sequenceOut] = value*value; // already there
	else
	  infeasible_->quickAdd(sequenceOut,value*value);
      } else {
	infeasible_->zero(sequenceOut);
      }
      break;
    case ClpSimplex::atLowerBound:
      if (value<-tolerance) {
	// store square in list
	if (infeas[sequenceOut])
	  infeas[sequenceOut] = value*value; // already there
	else
	  infeasible_->quickAdd(sequenceOut,value*value);
      } else {
	infeasible_->zero(sequenceOut);
      }
    }
  }

  // If in quadratic re-compute all
  if (model_->algorithm()==2) {
    for (iSection=0;iSection<2;iSection++) {
      
      reducedCost=model_->djRegion(iSection);
      int addSequence;
      int iSequence;
      
      if (!iSection) {
	number = model_->numberRows();
	addSequence = model_->numberColumns();
      } else {
	number = model_->numberColumns();
	addSequence = 0;
      }
      
      if (!model_->nonLinearCost()->lookBothWays()) {
	for (iSequence=0;iSequence<number;iSequence++) {
	  double value = reducedCost[iSequence];
	  ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
	  
	  switch(status) {
	    
	  case ClpSimplex::basic:
	    infeasible_->zero(iSequence+addSequence);
	  case ClpSimplex::isFixed:
	    break;
	  case ClpSimplex::isFree:
	  case ClpSimplex::superBasic:
	    if (fabs(value)>tolerance) {
	      // we are going to bias towards free (but only if reasonable)
	      value *= FREE_BIAS;
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	    break;
	  case ClpSimplex::atUpperBound:
	    if (value>tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	    break;
	  case ClpSimplex::atLowerBound:
	    if (value<-tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	  }
	}
      } else {
	// we can go both ways
	ClpNonLinearCost * nonLinear = model_->nonLinearCost(); 
	for (iSequence=0;iSequence<number;iSequence++) {
	  double value = reducedCost[iSequence];
	  ClpSimplex::Status status = model_->getStatus(iSequence+addSequence);
	  
	  switch(status) {
	    
	  case ClpSimplex::basic:
	    infeasible_->zero(iSequence+addSequence);
	  case ClpSimplex::isFixed:
	    break;
	  case ClpSimplex::isFree:
	  case ClpSimplex::superBasic:
	    if (fabs(value)>tolerance) {
	      // we are going to bias towards free (but only if reasonable)
	      value *= FREE_BIAS;
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      infeasible_->zero(iSequence+addSequence);
	    }
	    break;
	  case ClpSimplex::atUpperBound:
	    if (value>tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      // look other way - change up should be negative
	      value -= nonLinear->changeUpInCost(iSequence+addSequence);
	      if (value>-tolerance) {
		infeasible_->zero(iSequence+addSequence);
	      } else {
		// store square in list
		if (infeas[iSequence+addSequence])
		  infeas[iSequence+addSequence] = value*value; // already there
		else
		  infeasible_->quickAdd(iSequence+addSequence,value*value);
	      }
	    }
	    break;
	  case ClpSimplex::atLowerBound:
	    if (value<-tolerance) {
	      // store square in list
	      if (infeas[iSequence+addSequence])
		infeas[iSequence+addSequence] = value*value; // already there
	      else
		infeasible_->quickAdd(iSequence+addSequence,value*value);
	    } else {
	      // look other way - change down should be positive
	      value -= nonLinear->changeDownInCost(iSequence+addSequence);
	      if (value<tolerance) {
		infeasible_->zero(iSequence+addSequence);
	      } else {
		// store square in list
		if (infeas[iSequence+addSequence])
		  infeas[iSequence+addSequence] = value*value; // already there
		else
		  infeasible_->quickAdd(iSequence+addSequence,value*value);
	      }
	    }
	  }
	}
      }
    }
  }
  // See what sort of pricing
  int numberWanted=0;
  number = infeasible_->getNumElements();
  int numberColumns = model_->numberColumns();
  if (switchType==5) {
    // we can zero out
    updates->clear();
    spareColumn1->clear();
    pivotSequence_=-1;
    pivotRow=-1;
    // See if to switch
    int numberElements = model_->factorization()->numberElements();
    int numberRows = model_->numberRows();
    // ratio is done on number of columns here
    //double ratio = (double) numberElements/(double) numberColumns;
    double ratio = (double) numberElements/(double) numberRows;
    //double ratio = (double) numberElements/(double) model_->clpMatrix()->getNumElements();
    if (ratio<0.1) {
      numberWanted = max(100,number/200);
    } else if (ratio<0.3) {
      numberWanted = max(500,number/40);
    } else if (ratio<0.5) {
      numberWanted = max(2000,number/10);
      numberWanted = max(numberWanted,numberColumns/30);
    } else {
      switchType=4;
      // initialize
      numberSwitched_++;
      // Make sure will re-do
      delete [] weights_;
      weights_=NULL;
      saveWeights(model_,4);
      printf("switching to devex %d nel ratio %g\n",numberElements,ratio);
      updates->clear();
    }
    if (model_->numberIterations()%1000==0)
      printf("numels %d ratio %g wanted %d\n",numberElements,ratio,numberWanted);
  }
  if(switchType==4) {
    // Still in devex mode
    int numberElements = model_->factorization()->numberElements();
    int numberRows = model_->numberRows();
    // ratio is done on number of rows here
    double ratio = (double) numberElements/(double) numberRows;
    // Go to steepest if lot of iterations?
    if (ratio<1.0) {
      numberWanted = max(2000,number/20);
    } else if (ratio<5.0) {
      numberWanted = max(2000,number/10);
      numberWanted = max(numberWanted,numberColumns/20);
    } else {
      // we can zero out
      updates->clear();
      spareColumn1->clear();
      switchType=3;
      // initialize
      pivotSequence_=-1;
      pivotRow=-1;
      numberSwitched_++;
      // Make sure will re-do
      delete [] weights_;
      weights_=NULL;
      saveWeights(model_,4);
      printf("switching to exact %d nel ratio %g\n",numberElements,ratio);
      updates->clear();
    }
    if (model_->numberIterations()%1000==0)
      printf("numels %d ratio %g wanted %d\n",numberElements,ratio,numberWanted);
  } 
  if (switchType<4) {
    if (switchType<2 ) {
      numberWanted = number+1;
    } else if (switchType==2) {
      numberWanted = max(2000,number/8);
    } else {
      int numberElements = model_->factorization()->numberElements();
      double ratio = (double) numberElements/(double) model_->numberRows();
      if (ratio<1.0) {
	numberWanted = max(2000,number/20);
      } else if (ratio<5.0) {
	numberWanted = max(2000,number/10);
	numberWanted = max(numberWanted,numberColumns/20);
      } else if (ratio<10.0) {
	numberWanted = max(2000,number/8);
	numberWanted = max(numberWanted,numberColumns/20);
      } else {
	ratio = number * (ratio/80.0);
	if (ratio>number) {
	  numberWanted=number+1;
	} else {
	  numberWanted = max(2000,(int) ratio);
	  numberWanted = max(numberWanted,numberColumns/10);
	}
      }
    }
  }
  // for weights update we use pivotSequence
  pivotRow = pivotSequence_;
  // unset in case sub flip
  pivotSequence_=-1;
  if (pivotRow>=0) {
    // make sure infeasibility on incoming is 0.0
    const int * pivotVariable = model_->pivotVariable();
    int sequenceIn = pivotVariable[pivotRow];
    infeasible_->zero(sequenceIn);
    // and we can see if reference
    double referenceIn=0.0;
    if (switchType!=1&&reference(sequenceIn))
      referenceIn=1.0;
    // save outgoing weight round update
    double outgoingWeight=0.0;
    if (sequenceOut>=0)
      outgoingWeight=weights_[sequenceOut];
    // update weights
    if (anyUpdates!=1) {
      updates->setNumElements(0);
      spareColumn1->setNumElements(0);
      // might as well set dj to 1
      dj = 1.0;
      updates->insert(pivotRow,-dj);
      model_->factorization()->updateColumnTranspose(spareRow2,updates);
      // put row of tableau in rowArray and columnArray
      model_->clpMatrix()->transposeTimes(model_,-1.0,
					  updates,spareColumn2,spareColumn1);
    }
    double * weight;
    double * other = alternateWeights_->denseVector();
    int numberColumns = model_->numberColumns();
    double scaleFactor = -1.0/dj; // as formula is with 1.0
    // rows
    number = updates->getNumElements();
    index = updates->getIndices();
    updateBy = updates->denseVector();
    weight = weights_+numberColumns;
      
    if (switchType<4) {
      // Exact
      // now update weight update array
      model_->factorization()->updateColumnTranspose(spareRow2,
						     alternateWeights_);
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	// row has -1 
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double modification = other[iSequence];
	double pivotSquared = pivot * pivot;
	
	thisWeight += pivotSquared * devex_ + pivot * modification;
	if (thisWeight<TRY_NORM) {
	  if (switchType==1) {
	    // steepest
	    thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	  } else {
	    // exact
	    thisWeight = referenceIn*pivotSquared;
	    if (reference(iSequence+numberColumns))
	      thisWeight += 1.0;
	    thisWeight = max(thisWeight,TRY_NORM);
	  }
	}
	weight[iSequence] = thisWeight;
      }
    } else if (switchType==4) {
      // Devex
      assert (devex_>0.0);
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	// row has -1 
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double value = pivot * pivot*devex_;
	if (reference(iSequence+numberColumns))
	  value += 1.0;
	weight[iSequence] = max(0.99*thisWeight,value);
      }
    }
    
    // columns
    weight = weights_;
    
    scaleFactor = -scaleFactor;
    
    number = spareColumn1->getNumElements();
    index = spareColumn1->getIndices();
    updateBy = spareColumn1->denseVector();
    if (switchType<4) {
      // Exact
      // get subset which have nonzero tableau elements
      model_->clpMatrix()->subsetTransposeTimes(model_,alternateWeights_,
						spareColumn1,
						spareColumn2);
      assert (spareColumn2->packedMode());
      double * updateBy2 = spareColumn2->denseVector();
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double modification = updateBy2[j];
	updateBy2[j]=0.0;
	double pivotSquared = pivot * pivot;
	
	thisWeight += pivotSquared * devex_ + pivot * modification;
	if (thisWeight<TRY_NORM) {
	  if (switchType==1) {
	    // steepest
	    thisWeight = max(TRY_NORM,ADD_ONE+pivotSquared);
	  } else {
	    // exact
	    thisWeight = referenceIn*pivotSquared;
	    if (reference(iSequence))
	      thisWeight += 1.0;
	    thisWeight = max(thisWeight,TRY_NORM);
	  }
	}
	weight[iSequence] = thisWeight;
      }
    } else if (switchType==4) {
      // Devex
      for (j=0;j<number;j++) {
	int iSequence = index[j];
	double thisWeight = weight[iSequence];
	// row has -1 
	double pivot = updateBy[iSequence]*scaleFactor;
	updateBy[iSequence]=0.0;
	double value = pivot * pivot*devex_;
	if (reference(iSequence))
	  value += 1.0;
	weight[iSequence] = max(0.99*thisWeight,value);
      }
    }
    // restore outgoing weight
    if (sequenceOut>=0)
      weights_[sequenceOut]=outgoingWeight;
    alternateWeights_->clear();
    spareColumn2->setNumElements(0);
    //#define SOME_DEBUG_1
#ifdef SOME_DEBUG_1
    // check for accuracy
    int iCheck=229;
    //printf("weight for iCheck is %g\n",weights_[iCheck]);
    int numberRows = model_->numberRows();
    //int numberColumns = model_->numberColumns();
    for (iCheck=0;iCheck<numberRows+numberColumns;iCheck++) {
      if (model_->getStatus(iCheck)!=ClpSimplex::basic&&
	  !model_->getStatus(iCheck)!=ClpSimplex::isFixed)
	checkAccuracy(iCheck,1.0e-1,updates,spareRow2);
    }
#endif
    updates->setNumElements(0);
    spareColumn1->setNumElements(0);
  }

  // update of duals finished - now do pricing


  double bestDj = 1.0e-30;
  int bestSequence=-1;

  int i,iSequence;
  index = infeasible_->getIndices();
  number = infeasible_->getNumElements();
  if(model_->numberIterations()<model_->lastBadIteration()+200) {
    // we can't really trust infeasibilities if there is dual error
    double checkTolerance = 1.0e-8;
    if (!model_->factorization()->pivots())
      checkTolerance = 1.0e-6;
    if (model_->largestDualError()>checkTolerance)
      tolerance *= model_->largestDualError()/checkTolerance;
    // But cap
    tolerance = min(1000.0,tolerance);
  }
#ifdef CLP_DEBUG
  if (model_->numberDualInfeasibilities()==1) 
    printf("** %g %g %g %x %x %d\n",tolerance,model_->dualTolerance(),
	   model_->largestDualError(),model_,model_->messageHandler(),
	   number);
#endif
  // stop last one coming immediately
  double saveOutInfeasibility=0.0;
  if (sequenceOut>=0) {
    saveOutInfeasibility = infeas[sequenceOut];
    infeas[sequenceOut]=0.0;
  }
  tolerance *= tolerance; // as we are using squares

  int iPass;
  // Setup two passes
  int start[4];
  start[1]=number;
  start[2]=0;
  double dstart = ((double) number) * CoinDrand48();
  start[0]=(int) dstart;
  start[3]=start[0];
  //double largestWeight=0.0;
  //double smallestWeight=1.0e100;
  for (iPass=0;iPass<2;iPass++) {
    int end = start[2*iPass+1];
    if (switchType<5) {
      for (i=start[2*iPass];i<end;i++) {
	iSequence = index[i];
	double value = infeas[iSequence];
	if (value>tolerance) {
	  double weight = weights_[iSequence];
	  //weight=1.0;
	  if (value>bestDj*weight) {
	    // check flagged variable and correct dj
	    if (!model_->flagged(iSequence)) {
	      bestDj=value/weight;
	      bestSequence = iSequence;
	    } else {
	      // just to make sure we don't exit before got something
	      numberWanted++;
	    }
	  }
	}
	numberWanted--;
	if (!numberWanted)
	  break;
      }
    } else {
      // Dantzig
      for (i=start[2*iPass];i<end;i++) {
	iSequence = index[i];
	double value = infeas[iSequence];
	if (value>tolerance) {
	  if (value>bestDj) {
	    // check flagged variable and correct dj
	    if (!model_->flagged(iSequence)) {
	      bestDj=value;
	      bestSequence = iSequence;
	    } else {
	      // just to make sure we don't exit before got something
	      numberWanted++;
	    }
	  }
	}
	numberWanted--;
	if (!numberWanted)
	  break;
      }
    }
    if (!numberWanted)
      break;
  }
  if (sequenceOut>=0) {
    infeas[sequenceOut]=saveOutInfeasibility;
  }
  /*if (model_->numberIterations()%100==0)
    printf("%d best %g\n",bestSequence,bestDj);*/
  
#ifdef CLP_DEBUG
  if (bestSequence>=0) {
    if (model_->getStatus(bestSequence)==ClpSimplex::atLowerBound)
      assert(model_->reducedCost(bestSequence)<0.0);
    if (model_->getStatus(bestSequence)==ClpSimplex::atUpperBound)
      assert(model_->reducedCost(bestSequence)>0.0);
  }
#endif
  return bestSequence;
}
/* 
   1) before factorization
   2) after factorization
   3) just redo infeasibilities
   4) restore weights
*/
void 
ClpPrimalColumnSteepest::saveWeights(ClpSimplex * model,int mode)
{
  model_ = model;
  if (mode_==4) {
    if (mode==1&&!weights_) 
      numberSwitched_=0; // Reset
  }
  // alternateWeights_ is defined as indexed but is treated oddly
  // at times
  int numberRows = model_->numberRows();
  int numberColumns = model_->numberColumns();
  const int * pivotVariable = model_->pivotVariable();
  if (mode==1&&weights_) {
    if (pivotSequence_>=0) {
      // save pivot order
      memcpy(alternateWeights_->getIndices(),pivotVariable,
	     numberRows*sizeof(int));
      // change from pivot row number to sequence number
      pivotSequence_=pivotVariable[pivotSequence_];
    } else {
      alternateWeights_->clear();
    }
    state_=1;
  } else if (mode==2||mode==4||mode==5) {
    // restore
    if (!weights_||state_==-1||mode==5) {
      // initialize weights
      delete [] weights_;
      delete alternateWeights_;
      weights_ = new double[numberRows+numberColumns];
      alternateWeights_ = new CoinIndexedVector();
      // enough space so can use it for factorization
      alternateWeights_->reserve(numberRows+
				 model_->factorization()->maximumPivots());
      initializeWeights();
      // create saved weights 
      delete [] savedWeights_;
      savedWeights_ = new double[numberRows+numberColumns];
      memcpy(savedWeights_,weights_,(numberRows+numberColumns)*
	     sizeof(double));
      savedPivotSequence_=-2;
      savedSequenceOut_ = -2;
      
    } else {
      if (mode!=4) {
	// save
	memcpy(savedWeights_,weights_,(numberRows+numberColumns)*
	       sizeof(double));
	savedPivotSequence_= pivotSequence_;
	savedSequenceOut_ = model_->sequenceOut();
      } else {
	// restore
	memcpy(weights_,savedWeights_,(numberRows+numberColumns)*
	       sizeof(double));
	// was - but I think should not be
	//pivotSequence_= savedPivotSequence_;
	//model_->setSequenceOut(savedSequenceOut_); 
	pivotSequence_= -1;
	model_->setSequenceOut(-1); 
      }
    }
    state_=0;
    // set up infeasibilities
    if (!infeasible_) {
      infeasible_ = new CoinIndexedVector();
      infeasible_->reserve(numberColumns+numberRows);
    }
  }
  if (mode>=2&&mode!=5) {
    if (mode!=3&&pivotSequence_>=0) {
      // restore pivot row
      int iRow;
      // permute alternateWeights
      double * temp = new double[numberRows+numberColumns];
      double * work = alternateWeights_->denseVector();
      int * oldPivotOrder = alternateWeights_->getIndices();
      for (iRow=0;iRow<numberRows;iRow++) {
	int iPivot=oldPivotOrder[iRow];
	temp[iPivot]=work[iRow];
      }
      int number=0;
      int found=-1;
      int * which = oldPivotOrder;
      // find pivot row and re-create alternateWeights
      for (iRow=0;iRow<numberRows;iRow++) {
	int iPivot=pivotVariable[iRow];
	if (iPivot==pivotSequence_) 
	  found=iRow;
	work[iRow]=temp[iPivot];
	if (work[iRow])
	  which[number++]=iRow;
      }
      alternateWeights_->setNumElements(number);
#ifdef CLP_DEBUG
      // Can happen but I should clean up
      assert(found>=0);
#endif
      pivotSequence_ = found;
      delete [] temp;
    }
    infeasible_->clear();
    double tolerance=model_->currentDualTolerance();
    int number = model_->numberRows() + model_->numberColumns();
    int iSequence;
   
    double * reducedCost = model_->djRegion();
      
    if (!model_->nonLinearCost()->lookBothWays()) {
      for (iSequence=0;iSequence<number;iSequence++) {
	double value = reducedCost[iSequence];
	ClpSimplex::Status status = model_->getStatus(iSequence);

	switch(status) {
	  
	case ClpSimplex::basic:
	case ClpSimplex::isFixed:
	  break;
	case ClpSimplex::isFree:
	case ClpSimplex::superBasic:
	  if (fabs(value)>FREE_ACCEPT*tolerance) { 
	    // we are going to bias towards free (but only if reasonable)
	    value *= FREE_BIAS;
	    // store square in list
	    infeasible_->quickAdd(iSequence,value*value);
	  }
	  break;
	case ClpSimplex::atUpperBound:
	  if (value>tolerance) {
	    infeasible_->quickAdd(iSequence,value*value);
	  }
	  break;
	case ClpSimplex::atLowerBound:
	  if (value<-tolerance) {
	    infeasible_->quickAdd(iSequence,value*value);
	  }
	}
      }
    } else {
      ClpNonLinearCost * nonLinear = model_->nonLinearCost(); 
      // can go both ways
      for (iSequence=0;iSequence<number;iSequence++) {
	double value = reducedCost[iSequence];
	ClpSimplex::Status status = model_->getStatus(iSequence);

	switch(status) {
	  
	case ClpSimplex::basic:
	case ClpSimplex::isFixed:
	  break;
	case ClpSimplex::isFree:
	case ClpSimplex::superBasic:
	  if (fabs(value)>FREE_ACCEPT*tolerance) { 
	    // we are going to bias towards free (but only if reasonable)
	    value *= FREE_BIAS;
	    // store square in list
	    infeasible_->quickAdd(iSequence,value*value);
	  }
	  break;
	case ClpSimplex::atUpperBound:
	  if (value>tolerance) {
	    infeasible_->quickAdd(iSequence,value*value);
	  } else {
	    // look other way - change up should be negative
	    value -= nonLinear->changeUpInCost(iSequence);
	    if (value<-tolerance) {
	      // store square in list
	      infeasible_->quickAdd(iSequence,value*value);
	    }
	  }
	  break;
	case ClpSimplex::atLowerBound:
	  if (value<-tolerance) {
	    infeasible_->quickAdd(iSequence,value*value);
	  } else {
	    // look other way - change down should be positive
	    value -= nonLinear->changeDownInCost(iSequence);
	    if (value>tolerance) {
	      // store square in list
	      infeasible_->quickAdd(iSequence,value*value);
	    }
	  }
	}
      }
    }
  }
}
// Gets rid of last update
void 
ClpPrimalColumnSteepest::unrollWeights()
{
  if (mode_==4&&!numberSwitched_)
    return;
  double * saved = alternateWeights_->denseVector();
  int number = alternateWeights_->getNumElements();
  int * which = alternateWeights_->getIndices();
  int i;
  for (i=0;i<number;i++) {
    int iRow = which[i];
    weights_[iRow]=saved[iRow];
    saved[iRow]=0.0;
  }
  alternateWeights_->setNumElements(0);
}
  
//-------------------------------------------------------------------
// Clone
//-------------------------------------------------------------------
ClpPrimalColumnPivot * ClpPrimalColumnSteepest::clone(bool CopyData) const
{
  if (CopyData) {
    return new ClpPrimalColumnSteepest(*this);
  } else {
    return new ClpPrimalColumnSteepest();
  }
}
void
ClpPrimalColumnSteepest::updateWeights(CoinIndexedVector * input)
{
  // Local copy of mode so can decide what to do
  int switchType = mode_;
  if (mode_==4&&numberSwitched_)
    switchType=3;
  else if (mode_==4)
    return;
  int number=input->getNumElements();
  int * which = input->getIndices();
  double * work = input->denseVector();
  int newNumber = 0;
  int * newWhich = alternateWeights_->getIndices();
  double * newWork = alternateWeights_->denseVector();
  int i;
  int sequenceIn = model_->sequenceIn();
  int sequenceOut = model_->sequenceOut();
  const int * pivotVariable = model_->pivotVariable();

  int pivotRow = model_->pivotRow();
  pivotSequence_ = pivotRow;

  devex_ =0.0;

  if (pivotRow>=0) {
    if (switchType==1) {
      for (i=0;i<number;i++) {
	int iRow = which[i];
	devex_ += work[iRow]*work[iRow];
	newWork[iRow] = -2.0*work[iRow];
      }
      newWork[pivotRow] = -2.0*max(devex_,0.0);
      devex_+=ADD_ONE;
      weights_[sequenceOut]=1.0+ADD_ONE;
      memcpy(newWhich,which,number*sizeof(int));
      alternateWeights_->setNumElements(number);
    } else {
      if (mode_!=4||numberSwitched_>1) {
	for (i=0;i<number;i++) {
	  int iRow = which[i];
	  int iPivot = pivotVariable[iRow];
	  if (reference(iPivot)) {
	    devex_ += work[iRow]*work[iRow];
	    newWork[iRow] = -2.0*work[iRow];
	    newWhich[newNumber++]=iRow;
	  }
	}
	if (!newWork[pivotRow])
	  newWhich[newNumber++]=pivotRow; // add if not already in
	newWork[pivotRow] = -2.0*max(devex_,0.0);
      } else {
	for (i=0;i<number;i++) {
	  int iRow = which[i];
	  int iPivot = pivotVariable[iRow];
	  if (reference(iPivot)) 
	    devex_ += work[iRow]*work[iRow];
	}
      }
      if (reference(sequenceIn)) {
	devex_+=1.0;
      } else {
      }
      if (reference(sequenceOut)) {
	weights_[sequenceOut]=1.0+1.0;
      } else {
	weights_[sequenceOut]=1.0;
      }
      alternateWeights_->setNumElements(newNumber);
    }
  } else {
    if (switchType==1) {
      for (i=0;i<number;i++) {
	int iRow = which[i];
	devex_ += work[iRow]*work[iRow];
      }
      devex_ += ADD_ONE;
    } else {
      for (i=0;i<number;i++) {
	int iRow = which[i];
	int iPivot = pivotVariable[iRow];
	if (reference(iPivot)) {
	  devex_ += work[iRow]*work[iRow];
	}
      }
      if (reference(sequenceIn)) 
	devex_+=1.0;
    }
  }
  double oldDevex = weights_[sequenceIn];
#ifdef CLP_DEBUG
  if ((model_->messageHandler()->logLevel()&32))
    printf("old weight %g, new %g\n",oldDevex,devex_);
#endif
  double check = max(devex_,oldDevex)+0.1;
  weights_[sequenceIn] = devex_;
  double testValue=0.1;
  if (mode_==4&&numberSwitched_==1)
    testValue = 0.5;
  if ( fabs ( devex_ - oldDevex ) > testValue * check ) {
#ifdef CLP_DEBUG
    if ((model_->messageHandler()->logLevel()&48)==16)
      printf("old weight %g, new %g\n",oldDevex,devex_);
#endif
    //printf("old weight %g, new %g\n",oldDevex,devex_);
    testValue=0.99;
    if (mode_==1) 
      testValue=1.01e1; // make unlikely to do if steepest
    else if (mode_==4&&numberSwitched_==1)
      testValue=0.9;
    double difference = fabs(devex_-oldDevex);
    if ( difference > testValue * check ) {
      // need to redo
      model_->messageHandler()->message(CLP_INITIALIZE_STEEP,
					*model_->messagesPointer())
					  <<oldDevex<<devex_
					  <<CoinMessageEol;
      initializeWeights();
    }
  }
  if (pivotRow>=0) {
    // set outgoing weight here
    weights_[model_->sequenceOut()]=devex_/(model_->alpha()*model_->alpha());
  }
}
// Checks accuracy - just for debug
void 
ClpPrimalColumnSteepest::checkAccuracy(int sequence,
				       double relativeTolerance,
				       CoinIndexedVector * rowArray1,
				       CoinIndexedVector * rowArray2)
{
  if (mode_==4&&!numberSwitched_)
    return;
  model_->unpack(rowArray1,sequence);
  model_->factorization()->updateColumn(rowArray2,rowArray1);
  int number=rowArray1->getNumElements();
  int * which = rowArray1->getIndices();
  double * work = rowArray1->denseVector();
  const int * pivotVariable = model_->pivotVariable();
  
  double devex =0.0;
  int i;

  if (mode_==1) {
    for (i=0;i<number;i++) {
      int iRow = which[i];
      devex += work[iRow]*work[iRow];
      work[iRow]=0.0;
    }
    devex += ADD_ONE;
  } else {
    for (i=0;i<number;i++) {
      int iRow = which[i];
      int iPivot = pivotVariable[iRow];
      if (reference(iPivot)) {
	devex += work[iRow]*work[iRow];
      }
      work[iRow]=0.0;
    }
    if (reference(sequence)) 
      devex+=1.0;
  }

  double oldDevex = weights_[sequence];
  double check = max(devex,oldDevex);;
  if ( fabs ( devex - oldDevex ) > relativeTolerance * check ) {
    printf("check %d old weight %g, new %g\n",sequence,oldDevex,devex);
    // update so won't print again
    weights_[sequence]=devex;
  }
  rowArray1->setNumElements(0);
}

// Initialize weights
void 
ClpPrimalColumnSteepest::initializeWeights()
{
  int numberRows = model_->numberRows();
  int numberColumns = model_->numberColumns();
  int number = numberRows + numberColumns;
  int iSequence;
  if (mode_!=1) {
    // initialize to 1.0 
    // and set reference framework
    if (!reference_) {
      int nWords = (number+31)>>5;
      reference_ = new unsigned int[nWords];
      // tiny overhead to zero out (stops valgrind complaining)
      memset(reference_,0,nWords*sizeof(int));
    }
    
    for (iSequence=0;iSequence<number;iSequence++) {
      weights_[iSequence]=1.0;
      if (model_->getStatus(iSequence)==ClpSimplex::basic) {
	setReference(iSequence,false);
      } else {
	setReference(iSequence,true);
      }
    }
  } else {
    CoinIndexedVector * temp = new CoinIndexedVector();
    temp->reserve(numberRows+
		  model_->factorization()->maximumPivots());
    double * array = alternateWeights_->denseVector();
    int * which = alternateWeights_->getIndices();
      
    for (iSequence=0;iSequence<number;iSequence++) {
      weights_[iSequence]=1.0+ADD_ONE;
      if (model_->getStatus(iSequence)!=ClpSimplex::basic&&
	  model_->getStatus(iSequence)!=ClpSimplex::isFixed) {
	model_->unpack(alternateWeights_,iSequence);
	double value=ADD_ONE;
	model_->factorization()->updateColumn(temp,alternateWeights_);
	int number = alternateWeights_->getNumElements();	int j;
	for (j=0;j<number;j++) {
	  int iRow=which[j];
	  value+=array[iRow]*array[iRow];
	  array[iRow]=0.0;
	}
	alternateWeights_->setNumElements(0);
	weights_[iSequence] = value;
      }
    }
    delete temp;
  }
}
// Gets rid of all arrays
void 
ClpPrimalColumnSteepest::clearArrays()
{
  delete [] weights_;
  weights_=NULL;
  delete infeasible_;
  infeasible_ = NULL;
  delete alternateWeights_;
  alternateWeights_ = NULL;
  delete [] savedWeights_;
  savedWeights_ = NULL;
  delete [] reference_;
  reference_ = NULL;
  pivotSequence_=-1;
  state_ = -1;
  savedPivotSequence_ = -1;
  savedSequenceOut_ = -1;  
  devex_ = 0.0;
}
// Returns true if would not find any column
bool 
ClpPrimalColumnSteepest::looksOptimal() const
{
  //**** THIS MUST MATCH the action coding above
  double tolerance=model_->currentDualTolerance();
  // we can't really trust infeasibilities if there is dual error
  // this coding has to mimic coding in checkDualSolution
  double error = min(1.0e-3,model_->largestDualError());
  // allow tolerance at least slightly bigger than standard
  tolerance = tolerance +  error;
  if(model_->numberIterations()<model_->lastBadIteration()+200) {
    // we can't really trust infeasibilities if there is dual error
    double checkTolerance = 1.0e-8;
    if (!model_->factorization()->pivots())
      checkTolerance = 1.0e-6;
    if (model_->largestDualError()>checkTolerance)
      tolerance *= model_->largestDualError()/checkTolerance;
    // But cap
    tolerance = min(1000.0,tolerance);
  }
  int number = model_->numberRows() + model_->numberColumns();
  int iSequence;
  
  double * reducedCost = model_->djRegion();
  int numberInfeasible=0;
  if (!model_->nonLinearCost()->lookBothWays()) {
    for (iSequence=0;iSequence<number;iSequence++) {
      double value = reducedCost[iSequence];
      ClpSimplex::Status status = model_->getStatus(iSequence);
      
      switch(status) {

      case ClpSimplex::basic:
      case ClpSimplex::isFixed:
	break;
      case ClpSimplex::isFree:
      case ClpSimplex::superBasic:
	if (fabs(value)>FREE_ACCEPT*tolerance) 
	  numberInfeasible++;
	break;
      case ClpSimplex::atUpperBound:
	if (value>tolerance) 
	  numberInfeasible++;
	break;
      case ClpSimplex::atLowerBound:
	if (value<-tolerance) 
	  numberInfeasible++;
      }
    }
  } else {
    ClpNonLinearCost * nonLinear = model_->nonLinearCost(); 
    // can go both ways
    for (iSequence=0;iSequence<number;iSequence++) {
      double value = reducedCost[iSequence];
      ClpSimplex::Status status = model_->getStatus(iSequence);
      
      switch(status) {

      case ClpSimplex::basic:
      case ClpSimplex::isFixed:
	break;
      case ClpSimplex::isFree:
      case ClpSimplex::superBasic:
	if (fabs(value)>FREE_ACCEPT*tolerance) 
	  numberInfeasible++;
	break;
      case ClpSimplex::atUpperBound:
	if (value>tolerance) {
	  numberInfeasible++;
	} else {
	  // look other way - change up should be negative
	  value -= nonLinear->changeUpInCost(iSequence);
	  if (value<-tolerance) 
	    numberInfeasible++;
	}
	break;
      case ClpSimplex::atLowerBound:
	if (value<-tolerance) {
	  numberInfeasible++;
	} else {
	  // look other way - change down should be positive
	  value -= nonLinear->changeDownInCost(iSequence);
	  if (value>tolerance) 
	    numberInfeasible++;
	}
      }
    }
  }
  return numberInfeasible==0;
}
/* Returns number of extra columns for sprint algorithm - 0 means off.
   Also number of iterations before recompute
*/
int 
ClpPrimalColumnSteepest::numberSprintColumns(int & numberIterations) const
{
  numberIterations=0;
  int numberAdd=0;
  if (!numberSwitched_&&mode_>=10) {
    numberIterations = min(2000,model_->numberRows()/5);
    numberIterations = max(numberIterations,model_->factorizationFrequency());
    numberIterations = max(numberIterations,500);
    if (mode_==10) {
      numberAdd=max(300,model_->numberColumns()/10);
      numberAdd=max(numberAdd,model_->numberRows()/5);
      // fake all
      //numberAdd=1000000;
      numberAdd = min(numberAdd,model_->numberColumns());
    } else {
      abort();
    }
  }
  return numberAdd;
}
// Switch off sprint idea
void 
ClpPrimalColumnSteepest::switchOffSprint()
{
  numberSwitched_=10;
}
