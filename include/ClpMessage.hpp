// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef ClpMessage_H
#define ClpMessage_H

#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif

/** This deals with Clp messages (as against Osi messages etc)
 */

#include "OsiMessageHandler.hpp"
enum CLP_Message
{
  CLP_SIMPLEX_FINISHED,
  CLP_SIMPLEX_INFEASIBLE,
  CLP_SIMPLEX_UNBOUNDED,
  CLP_SIMPLEX_STOPPED,
  CLP_SIMPLEX_ERROR,
  CLP_SIMPLEX_STATUS,
  CLP_DUAL_BOUNDS,
  CLP_SIMPLEX_ACCURACY,
  CLP_SIMPLEX_BADFACTOR,
  CLP_SIMPLEX_BOUNDTIGHTEN,
  CLP_SIMPLEX_INFEASIBILITIES,
  CLP_SIMPLEX_FLAG,
  CLP_SIMPLEX_GIVINGUP,
  CLP_DUAL_CHECKB,
  CLP_DUAL_ORIGINAL,
  CLP_SIMPLEX_PERTURB,
  CLP_PRIMAL_ORIGINAL,
  CLP_PRIMAL_WEIGHT,
  CLP_PRIMAL_OPTIMAL,
  CLP_SIMPLEX_HOUSE1,
  CLP_SIMPLEX_HOUSE2,
  CLP_SIMPLEX_NONLINEAR,
  CLP_SIMPLEX_FREEIN,
  CLP_SIMPLEX_PIVOTROW,
  CLP_DUAL_CHECK,
  CLP_PRIMAL_DJ,
  CLP_PACKEDSCALE_INITIAL,
  CLP_PACKEDSCALE_WHILE,
  CLP_PACKEDSCALE_FINAL,
  CLP_PACKEDSCALE_FORGET,
  CLP_INITIALIZE_STEEP,
  CLP_UNABLE_OPEN,
  CLP_IMPORT_RESULT,
  CLP_IMPORT_ERRORS,
  CLP_SINGULARITIES,
  CLP_DUMMY_END
};

class ClpMessage : public OsiMessages {

public:

  /**@name Constructors etc */
  //@{
  /** Constructor */
  ClpMessage(Language language=us_en);
  //@}

};

#endif
