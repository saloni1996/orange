/*
    This file is part of Orange.

    Orange is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Orange is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Orange; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Authors: Janez Demsar, Blaz Zupan, 1996--2002
    Contact: janez.demsar@fri.uni-lj.si
*/


#include <stdlib.h>
#include <iostream>
#include <fstream>

#include "stladdon.hpp"
#include "random.hpp"

#include "vars.hpp"
#include "stringvars.hpp"
#include "domain.hpp"
#include "distvars.hpp"
#include "examplegen.hpp"

#include "filter.ppp"


DEFINE_TOrangeVector_classDescription(PValueFilter, "TValueFilterList")
DEFINE_TOrangeVector_classDescription(PFilter, "TFilterList")

// Sets the negate field (default is false)
TFilter::TFilter(bool anegate, PDomain dom) 
: negate(anegate),
  domain(dom)
{}

void TFilter::reset()
{}

// Sets the maxrand field to RAND_MAX*ap
TFilter_random::TFilter_random(const float ap, bool aneg, PRandomGenerator rgen)
: TFilter(aneg, PDomain()),
  prob(ap),
  randomGenerator(rgen ? rgen : PRandomGenerator(mlnew TRandomGenerator()))
{};

// Chooses an example (returns true) if rand()<maxrand; example is ignored
bool TFilter_random::operator()(const TExample &)
{
  if (!randomGenerator)
    randomGenerator = mlnew TRandomGenerator;

  return (randomGenerator->randfloat()<prob)!=negate;
}



TFilter_hasSpecial::TFilter_hasSpecial(bool aneg, PDomain dom)
  : TFilter(aneg, dom)
  {}


// Chooses an example if it has (no) special values.
bool TFilter_hasSpecial::operator()(const TExample &exam)
{ int i=0, Nv;
  if (domain) {
    TExample example(domain, exam);
    for(Nv = domain->variables->size(); (i<Nv) && !example[i].isSpecial(); i++);
  }
  else
    for(Nv = exam.domain->variables->size(); (i<Nv) && !exam[i].isSpecial(); i++);

  return ((i==Nv)==negate);
}


TFilter_hasClassValue::TFilter_hasClassValue(bool aneg, PDomain dom)
  : TFilter(aneg, dom)
  {}

// Chooses an example if it has (no) special values.
bool TFilter_hasClassValue::operator()(const TExample &exam)
{ return (domain ? TExample(domain, exam).getClass().isSpecial() : exam.getClass().isSpecial()) ==negate; }


// Constructor; sets the value and position
TFilter_sameValue::TFilter_sameValue(const TValue &aval, int apos, bool aneg, PDomain dom)
: TFilter(aneg, dom), 
  position(apos),
  value(aval)
{}


// Chooses an example if position-th attribute's value equals (or not) the specified value
bool TFilter_sameValue::operator()(const TExample &example)
{ signed char equ = (domain ? TExample(domain, example)[position] : example[position]) == value;
  return equ==-1 ? negate : ((equ!=0) != negate);
}


TValueFilter::TValueFilter(const int &pos, const int &accs)
: position(pos),
  acceptSpecial(accs)
{}


TValueFilter_continuous::TValueFilter_continuous()
: TValueFilter(ILLEGAL_INT, -1),
  min(0.0),
  max(0.0),
  outside(false),
  oper(None)
{}

TValueFilter_continuous::TValueFilter_continuous(const int &pos, const float &amin, const float &amax, const bool &outs, const int &accs)
: TValueFilter(pos, accs),
  min(amin),
  max(amax),
  outside(outs),
  oper(None)
{}


TValueFilter_continuous::TValueFilter_continuous(const int &pos, const int &op, const float &amin, const float &amax, const int &accs)
: TValueFilter(pos, accs),
  min(amin),
  max(amax),
  oper(op)
{}


#define EQUAL(x,y)  (fabs(x-y) <= y*1e-10) ? 1 : 0
#define LESS_EQUAL(x,y) (x-y < y*1e-10) ? 1 : 0
#define TO_BOOL(x) (x) ? 1 : 0;

int TValueFilter_continuous::operator()(const TExample &example) const
{ const TValue &val = example[position];
  if (val.isSpecial())
    return acceptSpecial;

  switch (oper) {
    case None:         return TO_BOOL(((val.floatV>=min) && (val.floatV<=max)) != outside);

    case Equal:        return EQUAL(val.floatV, min);
    case NotEqual:     return 1 - EQUAL(val.floatV, min);
    case Less:         return TO_BOOL(val.floatV < min);
    case LessEqual:    return LESS_EQUAL(val.floatV, min);
    case Greater:      return TO_BOOL(min < val.floatV);
    case GreaterEqual: return LESS_EQUAL(min, val.floatV);
    case Between:      return (LESS_EQUAL(min, val.floatV)) * (LESS_EQUAL(val.floatV, max));
    case Outside:      return TO_BOOL((val.floatV < min) || (val.floatV > max));

    default:  return -1;
  }
}


TValueFilter_discrete::TValueFilter_discrete(const int &pos, PValueList bl, const int &accs)
: TValueFilter(pos, accs),
  values(bl ? bl : mlnew TValueList())
{}


TValueFilter_discrete::TValueFilter_discrete(const int &pos, PVariable var, const int &accs)
: TValueFilter(pos, accs),
  values(mlnew TValueList(var))
{}


int TValueFilter_discrete::operator()(const TExample &example) const
{ const TValue &val = example[position];
  if (val.isSpecial())
    return acceptSpecial;

  const_PITERATE(TValueList, vi, values)
    if ((*vi).intV == val.intV)
      return 1;

  return 0;
}


TValueFilter_string::TValueFilter_string()
: TValueFilter(ILLEGAL_INT, -1),
  min(),
  max(),
  oper(None)
{}



TValueFilter_string::TValueFilter_string(const int &pos, const int &op, const string &amin, const string &amax, const int &accs)
: TValueFilter(pos, accs),
  min(amin),
  max(amax),
  oper(op)
{}


int TValueFilter_string::operator()(const TExample &example) const
{ 
  const TValue &val = example[position];
  if (val.isSpecial())
    return acceptSpecial;

  const string &value = val.svalV.AS(TStringValue)->value;
  const string &ref = min;

  switch(oper) {
    case Equal:        return TO_BOOL(value == ref);
    case NotEqual:     return TO_BOOL(value != ref);
    case Less:         return TO_BOOL(value < ref);
    case LessEqual:    return TO_BOOL(value <= ref);
    case Greater:      return TO_BOOL(value > ref);
    case GreaterEqual: return TO_BOOL(value >= ref);
    case Contains:     return TO_BOOL(value.find(ref) != string::npos);
    case NotContains:  return TO_BOOL(value.find(ref) == string::npos);
    case BeginsWith:   return TO_BOOL(!strncmp(value.c_str(), ref.c_str(), ref.size()));
    case Between:       return TO_BOOL((value >= min) && (value <= max));
    case Outside:      return TO_BOOL((value < min) && (value > max));

    case EndsWith:
      { const int vsize = value.size(), rsize = ref.size();
        return TO_BOOL((vsize >= rsize) && !strcmp(value.c_str() + (vsize-rsize), ref.c_str()));
      }

    default:
      return -1;
  }
}



TValueFilter_stringList::TValueFilter_stringList(const int &pos, PStringList bl, const int &accs, const int &op)
: TValueFilter(pos, accs),
  values(bl)
{}

int TValueFilter_stringList::operator()(const TExample &example) const
{ 
  const TValue &val = example[position];
  if (val.isSpecial())
    return acceptSpecial;

  const string &value = val.svalV.AS(TStringValue)->value;

  const_PITERATE(TStringList, vi, values)
    if (value == *vi)
      return 1;
  return 0;
}


#undef DIFFERENT
#undef LESS_EQUAL
#undef TO_BOOL

TFilter_values::TFilter_values(bool anAnd, bool aneg, PDomain dom)
: TFilter(aneg, dom),
  conditions(mlnew TValueFilterList()),
  conjunction(anAnd)
{}


TFilter_values::TFilter_values(PValueFilterList v, bool anAnd, bool aneg, PDomain dom)
: TFilter(aneg, dom),
  conditions(v),
  conjunction(anAnd)
{}


TValueFilterList::iterator TFilter_values::findCondition(PVariable var, const int &varType, int &position)
{
  if (varType && (var->varType != varType))
    raiseError("invalid variable type");

  checkProperty(domain);

  position = domain->getVarNum(var);
  TValueFilterList::iterator condi(conditions->begin()), conde(conditions->end());
  while((condi!=conde) && ((*condi)->position != position))
    condi++;

  return condi;
}

void TFilter_values::updateCondition(PVariable var, const int &varType, PValueFilter filter)
{
  TValueFilterList::iterator condi = findCondition(var, varType, filter->position);
  if (condi==conditions->end())
    conditions->push_back(filter);
  else
    *condi = filter;
}


void TFilter_values::addCondition(PVariable var, const TValue &val)
{
  int position;
  TValueFilterList::iterator condi = findCondition(var, TValue::INTVAR, position);

  TValueFilter_discrete *valueFilter;

  if (condi==conditions->end()) {
    valueFilter = mlnew TValueFilter_discrete(position); // it gets wrapped in the next line
    conditions->push_back(valueFilter);
  }
  else {
    valueFilter = (*condi).AS(TValueFilter_discrete);
    if (!valueFilter)
      raiseError("addCondition(Value) con only be used for setting ValueFilter_discrete");
  }

  if (val.isSpecial())
    valueFilter->acceptSpecial = 1;
  else {
    valueFilter->values->clear();
    valueFilter->values->push_back(val);
  }
}


void TFilter_values::addCondition(PVariable var, PValueList vallist)
{
  int position;
  TValueFilterList::iterator condi = findCondition(var, TValue::INTVAR, position);

  if (condi==conditions->end())
    conditions->push_back(mlnew TValueFilter_discrete(position, vallist));

  else {
    TValueFilter_discrete *valueFilter = (*condi).AS(TValueFilter_discrete);
    if (!valueFilter)
      raiseError("addCondition(Value) con only be used for setting ValueFilter_discrete");
    else
      valueFilter->values = vallist;
  }
}


void TFilter_values::addCondition(PVariable var, const int &oper, const float &min, const float &max)
{
  updateCondition(var, TValue::FLOATVAR, mlnew TValueFilter_continuous(ILLEGAL_INT, oper, min, max));
}


void TFilter_values::addCondition(PVariable var, const int &oper, const string &min, const string &max)
{
  updateCondition(var, STRINGVAR, mlnew TValueFilter_string(ILLEGAL_INT, oper, min, max));
}


void TFilter_values::addCondition(PVariable var, PStringList slist)
{
  updateCondition(var, STRINGVAR, mlnew TValueFilter_stringList(ILLEGAL_INT, slist));
}


void TFilter_values::removeCondition(PVariable var)
{
  int position;
  TValueFilterList::iterator condi = findCondition(var, 0, position);

  if (condi==conditions->end())
    raiseError("there is no condition on value of '%s' in the filter", var->name.c_str());

  conditions->erase(condi);
}
  

bool TFilter_values::operator()(const TExample &exam)
{ checkProperty(domain);
  checkProperty(conditions);

  TExample *example;
  PExample wex;
  if (domain && (domain != exam.domain)) {
    example = mlnew TExample(domain, exam);
    wex = example;
  }
  else
    example = const_cast<TExample *>(&exam);

  PITERATE(TValueFilterList, fi, conditions) {
    const int r = (*fi)->call(*example);
    if ((r==0) && conjunction)
      return negate;
    if ((r==1) && !conjunction)
      return !negate; // if this one is OK, we should return true if negate=false and vice versa
  }

  // If we've come this far; if conjunction==true, all were OK; conjunction==false, none were OK
  return conjunction!=negate;
}



/// Constructor; sets the example
TFilter_sameExample::TFilter_sameExample(PExample anexample, bool aneg)
  : TFilter(aneg, anexample->domain), example(anexample)
  {}


/// Chooses an examples (not) equal to the 'example'
bool TFilter_sameExample::operator()(const TExample &other)
{ return (example->compare(TExample(domain, other))==0)!=negate; }



/// Constructor; sets the example
TFilter_compatibleExample::TFilter_compatibleExample(PExample anexample, bool aneg)
: TFilter(aneg, anexample->domain),
  example(anexample)
{}


/// Chooses an examples (not) compatible with the 'example'
bool TFilter_compatibleExample::operator()(const TExample &other)
{ return example->compatible(TExample(domain, other))!=negate; }




TFilter_conjunction::TFilter_conjunction()
: filters(mlnew TFilterList())
{}


TFilter_conjunction::TFilter_conjunction(PFilterList af)
: filters(af)
{}

bool TFilter_conjunction::operator()(const TExample &ex)
{
  if (filters)
    PITERATE(TFilterList, fi, filters)
      if (!(*fi)->call(ex))
        return false;

  return true;
}


TFilter_disjunction::TFilter_disjunction()
: filters(mlnew TFilterList())
{}


TFilter_disjunction::TFilter_disjunction(PFilterList af)
: filters(af)
{}


bool TFilter_disjunction::operator()(const TExample &ex)
{
  if (filters)
    PITERATE(TFilterList, fi, filters)
      if ((*fi)->call(ex))
        return true;

  return false;
}
