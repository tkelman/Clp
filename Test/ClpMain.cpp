// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#include "CoinPragma.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <string>
#include <iostream>


#include "CoinPragma.hpp"
#include "CoinHelperFunctions.hpp"
#define CLPVERSION "0.98.5"

#include "CoinMpsIO.hpp"

#include "ClpFactorization.hpp"
#include "CoinTime.hpp"
#include "ClpSimplex.hpp"
#include "ClpSolve.hpp"
#include "ClpPackedMatrix.hpp"
#include "ClpPlusMinusOneMatrix.hpp"
#include "ClpNetworkMatrix.hpp"
#include "ClpDualRowSteepest.hpp"
#include "ClpDualRowDantzig.hpp"
#include "ClpPrimalColumnSteepest.hpp"
#include "ClpPrimalColumnDantzig.hpp"
#include "ClpPresolve.hpp"

// For Branch and bound
//  #include "OsiClpSolverInterface.hpp"
//  #include "OsiCuts.hpp"
//  #include "OsiRowCut.hpp"
//  #include "OsiColCut.hpp"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static double totalTime=0.0;

//#############################################################################

#ifdef NDEBUG
#undef NDEBUG
#endif

int mainTest (int argc, const char *argv[],bool doDual,
	      ClpSimplex empty, bool doPresolve,int doIdiot);
enum ClpParameterType {
  GENERALQUERY=-100,
  
  DUALTOLERANCE=1,PRIMALTOLERANCE,DUALBOUND,PRIMALWEIGHT,MAXTIME,OBJSCALE,

  LOGLEVEL=101,MAXFACTOR,PERTVALUE,MAXITERATION,PRESOLVEPASS,IDIOT,SPRINT,
  
  DIRECTION=201,DUALPIVOT,SCALING,ERRORSALLOWED,KEEPNAMES,SPARSEFACTOR,
  PRIMALPIVOT,PRESOLVE,CRASH,BIASLU,PERTURBATION,MESSAGES,
  
  DIRECTORY=301,IMPORT,EXPORT,RESTORE,SAVE,DUALSIMPLEX,PRIMALSIMPLEX,
  MAXIMIZE,MINIMIZE,EXIT,STDIN,UNITTEST,NETLIB_DUAL,NETLIB_PRIMAL,SOLUTION,
  TIGHTEN,FAKEBOUND,HELP,PLUSMINUS,NETWORK,ALLSLACK,

  INVALID=1000
};
static void printit(const char * input)
{
  int length =strlen(input);
  char temp[101];
  int i;
  int n=0;
  for (i=0;i<length;i++) {
    if (input[i]=='\n') {
      temp[n]='\0';
      std::cout<<temp<<std::endl;
      n=0;
    } else if (n>=65&&input[i]==' ') {
      temp[n]='\0';
      std::cout<<temp<<std::endl;
      n=0;
    } else if (n||input[i]!=' ') {
      temp[n++]=input[i];
    }
  }
  if (n) {
    temp[n]='\0';
    std::cout<<temp<<std::endl;
  }
}
/// Very simple class for setting parameters
class ClpItem {

public:

  /**@name Constructor and destructor */
  //@{
  /// Constructors
  ClpItem (  );
  ClpItem (std::string name, std::string help,
	   double lower, double upper, ClpParameterType type,bool display=true);
  ClpItem (std::string name, std::string help,
	   int lower, int upper, ClpParameterType type,bool display=true);
  // Other strings will be added by insert
  ClpItem (std::string name, std::string help, std::string firstValue,
	   ClpParameterType type,int defaultIndex=0,bool display=true);
  // Action
  ClpItem (std::string name, std::string help,
	   ClpParameterType type,int indexNumber=-1,bool display=true);
  /// Copy constructor. 
  ClpItem(const ClpItem &);
  /// Assignment operator. This copies the data
    ClpItem & operator=(const ClpItem & rhs);
  /// Destructor
  ~ClpItem (  );
  //@}

  /**@name stuff */
  //@{
  /// Insert string (only valid for keywords)
  void append(std::string keyWord);
  /// Adds one help line
  void addHelp(std::string keyWord);
  /// Returns name
  inline std::string  name(  ) const {
    return name_;
  };
  /// Returns short help
  inline std::string  shortHelp(  ) const {
    return shortHelp_;
  };
  /// Sets a double parameter (nonzero code if error)
  int setDoubleParameter(ClpSimplex * model, double value) const;
  /// Gets a double parameter
  double doubleParameter(ClpSimplex * model) const;
  /// Sets a int parameter (nonzero code if error)
  int setIntParameter(ClpSimplex * model, int value) const;
  /// Gets a int parameter
  int intParameter(ClpSimplex * model) const;
  /// Returns name which could match
  std::string matchName (  ) const;
  /// Returns parameter option which matches (-1 if none)
  int parameterOption ( std::string check ) const;
  /// Prints parameter options
  void printOptions (  ) const;
  /// Returns current parameter option
  inline std::string currentOption (  ) const
  { return definedKeyWords_[currentKeyWord_]; };
  /// Sets current parameter option
  inline void setCurrentOption ( int value )
  { currentKeyWord_=value; };
  /// Sets int value
  inline void setIntValue ( int value )
  { intValue_=value; };
  inline int intValue () const
  { return intValue_; };
  /// Sets double value
  inline void setDoubleValue ( double value )
  { doubleValue_=value; };
  inline double doubleValue () const
  { return doubleValue_; };
  /// Sets string value
  inline void setStringValue ( std::string value )
  { stringValue_=value; };
  inline std::string stringValue () const
  { return stringValue_; };
  /// Returns 1 if matches minimum, 2 if matches less, 0 if not matched
  int matches (std::string input) const;
  /// type
  inline ClpParameterType type() const
  { return type_;};
  /// whether to display
  inline bool displayThis() const
  { return display_;};
  /// Set Long help
  inline void setLonghelp(const std::string help) 
  {longHelp_=help;};
  /// Print Long help
  void printLongHelp() const;
  /// Print action and string
  void printString() const;
  /// type for classification
  inline int indexNumber() const
  { return indexNumber_;};
private:
  /// gutsOfConstructor
  void gutsOfConstructor();
  //@}
////////////////// data //////////////////
private:

  /**@name data 
   We might as well throw all type data in - could derive?
  */
  //@{
  // Type see ClpParameterType
  ClpParameterType type_;
  /// If double == okay
  double lowerDoubleValue_;
  double upperDoubleValue_;
  /// If int == okay
  int lowerIntValue_;
  int upperIntValue_;
  // Length of name
  unsigned int lengthName_;
  // Minimum match
  unsigned int lengthMatch_;
  /// set of valid strings
  std::vector<std::string> definedKeyWords_;
  /// Name
  std::string name_;
  /// Short help
  std::string shortHelp_;
  /// Long help
  std::string longHelp_;
  /// Action
  ClpParameterType action_;
  /// Current keyWord (if a keyword parameter)
  int currentKeyWord_;
  /// Display on ?
  bool display_;
  /// Integer parameter - current value
  int intValue_;
  /// Double parameter - current value
  double doubleValue_;
  /// String parameter - current value
  std::string stringValue_;
  /// index number to use for display purposes
  int indexNumber_;
  //@}
};
//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpItem::ClpItem () 
  : type_(INVALID),
    lowerDoubleValue_(0.0),
    upperDoubleValue_(0.0),
    lowerIntValue_(0),
    upperIntValue_(0),
    lengthName_(0),
    lengthMatch_(0),
    definedKeyWords_(),
    name_(),
    shortHelp_(),
    longHelp_(),
    action_(INVALID),
    currentKeyWord_(-1),
    display_(false),
    intValue_(-1),
    doubleValue_(-1.0),
    stringValue_(""),
    indexNumber_(INVALID)
{
}
// Other constructors
ClpItem::ClpItem (std::string name, std::string help,
	   double lower, double upper, ClpParameterType type,bool display)
  : type_(type),
    lowerIntValue_(0),
    upperIntValue_(0),
    definedKeyWords_(),
    name_(name),
    shortHelp_(help),
    longHelp_(),
    action_(type),
    currentKeyWord_(-1),
    display_(display),
    intValue_(-1),
    doubleValue_(-1.0),
    stringValue_(""),
    indexNumber_(type)
{
  lowerDoubleValue_ = lower;
  upperDoubleValue_ = upper;
  gutsOfConstructor();
}
ClpItem::ClpItem (std::string name, std::string help,
	   int lower, int upper, ClpParameterType type,bool display)
  : type_(type),
    lowerDoubleValue_(0.0),
    upperDoubleValue_(0.0),
    definedKeyWords_(),
    name_(name),
    shortHelp_(help),
    longHelp_(),
    action_(type),
    currentKeyWord_(-1),
    display_(display),
    intValue_(-1),
    doubleValue_(-1.0),
    stringValue_(""),
    indexNumber_(type)
{
  gutsOfConstructor();
  lowerIntValue_ = lower;
  upperIntValue_ = upper;
}
// Other strings will be added by append
ClpItem::ClpItem (std::string name, std::string help, 
		  std::string firstValue,
		  ClpParameterType type,
		  int defaultIndex,bool display)
  : type_(type),
    lowerDoubleValue_(0.0),
    upperDoubleValue_(0.0),
    lowerIntValue_(0),
    upperIntValue_(0),
    definedKeyWords_(),
    name_(name),
    shortHelp_(help),
    longHelp_(),
    action_(type),
    currentKeyWord_(defaultIndex),
    display_(display),
    intValue_(-1),
    doubleValue_(-1.0),
    stringValue_(""),
    indexNumber_(type)
{
  gutsOfConstructor();
  definedKeyWords_.push_back(firstValue);
}
// Action
ClpItem::ClpItem (std::string name, std::string help,
	   ClpParameterType type,int indexNumber,bool display)
  : type_(type),
    lowerDoubleValue_(0.0),
    upperDoubleValue_(0.0),
    lowerIntValue_(0),
    upperIntValue_(0),
    definedKeyWords_(),
    name_(name),
    shortHelp_(help),
    longHelp_(),
    action_(type),
    currentKeyWord_(-1),
    display_(display),
    intValue_(-1),
    doubleValue_(-1.0),
    stringValue_("")
{
  if (indexNumber<0)
    indexNumber_=type;
  else
    indexNumber_=indexNumber;
  gutsOfConstructor();
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpItem::ClpItem (const ClpItem & rhs) 
{  
  type_ = rhs.type_;
  lowerDoubleValue_ = rhs.lowerDoubleValue_;
  upperDoubleValue_ = rhs.upperDoubleValue_;
  lowerIntValue_ = rhs.lowerIntValue_;
  upperIntValue_ = rhs.upperIntValue_;
  lengthName_ = rhs.lengthName_;
  lengthMatch_ = rhs.lengthMatch_;
  definedKeyWords_ = rhs.definedKeyWords_;
  name_ = rhs.name_;
  shortHelp_ = rhs.shortHelp_;
  longHelp_ = rhs.longHelp_;
  action_ = rhs.action_;
  currentKeyWord_ = rhs.currentKeyWord_;
  display_=rhs.display_;
  intValue_=rhs.intValue_;
  doubleValue_=rhs.doubleValue_;
  stringValue_=rhs.stringValue_;
  indexNumber_=rhs.indexNumber_;
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpItem::~ClpItem ()
{
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpItem &
ClpItem::operator=(const ClpItem& rhs)
{
  if (this != &rhs) {
    type_ = rhs.type_;
    lowerDoubleValue_ = rhs.lowerDoubleValue_;
    upperDoubleValue_ = rhs.upperDoubleValue_;
    lowerIntValue_ = rhs.lowerIntValue_;
    upperIntValue_ = rhs.upperIntValue_;
    lengthName_ = rhs.lengthName_;
    lengthMatch_ = rhs.lengthMatch_;
    definedKeyWords_ = rhs.definedKeyWords_;
    name_ = rhs.name_;
    shortHelp_ = rhs.shortHelp_;
    longHelp_ = rhs.longHelp_;
    action_ = rhs.action_;
    currentKeyWord_ = rhs.currentKeyWord_;
    display_=rhs.display_;
    intValue_=rhs.intValue_;
    doubleValue_=rhs.doubleValue_;
    stringValue_=rhs.stringValue_;
    indexNumber_=rhs.indexNumber_;
  }
  return *this;
}
void 
ClpItem::gutsOfConstructor()
{
  unsigned int  shriekPos = name_.find('!');
  lengthName_ = name_.length();
  if ( shriekPos==std::string::npos ) {
    //does not contain '!'
    lengthMatch_= lengthName_;
  } else {
    lengthMatch_=shriekPos;
    name_ = name_.substr(0,shriekPos)+name_.substr(shriekPos+1);
    lengthName_--;
  }
}
// Insert string (only valid for keywords)
void 
ClpItem::append(std::string keyWord)
{
  definedKeyWords_.push_back(keyWord);
}

int 
ClpItem::matches (std::string input) const
{
  // look up strings to do more elegantly
  if (input.length()>lengthName_) {
    return 0;
  } else {
    unsigned int i;
    for (i=0;i<input.length();i++) {
      if (tolower(name_[i])!=tolower(input[i])) 
	break;
    }
    if (i<input.length()) {
      return 0;
    } else if (i>=lengthMatch_) {
      return 1;
    } else {
      // matched but too short
      return 2;
    }
  }
}
// Returns name which could match
std::string 
ClpItem::matchName (  ) const
{ 
  if (lengthMatch_==lengthName_) 
    return name_;
  else
    return name_.substr(0,lengthMatch_)+"("+name_.substr(lengthMatch_)+")";
}

// Returns parameter option which matches (-1 if none)
int 
ClpItem::parameterOption ( std::string check ) const
{
  int numberItems = definedKeyWords_.size();
  if (!numberItems) {
    return -1;
  } else {
    int whichItem=0;
    unsigned int it;
    for (it=0;it<definedKeyWords_.size();it++) {
      std::string thisOne = definedKeyWords_[it];
      unsigned int  shriekPos = thisOne.find('!');
      unsigned int length1 = thisOne.length();
      unsigned int length2 = length1;
      if ( shriekPos!=std::string::npos ) {
	//contains '!'
	length2 = shriekPos;
	thisOne = thisOne.substr(0,shriekPos)+
	  thisOne.substr(shriekPos+1);
	length1 = thisOne.length();
      }
      if (check.length()<=length1) {
	unsigned int i;
	for (i=0;i<check.length();i++) {
	  if (tolower(thisOne[i])!=tolower(check[i])) 
	    break;
	}
	if (i<check.length()) {
	  whichItem++;
	} else if (i>=length2) {
	  break;
	} 
      } else {
	whichItem++;
      }
    }
    if (whichItem<numberItems)
      return whichItem;
    else
      return -1;
  }
}
// Prints parameter options
void 
ClpItem::printOptions (  ) const
{
  std::cout<<"Possible options for "<<name_<<" are:"<<std::endl;
  unsigned int it;
  for (it=0;it<definedKeyWords_.size();it++) {
    std::string thisOne = definedKeyWords_[it];
    unsigned int  shriekPos = thisOne.find('!');
    if ( shriekPos!=std::string::npos ) {
      //contains '!'
      thisOne = thisOne.substr(0,shriekPos)+
	"("+thisOne.substr(shriekPos+1)+")";
    }
    std::cout<<thisOne<<std::endl;
  }
}
// Print action and string
void 
ClpItem::printString() const
{
  if (name_=="directory")
    std::cout<<"Current working directory is "<<stringValue_<<std::endl;
  else
    std::cout<<"Current default (if $ as parameter) for "<<name_
	     <<" is "<<stringValue_<<std::endl;
}
int
ClpItem::setDoubleParameter (ClpSimplex * model,double value) const
{
  double oldValue = doubleParameter(model);
  if (value<lowerDoubleValue_||value>upperDoubleValue_) {
    std::cout<<value<<" was provided for "<<name_<<
      " - valid range is "<<lowerDoubleValue_<<" to "<<
      upperDoubleValue_<<std::endl;
    return 1;
  } else {
    std::cout<<name_<<" was changed from "<<oldValue<<" to "
	     <<value<<std::endl;
    switch(type_) {
    case DUALTOLERANCE:
      model->setDualTolerance(value);
      break;
    case PRIMALTOLERANCE:
      model->setPrimalTolerance(value);
      break;
    case DUALBOUND:
      model->setDualBound(value);
      break;
    case PRIMALWEIGHT:
      model->setInfeasibilityCost(value);
      break;
    case MAXTIME:
      model->setMaximumSeconds(value);
      break;
    case OBJSCALE:
      model->setOptimizationDirection(value);
      break;
    default:
      abort();
    }
    return 0;
  }
}
double 
ClpItem::doubleParameter (ClpSimplex * model) const
{
  double value;
  switch(type_) {
  case DUALTOLERANCE:
    value=model->dualTolerance();
    break;
  case PRIMALTOLERANCE:
    value=model->primalTolerance();
    break;
  case DUALBOUND:
    value=model->dualBound();
    break;
  case PRIMALWEIGHT:
    value=model->infeasibilityCost();
    break;
  case MAXTIME:
    value=model->maximumSeconds();
    break;
  case OBJSCALE:
    value=model->optimizationDirection();
    break;
  default:
    abort();
  }
  return value;
}
int 
ClpItem::setIntParameter (ClpSimplex * model,int value) const
{
  int oldValue = intParameter(model);
  if (value<lowerIntValue_||value>upperIntValue_) {
    std::cout<<value<<" was provided for "<<name_<<
      " - valid range is "<<lowerIntValue_<<" to "<<
      upperIntValue_<<std::endl;
    return 1;
  } else {
    std::cout<<name_<<" was changed from "<<oldValue<<" to "
	     <<value<<std::endl;
    switch(type_) {
    case LOGLEVEL:
      model->setLogLevel(value);
      break;
    case MAXFACTOR:
      model->factorization()->maximumPivots(value);
      break;
    case PERTVALUE:
      model->setPerturbation(value);
      break;
    case MAXITERATION:
      model->setMaximumIterations(value);
      break;
    default:
      abort();
    }
    return 0;
  }
}
int 
ClpItem::intParameter (ClpSimplex * model) const
{
  int value;
  switch(type_) {
  case LOGLEVEL:
    value=model->logLevel();
    break;
  case MAXFACTOR:
    value=model->factorization()->maximumPivots();
    break;
    break;
  case PERTVALUE:
    value=model->perturbation();
    break;
  case MAXITERATION:
    value=model->maximumIterations();
    break;
  default:
    value=-1;
    break;
  }
  return value;
}
// Print Long help
void 
ClpItem::printLongHelp() const
{
  if (type_>=1&&type_<400) {
    if (type_<LOGLEVEL) {
      printf("Range of values is %g to %g\n",lowerDoubleValue_,upperDoubleValue_);
    } else if (type_<DIRECTION) {
      printf("Range of values is %d to %d\n",lowerIntValue_,upperIntValue_);
    } else if (type_<DIRECTORY) {
      printOptions();
    }
    printit(longHelp_.c_str());
  }
}
#ifdef READLINE     
#include <readline/readline.h>
#include <readline/history.h>
#endif
// Returns next valid field
static int read_mode=1;
static char line[1000];
static char * where=NULL;

std::string
nextField()
{
  std::string field;
  if (!where) {
    // need new line
#ifdef READLINE     
    // Get a line from the user. 
    where = readline ("Clp:");
     
    // If the line has any text in it, save it on the history.
    if (where) {
      if ( *where)
	add_history (where);
      strcpy(line,where);
    }
#else
    fprintf(stdout,"Clp:");
    fflush(stdout);
    where = fgets(line,1000,stdin);
#endif
    if (!where)
      return field; // EOF
    where = line;
    // clean image
    char * lastNonBlank = line-1;
    while ( *where != '\0' ) {
      if ( *where != '\t' && *where < ' ' ) {
	break;
      } else if ( *where != '\t' && *where != ' ') {
	lastNonBlank = where;
      }
      where++;
    }
    where=line;
    *(lastNonBlank+1)='\0';
  }
  // munch white space
  while(*where==' '||*where=='\t')
    where++;
  char * saveWhere = where;
  while (*where!=' '&&*where!='\t'&&*where!='\0')
    where++;
  if (where!=saveWhere) {
    char save = *where;
    *where='\0';
    //convert to string
    field=saveWhere;
    *where=save;
  } else {
    where=NULL;
    field="EOL";
  }
  return field;
}

std::string
getCommand(int argc, const char *argv[])
{
  std::string field="EOL";
  while (field=="EOL") {
    if (read_mode>0) {
      if (read_mode<argc) {
	field = argv[read_mode++];
	if (field=="-") {
	  std::cout<<"Switching to line mode"<<std::endl;
	  read_mode=-1;
	  field=nextField();
	} else if (field[0]!='-') {
	  if (read_mode!=2) {
	    std::cout<<"skipping non-command "<<field<<std::endl;
	    field="EOL"; // skip
	  } else {
	    // special dispensation - taken as -import name
	    read_mode--;
	    field="import";
	  }
	} else {
	  if (field!="--") {
	    // take off -
	    field = field.substr(1);
	  } else {
	    // special dispensation - taken as -import --
	    read_mode--;
	    field="import";
	  }
	}
      } else {
	field="";
      }
    } else {
      field=nextField();
    }
  }
  //std::cout<<field<<std::endl;
  return field;
}
std::string
getString(int argc, const char *argv[])
{
  std::string field="EOL";
  if (read_mode>0) {
    if (read_mode<argc) {
      if (argv[read_mode][0]!='-') { 
	field = argv[read_mode++];
      } else if (!strcmp(argv[read_mode],"--")) {
	field = argv[read_mode++];
	// -- means import from stdin
	field = "-";
      }
    }
  } else {
    field=nextField();
  }
  //std::cout<<field<<std::endl;
  return field;
}
// valid 0 - okay, 1 bad, 2 not there
int
getIntField(int argc, const char *argv[],int * valid)
{
  std::string field="EOL";
  if (read_mode>0) {
    if (read_mode<argc) {
      // may be negative value so do not check for -
      field = argv[read_mode++];
    }
  } else {
    field=nextField();
  }
  int value=0;
  //std::cout<<field<<std::endl;
  if (field!="EOL") {
    // how do I check valid
    value =  atoi(field.c_str());
    *valid=0;
  } else {
    *valid=2;
  }
  return value;
}
double
getDoubleField(int argc, const char *argv[],int * valid)
{
  std::string field="EOL";
  if (read_mode>0) {
    if (read_mode<argc) {
      // may be negative value so do not check for -
      field = argv[read_mode++];
    }
  } else {
    field=nextField();
  }
  double value=0.0;
  //std::cout<<field<<std::endl;
  if (field!="EOL") {
    // how do I check valid
    value = atof(field.c_str());
    *valid=0;
  } else {
    *valid=2;
  }
  return value;
}
int main (int argc, const char *argv[])
{
  // next {} is just to make sure all memory should be freed - for debug
  {
    double time1 = CoinCpuTime(),time2;
    // Set up all non-standard stuff
    //int numberModels=1;
    ClpSimplex * models = new ClpSimplex[1];
    
    // default action on import
    int allowImportErrors=0;
    int keepImportNames=1;
    int doIdiot=-2;
    int doCrash=0;
    int doSprint=-1;
    // set reasonable defaults
    int preSolve=5;
    models->setPerturbation(50);
    models->messageHandler()->setPrefix(false);
    std::string directory ="./";
    std::string importFile ="";
    std::string exportFile ="default.mps";
    std::string saveFile ="default.prob";
    std::string restoreFile ="default.prob";
    std::string solutionFile ="default.sol";
#define MAXPARAMETERS 100
    ClpItem parameters[MAXPARAMETERS];
    int numberParameters=0;
    parameters[numberParameters++]=
      ClpItem("?","For help",GENERALQUERY,-1,false);
    parameters[numberParameters++]=
      ClpItem("-","From stdin",
	      STDIN,299,false);
    parameters[numberParameters++]=
      ClpItem("allS!lack","Set basis back to all slack",
	      ALLSLACK);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("biasLU","Whether factorization biased towards U",
	      "UU",BIASLU,2,false);
    parameters[numberParameters-1].append("UX");
    parameters[numberParameters-1].append("LX");
    parameters[numberParameters-1].append("LL");
    parameters[numberParameters++]=
      ClpItem("crash","Whether to create basis for problem",
	      "off",CRASH);
    parameters[numberParameters-1].append("on");
    parameters[numberParameters-1].setLonghelp
      (
       "If crash is set on and there is an all slack basis then Clp will put structural\
variables into basis with the aim of getting dual feasible.  On the whole dual seems to be\
better without it and there alernative types of 'crash' for primal e.g. 'idiot' or 'sprint'."
       ); 
    parameters[numberParameters++]=
      ClpItem("direction","Minimize or Maximize",
	      "min!imize",DIRECTION);
    parameters[numberParameters-1].append("max!imize");
    parameters[numberParameters-1].append("zero");
    parameters[numberParameters-1].setLonghelp
      (
       "The default is minimize - use 'direction maximize' for maximization.\n\
You can also use the parameters 'maximize' or 'minimize'."
       ); 
    parameters[numberParameters++]=
      ClpItem("directory","Set Default directory for import etc.",
	      DIRECTORY,299);
    parameters[numberParameters-1].setLonghelp
      (
       "This sets the directory which import, export, saveModel and restoreModel will use"
       ); 
    parameters[numberParameters-1].setStringValue(directory);
    parameters[numberParameters++]=
      ClpItem("dualB!ound","Initially algorithm acts as if no \
gap between bounds exceeds this value",
	      1.0e-20,1.0e12,DUALBOUND);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       );
    parameters[numberParameters-1].setDoubleValue(models->dualBound());
    parameters[numberParameters++]=
      ClpItem("dualP!ivot","Dual pivot choice algorithm",
	      "auto!matic",DUALPIVOT);
    parameters[numberParameters-1].append("dant!zig");
    parameters[numberParameters-1].append("partial");
    parameters[numberParameters-1].append("steep!est");
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("dualS!implex","Do dual simplex algorithm",
	      DUALSIMPLEX);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("dualT!olerance","For an optimal solution \
no dual infeasibility may exceed this value",
	      1.0e-20,1.0e12,DUALTOLERANCE);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setDoubleValue(models->dualTolerance());
    parameters[numberParameters++]=
      ClpItem("end","Stops clp execution",
	      EXIT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("error!sAllowed","Whether to allow import errors",
	      "off",ERRORSALLOWED);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       );
    parameters[numberParameters++]=
      ClpItem("exit","Stops clp execution",
	      EXIT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("export","Export model as mps file",
	      EXPORT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setStringValue(exportFile);
    parameters[numberParameters++]=
      ClpItem("fakeB!ound","All bounds <= this value - DEBUG",
	      1.0,1.0e15,FAKEBOUND,false);
    parameters[numberParameters++]=
      ClpItem("help","Print out version, non-standard options and some help",
	      HELP);
    parameters[numberParameters++]=
      ClpItem("idiot!Crash","Whether to try idiot crash",
	      -2,200,IDIOT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setIntValue(doIdiot);
    parameters[numberParameters++]=
      ClpItem("import","Import model from mps file",
	      IMPORT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setStringValue(importFile);
    parameters[numberParameters++]=
      ClpItem("keepN!ames","Whether to keep names from import",
	      "on",KEEPNAMES);
    parameters[numberParameters-1].append("off");
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("log!Level","Level of detail in output",
	      0,63,LOGLEVEL);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setIntValue(models->logLevel());
    parameters[numberParameters++]=
      ClpItem("max!imize","Set optimization direction to maximize",
	      MAXIMIZE,299);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("maxF!actor","Maximum number of iterations between \
refactorizations",
	      1,999999,MAXFACTOR);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setIntValue(models->factorizationFrequency());
    parameters[numberParameters++]=
      ClpItem("maxIt!erations","Maximum number of iterations before \
stopping",
	      0,99999999,MAXITERATION);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setIntValue(models->maximumIterations());
    parameters[numberParameters++]=
      ClpItem("min!imize","Set optimization direction to minimize",
	      MINIMIZE,299);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("mess!ages","Controls if Clpnnnn is printed",
	      "off",MESSAGES);
    parameters[numberParameters-1].append("on");
    parameters[numberParameters-1].setLonghelp
      (""
       ); 
    parameters[numberParameters++]=
      ClpItem("netlib","Solve entire netlib test set",
	      NETLIB_DUAL,-1,false);
    parameters[numberParameters++]=
      ClpItem("netlibP!rimal","Solve entire netlib test set (primal)",
	      NETLIB_PRIMAL,-1,false);
    parameters[numberParameters++]=
      ClpItem("network","Tries to make network matrix",
	      NETWORK,-1,false);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("objective!Scale","Scale factor to apply to objective",
	      -1.0e20,1.0e20,OBJSCALE,false);
    parameters[numberParameters-1].setDoubleValue(models->optimizationDirection());
    parameters[numberParameters++]=
      ClpItem("passP!resolve","How many passes in presolve",
	      0,100,PRESOLVEPASS);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setIntValue(preSolve);
    parameters[numberParameters++]=
      ClpItem("pertV!alue","Method of perturbation",
	      -5000,102,PERTVALUE,false);
    parameters[numberParameters-1].setIntValue(models->perturbation());
    parameters[numberParameters++]=
      ClpItem("perturb!ation","Whether to perturb problem",
	      "on",PERTURBATION);
    parameters[numberParameters-1].append("off");
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("plus!Minus","Tries to make +- 1 matrix",
	      PLUSMINUS,-1,false);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("presolve","Whether to presolve problem",
	      "on",PRESOLVE);
    parameters[numberParameters-1].append("off");
    parameters[numberParameters-1].append("more");
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("primalP!ivot","Primal pivot choice algorithm",
	      "auto!matic",PRIMALPIVOT);
    parameters[numberParameters-1].append("exa!ct");
    parameters[numberParameters-1].append("dant!zig");
    parameters[numberParameters-1].append("part!ial");
    parameters[numberParameters-1].append("steep!est");
    //parameters[numberParameters-1].append("change");
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("primalS!implex","Do primal simplex algorithm",
	      PRIMALSIMPLEX);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("primalT!olerance","For an optimal solution \
no primal infeasibility may exceed this value",
	      1.0e-20,1.0e12,PRIMALTOLERANCE);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setDoubleValue(models->primalTolerance());
    parameters[numberParameters++]=
      ClpItem("primalW!eight","Initially algorithm acts as if it \
costs this much to be infeasible",
	      1.0e-20,1.0e12,PRIMALWEIGHT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setDoubleValue(models->infeasibilityCost());
    parameters[numberParameters++]=
      ClpItem("quit","Stops clp execution",
	      EXIT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("restore!Model","Restore model from binary file",
	      RESTORE);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setStringValue(restoreFile);
    parameters[numberParameters++]=
      ClpItem("save!Model","Save model to binary file",
	      SAVE);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setStringValue(saveFile);
    parameters[numberParameters++]=
      ClpItem("scal!ing","Whether to scale problem",
	      "on",SCALING);
    parameters[numberParameters-1].append("off");
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("sec!onds","maximum seconds",
	      0.0,1.0e12,MAXTIME);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setDoubleValue(models->maximumSeconds());
    parameters[numberParameters++]=
      ClpItem("sol!ution","Prints solution to file",
	      SOLUTION);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setStringValue(solutionFile);
    parameters[numberParameters++]=
      ClpItem("spars!eFactor","Whether factorization treated as sparse",
	      "on",SPARSEFACTOR,0,false);
    parameters[numberParameters-1].append("off");
    parameters[numberParameters-1].append("on");
    parameters[numberParameters++]=
      ClpItem("sprint!Crash","Whether to try sprint crash",
	      0,200,SPRINT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters-1].setIntValue(doSprint);
      ClpItem("stdin","From stdin",
	      STDIN,-1,false);
    parameters[numberParameters++]=
      ClpItem("stop","Stops clp execution",
	      EXIT);
    parameters[numberParameters-1].setLonghelp
      (
       ""
       ); 
    parameters[numberParameters++]=
      ClpItem("tight!en","Poor person's preSolve for now",
	      TIGHTEN,-1,false);
    parameters[numberParameters++]=
      ClpItem("unitTest","Do unit test",
	      UNITTEST,-1,false);
    assert(numberParameters<MAXPARAMETERS);
    
    // total number of commands read
    int numberGoodCommands=0;
    bool * goodModels = new bool[1];
    int getNewMatrix=0;
    
    
    int iModel=0;
    goodModels[0]=false;
    //models[0].scaling(1);
    //models[0].setDualBound(1.0e6);
    //models[0].setDualTolerance(1.0e-7);
    //ClpDualRowSteepest steep;
    //models[0].setDualRowPivotAlgorithm(steep);
    //models[0].setPrimalTolerance(1.0e-7);
    //ClpPrimalColumnSteepest steepP;
    //models[0].setPrimalColumnPivotAlgorithm(steepP);
    std::string field;
    std::cout<<"Coin LP version "<<CLPVERSION
	     <<", build "<<__DATE__<<std::endl;
    
    while (1) {
      // next command
      field=getCommand(argc,argv);
      
      // exit if null or similar
      if (!field.length()) {
	if (numberGoodCommands==1&&goodModels[0]) {
	  // we just had file name - do dual
	  field="duals";
	} else if (!numberGoodCommands) {
	  // let's give the sucker a hint
	  std::cout
	    <<"Clp takes input from arguments ( - switches to stdin)"
	    <<std::endl
	    <<"Enter ? for list of commands or help"<<std::endl;
	  field="-";
	} else {
	  break;
	}
      }
      
      // see if ? at end
      int numberQuery=0;
      if (field!="?") {
	int length = field.length();
	int i;
	for (i=length-1;i>0;i--) {
	  if (field[i]=='?') 
	    numberQuery++;
	  else
	    break;
	}
	field=field.substr(0,length-numberQuery);
      }
      // find out if valid command
      int iParam;
      int numberMatches=0;
      int firstMatch=-1;
      for ( iParam=0; iParam<numberParameters; iParam++ ) {
	int match = parameters[iParam].matches(field);
	if (match==1) {
	  numberMatches = 1;
	  firstMatch=iParam;
	  break;
	} else {
	  if (match&&firstMatch<0)
	    firstMatch=iParam;
	  numberMatches += match>>1;
	}
      }
      if (iParam<numberParameters&&!numberQuery) {
	// found
	ClpItem found = parameters[iParam];
	ClpParameterType type = found.type();
	int valid;
	numberGoodCommands++;
	if (type==GENERALQUERY) {
	  std::cout<<"In argument list keywords have leading - "
	    ", -stdin or just - switches to stdin"<<std::endl;
	  std::cout<<"One command per line (and no -)"<<std::endl;
	  std::cout<<"abcd? gives list of possibilities, if only one + explanation"<<std::endl;
	  std::cout<<"abcd?? adds explanation, if only one fuller help(LATER)"<<std::endl;
	  std::cout<<"abcd without value (where expected) gives current value"<<std::endl;
	  std::cout<<"abcd value sets value"<<std::endl;
	  std::cout<<"Commands are:"<<std::endl;
	  int maxAcross=5;
	  int limits[]={1,101,201,301,401};
	  std::vector<std::string> types;
	  types.push_back("Double parameters:");
	  types.push_back("Int parameters:");
	  types.push_back("Keyword parameters and others:");
	  types.push_back("Actions:");
	  int iType;
	  for (iType=0;iType<4;iType++) {
	    int across=0;
	    std::cout<<types[iType]<<std::endl;
	    for ( iParam=0; iParam<numberParameters; iParam++ ) {
	      int type = parameters[iParam].indexNumber();
	      if (parameters[iParam].displayThis()&&type>=limits[iType]
		  &&type<limits[iType+1]) {
		if (!across)
		  std::cout<<"  ";
		std::cout<<parameters[iParam].matchName()<<"  ";
		across++;
		if (across==maxAcross) {
		  std::cout<<std::endl;
		  across=0;
		}
	      }
	    }
	    if (across)
	      std::cout<<std::endl;
	  }
	} else if (type<101) {
	  // get next field as double
	  double value = getDoubleField(argc,argv,&valid);
	  if (!valid) {
	    parameters[iParam].setDoubleValue(value);
	    parameters[iParam].setDoubleParameter(models+iModel,value);
	  } else if (valid==1) {
	    abort();
	  } else {
	    std::cout<<parameters[iParam].name()<<" has value "<<
	      parameters[iParam].doubleValue()<<std::endl;
	  }
	} else if (type<201) {
	  // get next field as int
	  int value = getIntField(argc,argv,&valid);
	  if (!valid) {
	    parameters[iParam].setIntValue(value);
	    if (parameters[iParam].type()==PRESOLVEPASS)
	      preSolve = value;
	    else if (parameters[iParam].type()==IDIOT)
	      doIdiot = value;
	    else if (parameters[iParam].type()==SPRINT)
	      doSprint = value;
	    else
	      parameters[iParam].setIntParameter(models+iModel,value);
	  } else if (valid==1) {
	    abort();
	  } else {
	    std::cout<<parameters[iParam].name()<<" has value "<<
	      parameters[iParam].intValue()<<std::endl;
	  }
	} else if (type<301) {
	  // one of several strings
	  std::string value = getString(argc,argv);
	  int action = parameters[iParam].parameterOption(value);
	  if (action<0) {
	    if (value!="EOL") {
	      // no match
	      parameters[iParam].printOptions();
	    } else {
	      // print current value
	      std::cout<<parameters[iParam].name()<<" has value "<<
		parameters[iParam].currentOption()<<std::endl;
	    }
	  } else {
	    parameters[iParam].setCurrentOption(action);
	    // for now hard wired
	    switch (type) {
	    case DIRECTION:
	      if (action==0)
		models[iModel].setOptimizationDirection(1);
	      else if (action==1)
		models[iModel].setOptimizationDirection(-1);
	      else
		models[iModel].setOptimizationDirection(0);
	      break;
	    case DUALPIVOT:
	      if (action==0) {
		ClpDualRowSteepest steep(3);
		models[iModel].setDualRowPivotAlgorithm(steep);
	      } else if (action==1) {
		ClpDualRowDantzig dantzig;
		models[iModel].setDualRowPivotAlgorithm(dantzig);
	      } else if (action==2) {
		// partial steep
		ClpDualRowSteepest steep(2);
		models[iModel].setDualRowPivotAlgorithm(steep);
	      } else {
		ClpDualRowSteepest steep;
		models[iModel].setDualRowPivotAlgorithm(steep);
	      }
	      break;
	    case PRIMALPIVOT:
	      if (action==0) {
		ClpPrimalColumnSteepest steep(3);
		models[iModel].setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==1) {
		ClpPrimalColumnSteepest steep(0);
		models[iModel].setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==2) {
		ClpPrimalColumnDantzig dantzig;
		models[iModel].setPrimalColumnPivotAlgorithm(dantzig);
	      } else if (action==3) {
		ClpPrimalColumnSteepest steep(2);
		models[iModel].setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==4) {
		ClpPrimalColumnSteepest steep(0);
		models[iModel].setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==5) {
		ClpPrimalColumnSteepest steep(4);
		models[iModel].setPrimalColumnPivotAlgorithm(steep);
	      }
	      break;
	    case SCALING:
	      models[iModel].scaling(1-action);
	      break;
	    case SPARSEFACTOR:
	      models[iModel].setSparseFactorization((1-action)!=0);
	      break;
	    case BIASLU:
	      models[iModel].factorization()->setBiasLU(action);
	      break;
	    case PERTURBATION:
	      if (action==0)
		models[iModel].setPerturbation(50);
	      else
		models[iModel].setPerturbation(100);
	      break;
	    case ERRORSALLOWED:
	      allowImportErrors = action;
	      break;
	    case KEEPNAMES:
	      keepImportNames = 1-action;
	      break;
	    case PRESOLVE:
	      if (action==0)
		preSolve = 5;
	      else if (action==1)
		preSolve=0;
	      else
		preSolve=10;
	      break;
	    case CRASH:
	      doCrash=-action;
	      break;
	    case MESSAGES:
	      models[iModel].messageHandler()->setPrefix(action!=0);
	      break;
	    default:
	      abort();
	    }
	  }
	} else {
	  // action
	  if (type==EXIT)
	    break; // stop all
	  switch (type) {
	  case DUALSIMPLEX:
	  case PRIMALSIMPLEX:
	    if (goodModels[iModel]) {
	      ClpSolve::SolveType method;
	      ClpSolve::PresolveType presolveType;
	      ClpSimplex * model2 = models+iModel;
	      ClpSolve solveOptions;
	      if (preSolve!=5&&preSolve)
		presolveType=ClpSolve::presolveNumber;
	      else if (preSolve)
		presolveType=ClpSolve::presolveOn;
	      else
		presolveType=ClpSolve::presolveOff;
	      solveOptions.setPresolveType(presolveType,preSolve);
	      if (type==DUALSIMPLEX)
		method=ClpSolve::useDual;
	      else
		method=ClpSolve::usePrimalorSprint;
	      solveOptions.setSolveType(method);
	      if (method==ClpSolve::useDual) {
		// dual
		if (doCrash)
		  solveOptions.setSpecialOption(0,1); // crash
	      } else {
		// primal
		if (doCrash) {
		  solveOptions.setSpecialOption(0,1); // crash
		} else if (doSprint>0) {
		  // sprint overrides idiot
		  solveOptions.setSpecialOption(1,3,doSprint); // sprint
		} else if (doIdiot>0) {
		  solveOptions.setSpecialOption(1,2,doIdiot); // idiot
		} else {
		  if (doIdiot==0) {
		    if (doSprint==0)
		      solveOptions.setSpecialOption(1,4); // all slack
		    else
		      solveOptions.setSpecialOption(1,9); // all slack or sprint
		  } else {
		    if (doSprint==0)
		      solveOptions.setSpecialOption(1,8); // all slack or idiot
		    else
		      solveOptions.setSpecialOption(1,7); // initiative
		  }
		}
	      }
	      model2->initialSolve(solveOptions);
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case TIGHTEN:
	    if (goodModels[iModel]) {
     	      int numberInfeasibilities = models[iModel].tightenPrimalBounds();
	      if (numberInfeasibilities)
		std::cout<<"** Analysis indicates model infeasible"<<std::endl;
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case PLUSMINUS:
	    getNewMatrix=1;
	    break;
	  case NETWORK:
	    getNewMatrix=2;
	    break;
	  case IMPORT:
	    {
	      // get next field
	      field = getString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field=="-") {
		// stdin
		canOpen=true;
		fileName = "-";
	      } else {
		if (field[0]=='/'||field[0]=='~')
		  fileName = field;
		else
		  fileName = directory+field;
		FILE *fp=fopen(fileName.c_str(),"r");
		if (fp) {
		  // can open - lets go for it
		  fclose(fp);
		  canOpen=true;
		} else {
		  std::cout<<"Unable to open file "<<fileName<<std::endl;
		}
	      }
	      if (canOpen) {
		int status =models[iModel].readMps(fileName.c_str(),
						   keepImportNames!=0,
						   allowImportErrors!=0);
		if (!status||(status>0&&allowImportErrors)) {
		  goodModels[iModel]=true;
		  // sets to all slack (not necessary?)
		  models[iModel].createStatus();
		  time2 = CoinCpuTime();
		  totalTime += time2-time1;
		  time1=time2;
		} else {
		  // errors
		  std::cout<<"There were "<<status<<
		    " errors on input"<<std::endl;
		}
	      }
	    }
	    break;
	  case EXPORT:
	    {
	      // get next field
	      field = getString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='~')
		fileName = field;
	      else
		fileName = directory+field;
	      FILE *fp=fopen(fileName.c_str(),"w");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		// If presolve on then save presolved
		bool deleteModel2=false;
		ClpSimplex * model2 = models+iModel;
#ifdef USE_PRESOLVE
		if (preSolve) {
		  ClpPresolve pinfo;
		  model2 = 
		    pinfo.presolvedModel(models[iModel],1.0e-8,
					 false,preSolve);
		  if (model2) {
		    printf("Saving presolved model on %s\n",
			   fileName.c_str());
		    deleteModel2=true;
		  } else {
		    printf("Presolved model looks infeasible - saving original on %s\n",
			   fileName.c_str());
		    deleteModel2=false;
		    model2 = models+iModel;

		  }
		} else {
#endif
		  printf("Saving model on %s\n",
			   fileName.c_str());
#ifdef USE_PRESOLVE
		}
#endif
		// Convert names
		int iRow;
		int numberRows=model2->numberRows();
		int iColumn;
		int numberColumns=model2->numberColumns();

		char ** rowNames = NULL;
		char ** columnNames = NULL;
		if (model2->lengthNames()) {
		  rowNames = new char * [numberRows];
		  for (iRow=0;iRow<numberRows;iRow++) {
		    rowNames[iRow] = 
		      strdup(model2->rowName(iRow).c_str());
#ifdef STRIPBLANKS
		    char * xx = rowNames[iRow];
		    int i;
		    int length = strlen(xx);
		    int n=0;
		    for (i=0;i<length;i++) {
		      if (xx[i]!=' ')
			xx[n++]=xx[i];
		    }
		    xx[n]='\0';
#endif
		  }
		  
		  columnNames = new char * [numberColumns];
		  for (iColumn=0;iColumn<numberColumns;iColumn++) {
		    columnNames[iColumn] = 
		      strdup(model2->columnName(iColumn).c_str());
#ifdef STRIPBLANKS
		    char * xx = columnNames[iColumn];
		    int i;
		    int length = strlen(xx);
		    int n=0;
		    for (i=0;i<length;i++) {
		      if (xx[i]!=' ')
			xx[n++]=xx[i];
		    }
		    xx[n]='\0';
#endif
		  }
		}

		CoinMpsIO writer;
		writer.setMpsData(*model2->matrix(), COIN_DBL_MAX,
				  model2->getColLower(), model2->getColUpper(),
				  model2->getObjCoefficients(),
				  (const char*) 0 /*integrality*/,
				  model2->getRowLower(), model2->getRowUpper(),
				  columnNames, rowNames);
		// Pass in array saying if each variable integer
		writer.copyInIntegerInformation(model2->integerInformation());
		writer.writeMps(fileName.c_str());
		if (rowNames) {
		  for (iRow=0;iRow<numberRows;iRow++) {
		    free(rowNames[iRow]);
		  }
		  delete [] rowNames;
		  for (iColumn=0;iColumn<numberColumns;iColumn++) {
		    free(columnNames[iColumn]);
		  }
		  delete [] columnNames;
		}
		if (deleteModel2)
		  delete model2;
		time2 = CoinCpuTime();
		totalTime += time2-time1;
		time1=time2;
	      }
	    }
	    break;
	  case SAVE:
	    {
	      // get next field
	      field = getString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='~')
		fileName = field;
	      else
		fileName = directory+field;
	      FILE *fp=fopen(fileName.c_str(),"wb");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		int status;
		// If presolve on then save presolved
		bool deleteModel2=false;
		ClpSimplex * model2 = models+iModel;
#ifdef USE_PRESOLVE
		if (preSolve) {
		  ClpPresolve pinfo;
		  model2 = 
		    pinfo.presolvedModel(models[iModel],1.0e-8,
					 false,preSolve);
		  if (model2) {
		    printf("Saving presolved model on %s\n",
			   fileName.c_str());
		    deleteModel2=true;
		  } else {
		    printf("Presolved model looks infeasible - saving original on %s\n",
			   fileName.c_str());
		    deleteModel2=false;
		    model2 = models+iModel;

		  }
		} else {
#endif
		  printf("Saving model on %s\n",
			   fileName.c_str());
#ifdef USE_PRESOLVE
		}
#endif
		status =model2->saveModel(fileName.c_str());
		if (deleteModel2)
		  delete model2;
		if (!status) {
		  goodModels[iModel]=true;
		  time2 = CoinCpuTime();
		  totalTime += time2-time1;
		  time1=time2;
		} else {
		  // errors
		  std::cout<<"There were errors on output"<<std::endl;
		}
	      }
	    }
	    break;
	  case RESTORE:
	    {
	      // get next field
	      field = getString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='~')
		fileName = field;
	      else
		fileName = directory+field;
	      FILE *fp=fopen(fileName.c_str(),"rb");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		int status =models[iModel].restoreModel(fileName.c_str());
		if (!status) {
		  goodModels[iModel]=true;
		  time2 = CoinCpuTime();
		  totalTime += time2-time1;
		  time1=time2;
		} else {
		  // errors
		  std::cout<<"There were errors on input"<<std::endl;
		}
	      }
	    }
	    break;
	  case MAXIMIZE:
	    models[iModel].setOptimizationDirection(-1);
	    break;
	  case MINIMIZE:
	    models[iModel].setOptimizationDirection(1);
	    break;
	  case ALLSLACK:
	    models[iModel].createStatus();
	    break;
	  case DIRECTORY:
	    {
	      std::string name = getString(argc,argv);
	      if (name!="EOL") {
		int length=name.length();
		if (name[length-1]=='/')
		  directory=name;
		else
		  directory = name+"/";
		parameters[iParam].setStringValue(directory);
	      } else {
		parameters[iParam].printString();
	      }
	    }
	    break;
	  case STDIN:
	    read_mode=-1;
	    break;
	  case NETLIB_DUAL:
	  case NETLIB_PRIMAL:
	    {
	      // create fields for unitTest
	      const char * fields[4];
	      int nFields=2;
	      fields[0]="fake main from unitTest";
	      fields[1]="-netlib";
	      if (directory!="./") {
		fields[2]="-netlibDir";
		fields[3]=directory.c_str();
		nFields=4;
	      }
	      if (type==NETLIB_DUAL)
		std::cerr<<"Doing netlib with dual agorithm"<<std::endl;
	      else
		std::cerr<<"Doing netlib with primal agorithm"<<std::endl;
	      mainTest(nFields,fields,(type==NETLIB_DUAL),models[iModel],
		       (preSolve!=0),doIdiot);
	    }
	    break;
	  case UNITTEST:
	    {
	      // create fields for unitTest
	      const char * fields[3];
	      int nFields=1;
	      fields[0]="fake main from unitTest";
	      if (directory!="./") {
		fields[1]="-mpsDir";
		fields[2]=directory.c_str();
		nFields=3;
	      }
	      mainTest(nFields,fields,false,models[iModel],(preSolve!=0),
		       false);
	    }
	    break;
	  case FAKEBOUND:
	    if (goodModels[iModel]) {
	      // get bound
	      double value = getDoubleField(argc,argv,&valid);
	      if (!valid) {
		std::cout<<"Setting "<<parameters[iParam].name()<<
		  " to DEBUG "<<value<<std::endl;
		int iRow;
		int numberRows=models[iModel].numberRows();
		double * rowLower = models[iModel].rowLower();
		double * rowUpper = models[iModel].rowUpper();
		for (iRow=0;iRow<numberRows;iRow++) {
		  // leave free ones for now
		  if (rowLower[iRow]>-1.0e20||rowUpper[iRow]<1.0e20) {
		    rowLower[iRow]=max(rowLower[iRow],-value);
		    rowUpper[iRow]=min(rowUpper[iRow],value);
		  }
		}
		int iColumn;
		int numberColumns=models[iModel].numberColumns();
		double * columnLower = models[iModel].columnLower();
		double * columnUpper = models[iModel].columnUpper();
		for (iColumn=0;iColumn<numberColumns;iColumn++) {
		  // leave free ones for now
		  if (columnLower[iColumn]>-1.0e20||
		      columnUpper[iColumn]<1.0e20) {
		    columnLower[iColumn]=max(columnLower[iColumn],-value);
		    columnUpper[iColumn]=min(columnUpper[iColumn],value);
		  }
		}
	      } else if (valid==1) {
		abort();
	      } else {
		std::cout<<"enter value for "<<parameters[iParam].name()<<
		  std::endl;
	      }
	    }
	    break;
	  case HELP:
	    std::cout<<"Coin LP version "<<CLPVERSION
		     <<", build "<<__DATE__<<std::endl;
	    std::cout<<"Non default values:-"<<std::endl;
	    std::cout<<"Perturbation "<<models[0].perturbation()<<" (default 100)"
		     <<std::endl;
	    printit(
		    "Presolve being done with 5 passes\n\
Dual steepest edge steep/partial on matrix shape and factorization density\n\
Clpnnnn taken out of messages\n\
If Factorization frequency default then done on size of matrix\n\n\
(-)unitTest, (-)netlib or (-)netlibp will do standard tests\n\n\
You can switch to interactive mode at any time so\n\
clp watson.mps -scaling off -primalsimplex\nis the same as\n\
clp watson.mps -\nscaling off\nprimalsimplex"
		    );
  	    break;
	  case SOLUTION:
	    if (goodModels[iModel]) {
	      // get next field
	      field = getString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      FILE *fp=NULL;
	      if (field=="-"||field=="EOL") {
		// stdout
		fp=stdout;
	      } else {
		if (field[0]=='/'||field[0]=='~')
		  fileName = field;
		else
		  fileName = directory+field;
		fp=fopen(fileName.c_str(),"w");
	      }
	      if (fp) {
		// make fancy later on
		int iRow;
		int numberRows=models[iModel].numberRows();
		int lengthName = models[iModel].lengthNames(); // 0 if no names
		// in general I don't want to pass around massive
		// amounts of data but seems simpler here
		std::vector<std::string> rowNames =
		  *(models[iModel].rowNames());
		std::vector<std::string> columnNames =
		  *(models[iModel].columnNames());

		double * dualRowSolution = models[iModel].dualRowSolution();
		double * primalRowSolution = 
		  models[iModel].primalRowSolution();
		double * rowLower = models[iModel].rowLower();
		double * rowUpper = models[iModel].rowUpper();
		double primalTolerance = models[iModel].primalTolerance();
		char format[6];
		sprintf(format,"%%-%ds",max(lengthName,8));
		for (iRow=0;iRow<numberRows;iRow++) {
		  if (primalRowSolution[iRow]>rowUpper[iRow]+primalTolerance||
		      primalRowSolution[iRow]<rowLower[iRow]-primalTolerance)
		    fprintf(fp,"** ");
		  fprintf(fp,"%7d ",iRow);
		  if (lengthName)
		    fprintf(fp,format,rowNames[iRow].c_str());
		  fprintf(fp,"%15.8g        %15.8g\n",primalRowSolution[iRow],
			  dualRowSolution[iRow]);
		}
		int iColumn;
		int numberColumns=models[iModel].numberColumns();
		double * dualColumnSolution = 
		  models[iModel].dualColumnSolution();
		double * primalColumnSolution = 
		  models[iModel].primalColumnSolution();
		double * columnLower = models[iModel].columnLower();
		double * columnUpper = models[iModel].columnUpper();
		for (iColumn=0;iColumn<numberColumns;iColumn++) {
		  if (primalColumnSolution[iColumn]>columnUpper[iColumn]+primalTolerance||
		      primalColumnSolution[iColumn]<columnLower[iColumn]-primalTolerance)
		    fprintf(fp,"** ");
		  fprintf(fp,"%7d ",iColumn);
		  if (lengthName)
		    fprintf(fp,format,columnNames[iColumn].c_str());
		  fprintf(fp,"%15.8g        %15.8g\n",
			  primalColumnSolution[iColumn],
			  dualColumnSolution[iColumn]);
		}
		if (fp!=stdout)
		  fclose(fp);
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	      
	    }
	    break;
	  default:
	    abort();
	  }
	} 
      } else if (!numberMatches) {
	std::cout<<"No match for "<<field<<" - ? for list of commands"
		 <<std::endl;
      } else if (numberMatches==1) {
	if (!numberQuery) {
	  std::cout<<"Short match for "<<field<<" - completion: ";
	  std::cout<<parameters[firstMatch].matchName()<<std::endl;
	} else if (numberQuery) {
	  std::cout<<parameters[firstMatch].matchName()<<" : ";
	  std::cout<<parameters[firstMatch].shortHelp()<<std::endl;
	  if (numberQuery>=2) 
	    parameters[firstMatch].printLongHelp();
	}
      } else {
	if (!numberQuery) 
	  std::cout<<"Multiple matches for "<<field<<" - possible completions:"
		   <<std::endl;
	else
	  std::cout<<"Completions of "<<field<<":"<<std::endl;
	for ( iParam=0; iParam<numberParameters; iParam++ ) {
	  int match = parameters[iParam].matches(field);
	  if (match&&parameters[iParam].displayThis()) {
	    std::cout<<parameters[iParam].matchName();
	    if (numberQuery>=2) 
	      std::cout<<" : "<<parameters[iParam].shortHelp();
	    std::cout<<std::endl;
	  }
	}
      }
    }
    delete [] models;
    delete [] goodModels;
  }
  // By now all memory should be freed
#ifdef DMALLOC
  dmalloc_log_unfreed();
  dmalloc_shutdown();
#endif
  return 0;
}    
